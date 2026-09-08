    json search_messages(const MessageSearch& query) {
        query.validate();
        std::lock_guard<std::mutex> guard(mutex);
        const std::string source=query.deliveries?"(SELECT live_id,id AS last_seq,body FROM events)":"message_views";
        std::string where=" FROM "+source+" v JOIN rooms r ON r.live_id=v.live_id WHERE r.source='douyin' AND feed_visible(v.body)";
        std::vector<json> values;
        auto text_filter=[&](const std::string& clause,const std::string& value){if(!value.empty()){where+=clause;values.push_back(value);}};
        text_filter(" AND v.live_id=?",query.room);
        // Same classification as the live UI, including pre-upgrade action-1 rows.
        text_filter(" AND (CASE WHEN json_extract(v.body,'$.type') IN ('emoji','episode_chat','audio_chat','screen_chat') THEN 'chat' WHEN json_extract(v.body,'$.type') IN ('system','room_notice') THEN 'notice' WHEN json_extract(v.body,'$.type')='social' THEN CASE WHEN json_extract(v.body,'$.social_action') IN ('follow','share') THEN json_extract(v.body,'$.social_action') WHEN json_extract(v.body,'$.social_action') IS NULL AND CAST(json_extract(v.body,'$.action') AS TEXT)='1' THEN 'follow' ELSE 'social' END ELSE json_extract(v.body,'$.type') END)=?",query.type);
        text_filter(" AND json_extract(v.body,'$.method')=?",query.method);
        text_filter(" AND COALESCE(json_extract(v.body,'$.parse_status'),'legacy')=?",query.parse_status);
        text_filter(" AND CAST(json_extract(v.body,'$.user_id') AS TEXT)=?",query.user);
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
        Statement count(db,("SELECT COUNT(*)"+where).c_str());bind(count);count.row();const auto total=count.number(0);
        std::string page=where;
        if(query.before>0)page+=" AND v.last_seq<?";
        Statement rows(db,("SELECT v.last_seq,v.body"+page+" ORDER BY v.last_seq DESC LIMIT ?").c_str());
        int i=bind(rows);if(query.before>0)rows.num(i++,query.before);rows.num(i,query.limit+1);
        json events=json::array();bool more=false;
        while(rows.row()) {if(events.size()==size_t(query.limit)){more=true;break;}auto event=json::parse(rows.str(1));event["seq"]=std::to_string(rows.number(0));events.push_back(event);}
        return {{"events",events},{"total",total},{"next_before",more?events.back().at("seq"):json(nullptr)}};
    }
