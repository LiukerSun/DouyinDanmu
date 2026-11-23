    json search_messages(const MessageSearch& query) {
        query.validate();
        if(query.as_of_seq>=0 && !query.deliveries)throw std::invalid_argument("统计快照明细请使用 view=events");
        // Use a WAL read snapshot for the count and page. Slow filters must not
        // take the connection/mutex used by room polling and ingestion.
        sqlite3* raw=nullptr;
        int rc=sqlite3_open_v2(database_path.c_str(),&raw,SQLITE_OPEN_READONLY,nullptr);
        std::unique_ptr<sqlite3,decltype(&sqlite3_close)> reader(raw,sqlite3_close);
        if(rc!=SQLITE_OK)throw std::runtime_error("Cannot open archive reader");
        sqlite3_busy_timeout(raw,3000);
        register_body_codecs(raw);
        register_feed_visibility(raw);
        // Closing the read-only connection ends the transaction on every exit.
        Statement snapshot(raw,"BEGIN");
        snapshot.row();
        // Keep stored bytes for the visibility check (which handles both TEXT
        // and gzip). Body filters and the response use decoded JSON.
        // Both source indexes cover the room and sequence. Known visible rows
        // need no body read for an unfiltered count, and a replaced/deleted
        // source row can never contribute a phantom to the total.
        const std::string source=query.deliveries?
            "(SELECT live_id,id AS last_seq,body AS stored_body,decode_body(body) AS body FROM events INDEXED BY events_room_seq)":
            "(SELECT live_id,last_seq,body AS stored_body,body FROM message_views INDEXED BY message_views_room_seq)";
        std::string from=" FROM message_archive k JOIN rooms r ON r.live_id=k.live_id CROSS JOIN "+source+
            " v ON v.last_seq=k.seq AND v.live_id=k.live_id";
        std::string where=" WHERE k.merged="+std::string(query.deliveries?"0":"1")+
            " AND k.visible<>0 AND (k.visible=1 OR feed_visible(v.stored_body)) AND r.source='douyin'";
        std::vector<json> values;
        auto text_filter=[&](const std::string& clause,const std::string& value){if(!value.empty()){where+=clause;values.push_back(value);}};
        text_filter(" AND k.live_id=?",query.room);
        if(query.as_of_seq>=0){where+=" AND k.seq<=?";values.push_back(query.as_of_seq);}
        // Same classification as the live UI, including pre-upgrade action-1 rows.
        text_filter(" AND (CASE WHEN json_extract(v.body,'$.type') IN ('emoji','episode_chat','audio_chat','screen_chat') THEN 'chat' WHEN json_extract(v.body,'$.type') IN ('system','room_notice') THEN 'notice' WHEN json_extract(v.body,'$.type')='social' THEN CASE WHEN json_extract(v.body,'$.social_action') IN ('follow','share') THEN json_extract(v.body,'$.social_action') WHEN json_extract(v.body,'$.social_action') IS NULL AND CAST(json_extract(v.body,'$.action') AS TEXT)='1' THEN 'follow' ELSE 'social' END ELSE json_extract(v.body,'$.type') END)=?",query.type);
        text_filter(" AND json_extract(v.body,'$.method')=?",query.method);
        text_filter(" AND COALESCE(json_extract(v.body,'$.parse_status'),'legacy')=?",query.parse_status);
        text_filter(" AND CAST(json_extract(v.body,'$.user_id') AS TEXT)=?",query.user);
        const std::string event_time="COALESCE(NULLIF(json_extract(v.body,'$.timestamp'),0),json_extract(v.body,'$.received_at_ms'),json_extract(v.body,'$.persisted_at_ms'),0)";
        if(query.from_ms>=0){where+=" AND "+event_time+">=?";values.push_back(query.from_ms);}
        if(query.to_ms>=0){where+=" AND "+event_time+"<?";values.push_back(query.to_ms);}
        text_filter(" AND instr(lower(COALESCE(json_extract(v.body,'$.content'),'') || ' ' || CASE WHEN json_extract(v.body,'$.type')='social' THEN CASE WHEN json_extract(v.body,'$.social_action')='follow' OR (json_extract(v.body,'$.social_action') IS NULL AND CAST(json_extract(v.body,'$.action') AS TEXT)='1') THEN '关注了主播' WHEN json_extract(v.body,'$.social_action')='share' THEN '分享了直播间' ELSE '其他互动' END ELSE '' END || ' ' || COALESCE(json_extract(v.body,'$.user_name'),'') || ' ' || COALESCE(json_extract(v.body,'$.user_id'),'') || ' ' || COALESCE(json_extract(v.body,'$.method'),'')),lower(?))>0",query.keyword);
        auto level_filter=[&](const std::string& field,int64_t low,int64_t high,bool unknown){
            const auto value="json_extract(v.body,'$."+field+"')";
            if(unknown)where+=" AND ("+value+" IS NULL OR "+value+"<0)";
            if(low>=0){where+=" AND "+value+">=?";values.push_back(low);}
            if(high>=0){where+=" AND "+value+"<=? AND "+value+">=0";values.push_back(high);}
        };
        level_filter("user_level",query.wealth_min,query.wealth_max,query.wealth_unknown);
        level_filter("fans_club.level",query.fans_min,query.fans_max,query.fans_unknown);
        auto bind=[&](Statement& s){int i=1;for(const auto& v:values){if(v.is_string())s.text(i++,v.get<std::string>());else s.num(i++,v.get<int64_t>());}return i;};
        Statement count(raw,("SELECT COUNT(*)"+from+where).c_str());bind(count);count.row();const auto total=count.number(0);
        if(query.deliveries)from+=" LEFT JOIN analytics_events a ON a.seq=k.seq";
        std::string page=where;
        if(query.before>0)page+=" AND k.seq<?";
        Statement rows(raw,("SELECT k.seq,v.body"+std::string(query.deliveries?",a.gift_quantity,a.gift_unit_price":"")+from+page+" ORDER BY k.seq DESC LIMIT ?").c_str());
        int i=bind(rows);if(query.before>0)rows.num(i++,query.before);rows.num(i,query.limit+1);
        json events=json::array();bool more=false;
        while(rows.row()) {
            if(events.size()==size_t(query.limit)){more=true;break;}
            auto event=json::parse(rows.str(1));event["seq"]=std::to_string(rows.number(0));
            if(query.deliveries && event.value("type","")=="gift" && !rows.str(2).empty()) {
                const bool known=!rows.str(3).empty();
                boost::multiprecision::cpp_int value=rows.number(2);value*=rows.number(3);
                event["gift_statistics"]={{"quantity_delta",rows.str(2)},{"unit_price",known?json(rows.number(3)):json(nullptr)},{"value_delta",known?json(value.str()):json(nullptr)}};
            }
            events.push_back(event);
        }
        return {{"events",events},{"total",total},{"next_before",more?events.back().at("seq"):json(nullptr)}};
    }
