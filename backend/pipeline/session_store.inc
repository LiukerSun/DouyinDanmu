    // Live broadcast sessions: one row per observed broadcast. Douyin assigns a
    // fresh room_id per broadcast, so a changed room identity is the authoritative
    // boundary; metadata polls and end-of-live control messages refine it. The
    // collector session_id tracks capture tasks, never broadcast boundaries.
    static int64_t session_event_time(const json& e) {
        auto at=e.value("timestamp",int64_t(0));
        if(at<=0)at=e.value("received_at_ms",e.value("persisted_at_ms",int64_t(0)));
        if(at<=0)at=now_ms();
        return at;
    }
    static bool session_end_signal(const json& e) {
        if(e.value("type","")!="system")return false;
        if(e.value("control_status",int64_t(0))==3)return true;
        // Journals written before control_status existed carry only the label.
        return e.value("content","")=="直播已结束";
    }
    bool session_tracked_unlocked(const std::string& live) {
        Statement source(db,"SELECT source FROM rooms WHERE live_id=?");source.text(1,live);
        return source.row() && source.str(0)=="douyin";
    }
    int64_t session_activity_unlocked(const std::string& live,const std::string& room,int64_t at,const std::string& source,int64_t started_hint=0) {
        if(!session_tracked_unlocked(live))return 0;
        Statement current(db,"SELECT id,room_id FROM live_sessions WHERE live_id=? AND ended_at_ms IS NULL");current.text(1,live);
        if(!current.row()) {
            // A plausible official start time anchors the session; anything else
            // falls back to the first observed moment of the broadcast.
            const auto started=started_hint>0 && started_hint>=at-12LL*3600*1000 && started_hint<=at+12LL*3600*1000?started_hint:at;
            Statement open(db,"INSERT INTO live_sessions(live_id,room_id,started_at_ms,start_source,created_at) VALUES(?,?,?,?,?)");
            open.text(1,live).text(2,room).num(3,started).text(4,source).num(5,now_ms()).row();
            return sqlite3_last_insert_rowid(db);
        }
        const auto existing=current.str(1);
        if(!room.empty() && existing!=room) {
            Statement close(db,"UPDATE live_sessions SET ended_at_ms=MAX(started_at_ms,?),end_source='room_id_change' WHERE id=?");
            close.num(1,at).num(2,current.number(0)).row();
            Statement open(db,"INSERT INTO live_sessions(live_id,room_id,started_at_ms,start_source,created_at) VALUES(?,?,?,'room_id_change',?)");
            open.text(1,live).text(2,room).num(3,at).num(4,now_ms()).row();
            return sqlite3_last_insert_rowid(db);
        }
        if(existing.empty() && !room.empty()) {
            // Sessions opened on sparse evidence adopt the first concrete room identity.
            Statement adopt(db,"UPDATE live_sessions SET room_id=? WHERE id=?");adopt.text(1,room).num(2,current.number(0)).row();
        }
        return current.number(0);
    }
    void session_end_unlocked(const std::string& live,int64_t at,const std::string& source) {
        if(!session_tracked_unlocked(live))return;
        Statement close(db,"UPDATE live_sessions SET ended_at_ms=MAX(started_at_ms,?),end_source=? WHERE live_id=? AND ended_at_ms IS NULL");
        close.num(1,at).text(2,source).text(3,live).row();
    }
    void session_peak_unlocked(int64_t session,int64_t online) {
        if(session<=0 || online<=0)return;
        Statement peak(db,"UPDATE live_sessions SET peak_online=MAX(peak_online,?) WHERE id=?");peak.num(1,online).num(2,session).row();
    }
    void sessions_initialize() {
        exec("BEGIN IMMEDIATE");
        try {
            Statement migration(db,"SELECT name FROM schema_migrations WHERE name='live_sessions_v1'");
            if(!migration.row()) {
                Statement with_events(db,"SELECT DISTINCT live_id FROM events");
                std::vector<std::string> lives; while(with_events.row())lives.push_back(with_events.str(0));
                for(const auto& live:lives) {
                    Statement scan(db,"SELECT body FROM events WHERE live_id=? ORDER BY id");scan.text(1,live);
                    int64_t last=0;
                    while(scan.row()) {
                        const auto e=json::parse(scan.str(0));
                        const auto at=session_event_time(e);last=at;
                        if(session_end_signal(e)) { session_end_unlocked(live,at,"backfill"); continue; }
                        const auto session=session_activity_unlocked(live,e.value("room_id",""),at,"backfill");
                        if(e.value("type","")=="online_count")session_peak_unlocked(session,e.value("online_count",int64_t(0)));
                    }
                    // History cannot prove a broadcast is still running. Only a room
                    // observed collecting right now keeps its last session open.
                    Statement status(db,"SELECT status FROM rooms WHERE live_id=?");status.text(1,live);
                    const auto state=status.row()?status.str(0):"";
                    if(state!="collecting" && state!="reconnecting" && state!="backpressured")
                        session_end_unlocked(live,last>0?last:now_ms(),"backfill");
                }
                exec("INSERT INTO schema_migrations VALUES('live_sessions_v1')");
            }
            exec("COMMIT");
        } catch(...){exec("ROLLBACK");throw;}
    }
public:
    struct SessionQuery {
        int limit=20,offset=0;
        void validate() const { if(limit<1||limit>100||offset<0||offset>1000000)throw std::invalid_argument("无效的场次分页参数"); }
    };
    static SessionQuery read_session_query(const httplib::Request& req) {
        for(const auto& parameter:req.params)
            if(parameter.first!="limit" && parameter.first!="offset")throw std::invalid_argument("不支持的场次查询参数");
        SessionQuery query;
        if(req.has_param("limit")) {
            const auto value=req.get_param_value("limit");
            if(value.empty() || value.size()>3 || value.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("无效的分页大小");
            query.limit=std::stoi(value);
        }
        if(req.has_param("offset")) {
            const auto value=req.get_param_value("offset");
            if(value.empty() || value.size()>7 || value.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("无效的分页偏移");
            query.offset=std::stoi(value);
        }
        query.validate();return query;
    }
    json sessions(const std::string& live,const SessionQuery& query) {
        query.validate();
        std::lock_guard<std::mutex> guard(mutex);
        Statement total(db,"SELECT COUNT(*) FROM live_sessions WHERE live_id=?");total.text(1,live);total.row();
        const auto count=total.number(0);
        Statement maximum(db,"SELECT COALESCE(MAX(id),0) FROM events");maximum.row();
        const auto seq=maximum.number(0);
        Statement rows(db,"SELECT id,room_id,started_at_ms,COALESCE(ended_at_ms,-1),start_source,COALESCE(end_source,''),peak_online FROM live_sessions WHERE live_id=? ORDER BY started_at_ms DESC,id DESC LIMIT ? OFFSET ?");
        rows.text(1,live).num(2,query.limit).num(3,query.offset);
        json items=json::array();
        while(rows.row()) {
            const auto started=rows.number(2),ended=rows.number(3);
            // Per-session aggregation over the session window, same fact table and
            // exact arithmetic as the rankings. Aggregate only the selected page.
            std::string scoped="WITH scoped AS (SELECT a.kind,a.gift_quantity,a.gift_unit_price,a.like_count FROM analytics_events a JOIN rooms r ON a.live_id=r.live_id WHERE r.source='douyin' AND a.live_id=? AND a.seq<=? AND a.occurred_at_ms>=?";
            if(ended>0)scoped+=" AND a.occurred_at_ms<?";
            Statement stats(db,(scoped+") SELECT COALESCE(SUM(kind='chat'),0),COALESCE(SUM(kind='gift'),0),analytics_sum(gift_quantity,1),analytics_sum(gift_quantity,gift_unit_price),analytics_sum(CASE WHEN gift_unit_price IS NULL THEN gift_quantity ELSE 0 END,1),COALESCE(SUM(like_count),0),COALESCE(SUM(kind='enter'),0) FROM scoped").c_str());
            stats.text(1,live).num(2,seq).num(3,started);if(ended>0)stats.num(4,ended);stats.row();
            const auto unknown=stats.str(4);
            const auto end_source=rows.str(5);
            items.push_back({{"id",rows.number(0)},{"room_id",rows.str(1)},{"started_at_ms",started},
                {"ended_at_ms",ended>0?json(ended):json(nullptr)},{"status",ended>0?"ended":"live"},
                {"start_source",rows.str(4)},{"end_source",end_source.empty()?json(nullptr):json(end_source)},
                {"stats",{{"chat_count",stats.str(0)},{"gift_events",stats.str(1)},{"gift_quantity",stats.str(2)},
                    {"known_gift_value",stats.str(3)},{"unknown_price_quantity",unknown},{"value_complete",unknown=="0"},
                    {"like_count",stats.str(5)},{"enter_count",stats.str(6)},{"peak_online",rows.number(6)}}}});
        }
        const auto next=query.offset+int64_t(items.size());
        return {{"items",items},{"total",std::to_string(count)},{"next_offset",next<count?json(next):json(nullptr)},
            {"meta",{{"as_of_seq",std::to_string(seq)},{"room",live},{"time_basis","event_timestamp"},{"time_range","[started_at_ms,ended_at_ms)"},
                {"coverage","collected_events_only"},{"value_unit","diamond"},{"value_basis","reported_gift_unit_price"},
                {"session_basis","observed_broadcast_boundaries"},{"chat_types",json::array({"chat","emoji"})}}}};
    }
private:
