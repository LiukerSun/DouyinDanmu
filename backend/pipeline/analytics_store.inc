    // Append one fact per accepted delivery. Combo high-water marks are evaluated
    // before time filtering, so a 1 -> 2 -> 5 streak contributes 1, 1, 3 units.
    void analytics_project(int64_t seq,const std::string& live,const json& event,const json& legacy_price=nullptr) {
        if(!feed_visible(event))return;
        const auto type=event.value("type","");
        const auto kind=(type=="chat" || type=="emoji")?"chat":type;
        const auto event_id=event.at("event_id").get<std::string>();
        Statement exists(db,"SELECT seq FROM analytics_events WHERE seq=?");exists.num(1,seq);if(exists.row())return;
        int64_t quantity=0;
        if(type=="gift") {
            const auto display=event.value("display_id",event_id);
            const auto observed=event.value("gift_count",int64_t(0));
            int64_t previous=0;
            Statement group(db,"SELECT quantity FROM analytics_gift_groups WHERE live_id=? AND display_id=?");group.text(1,live).text(2,display);
            if(group.row())previous=group.number(0);
            quantity=std::max(int64_t(0),observed-previous);
            Statement update(db,"INSERT INTO analytics_gift_groups VALUES(?,?,?) ON CONFLICT(live_id,display_id) DO UPDATE SET quantity=MAX(quantity,excluded.quantity)");
            update.text(1,live).text(2,display).num(3,std::max(int64_t(0),observed)).row();
        }
        auto timestamp=event.value("timestamp",int64_t(0));
        if(timestamp<=0)timestamp=event.value("received_at_ms",event.value("persisted_at_ms",int64_t(0)));
        auto user=event.value("user_id","");
        if(user.empty() || user.size()>32 || user.find_first_not_of("0123456789")!=std::string::npos || user.find_first_not_of('0')==std::string::npos)user.clear();
        auto level=event.contains("user_level") && event["user_level"].is_number_integer()?event["user_level"].get<int64_t>():-1;
        Statement insert(db,"INSERT INTO analytics_events(seq,event_id,live_id,user_id,user_name,occurred_at_ms,kind,gift_quantity,gift_unit_price,recipient_id,user_level,fans_club) VALUES(?,?,?,?,?,?,?,?,NULLIF(?,-1),?,NULLIF(?,-1),?)");
        insert.num(1,seq).text(2,event_id).text(3,live).text(4,user).text(5,event.value("user_name","")).num(6,timestamp).text(7,kind)
            .num(8,quantity).num(9,analytics_price(event,legacy_price)).text(10,event.value("recipient_id","")).num(11,level)
            .text(12,event.value("fans_club",json(nullptr)).dump()).row();
    }
    void analytics_initialize() {
        if(sqlite3_create_function_v2(db,"analytics_sum",2,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,nullptr,analytics_sum_step,analytics_sum_final,nullptr)!=SQLITE_OK)
            throw std::runtime_error("Cannot register exact statistics aggregate");
        exec("BEGIN IMMEDIATE");
        try {
            exec(R"SQL(
              CREATE TABLE IF NOT EXISTS analytics_events(seq INTEGER PRIMARY KEY,event_id TEXT NOT NULL UNIQUE,live_id TEXT NOT NULL,user_id TEXT NOT NULL,user_name TEXT NOT NULL,occurred_at_ms INTEGER NOT NULL,kind TEXT NOT NULL,gift_quantity INTEGER NOT NULL,gift_unit_price INTEGER,recipient_id TEXT NOT NULL,user_level INTEGER,fans_club TEXT NOT NULL);
              CREATE INDEX IF NOT EXISTS analytics_time ON analytics_events(occurred_at_ms,seq);
              CREATE INDEX IF NOT EXISTS analytics_room_time ON analytics_events(live_id,occurred_at_ms,seq);
              CREATE INDEX IF NOT EXISTS analytics_user_time ON analytics_events(user_id,occurred_at_ms,seq);
              CREATE TABLE IF NOT EXISTS analytics_gift_groups(live_id TEXT NOT NULL,display_id TEXT NOT NULL,quantity INTEGER NOT NULL,PRIMARY KEY(live_id,display_id));
            )SQL");
            Statement migration(db,"SELECT name FROM schema_migrations WHERE name='analytics_v1'");
            if(!migration.row()) {
                Statement events(db,R"SQL(SELECT e.id,e.live_id,e.body,json_extract(d.body,'$.decoded.gift.diamondCount')
                  FROM events e LEFT JOIN event_details d ON d.event_id=e.event_id AND json_extract(e.body,'$.type')='gift' ORDER BY e.id)SQL");
                while(events.row()) {
                    const auto price=events.str(3);
                    analytics_project(events.number(0),events.str(1),json::parse(events.str(2)),price.empty()?json(nullptr):json::parse(price,nullptr,false));
                }
                exec("INSERT INTO schema_migrations VALUES('analytics_v1')");
            }
            exec("COMMIT");
        } catch(...){exec("ROLLBACK");throw;}
    }
    struct AnalyticsScope {
        std::string sql;
        std::vector<json> values;
        int64_t seq=0;
        int bind(Statement& statement) const {
            int i=1;for(const auto& value:values){if(value.is_string())statement.text(i++,value.get<std::string>());else statement.num(i++,value.get<int64_t>());}return i;
        }
    };
    AnalyticsScope analytics_scope(const AnalyticsQuery& query,const std::string& user="") {
        query.validate();
        if(!user.empty()){MessageSearch check;check.user=user;check.validate();}
        Statement maximum(db,"SELECT COALESCE(MAX(id),0) FROM events");maximum.row();
        AnalyticsScope scope;scope.seq=query.scope.as_of_seq<0?maximum.number(0):std::min(query.scope.as_of_seq,maximum.number(0));
        scope.sql="WITH scoped AS (SELECT a.* FROM analytics_events a JOIN rooms r ON a.live_id=r.live_id WHERE r.source='douyin' AND a.seq<=?";
        scope.values.push_back(scope.seq);
        if(!query.scope.room.empty()){scope.sql+=" AND a.live_id=?";scope.values.push_back(query.scope.room);}
        if(!user.empty()){scope.sql+=" AND a.user_id=?";scope.values.push_back(user);}
        if(query.scope.from_ms>=0){scope.sql+=" AND a.occurred_at_ms>=?";scope.values.push_back(query.scope.from_ms);}
        if(query.scope.to_ms>=0){scope.sql+=" AND a.occurred_at_ms<?";scope.values.push_back(query.scope.to_ms);}
        scope.sql+=")";
        return scope;
    }
    static std::string analytics_metrics_sql() {
        return "COUNT(*),COALESCE(SUM(kind='chat'),0),analytics_sum(gift_quantity,1),analytics_sum(gift_quantity,gift_unit_price),analytics_sum(CASE WHEN gift_unit_price IS NULL THEN gift_quantity ELSE 0 END,1),MIN(occurred_at_ms),MAX(occurred_at_ms),COUNT(DISTINCT live_id)";
    }
    static json analytics_metrics(Statement& row,int offset=0) {
        const auto unknown=row.str(offset+4);
        return {{"event_count",row.str(offset)},{"chat_count",row.str(offset+1)},{"gift_quantity",row.str(offset+2)},
            {"known_gift_value",row.str(offset+3)},{"unknown_price_quantity",unknown},{"value_complete",unknown=="0"},
            {"first_seen_at_ms",row.str(offset+5).empty()?json(nullptr):json(row.number(offset+5))},
            {"last_seen_at_ms",row.str(offset+6).empty()?json(nullptr):json(row.number(offset+6))},{"room_count",row.str(offset+7)}};
    }
    static json analytics_meta(const AnalyticsQuery& query,const AnalyticsScope& scope) {
        return {{"as_of_seq",std::to_string(scope.seq)},{"room",query.scope.room.empty()?json(nullptr):json(query.scope.room)},
            {"from_ms",query.scope.from_ms<0?json(nullptr):json(query.scope.from_ms)},{"to_ms",query.scope.to_ms<0?json(nullptr):json(query.scope.to_ms)},
            {"time_basis","event_timestamp"},{"time_range","[from_ms,to_ms)"},{"coverage","collected_events_only"},
            {"value_unit","diamond"},{"value_basis","reported_gift_unit_price"},{"chat_types",json::array({"chat","emoji"})}};
    }
public:
    json analytics_ranking(const AnalyticsQuery& query,bool gifts) {
        std::lock_guard<std::mutex> guard(mutex);const auto scope=analytics_scope(query);
        // Paginate aggregates BEFORE looking up profiles. Joining a window over
        // all events to grouped users lets SQLite repeatedly scan/re-aggregate
        // the entire time range, monopolizing the Store mutex. Only the selected
        // page needs profile lookups, served by the user/time index.
        const std::string grouped=scope.sql+", grouped AS MATERIALIZED (SELECT user_id,"+
            "COALESCE(SUM(kind='chat'),0) AS chats,analytics_sum(gift_quantity,1) AS quantity,analytics_sum(gift_quantity,gift_unit_price) AS value,"+
            "analytics_sum(CASE WHEN gift_unit_price IS NULL THEN gift_quantity ELSE 0 END,1) AS unknown,MAX(occurred_at_ms) AS last_seen FROM scoped WHERE user_id<>'' GROUP BY user_id)";
        const std::string eligible=gifts?"quantity<>'0'":"chats>0";
        Statement count(db,(scope.sql+" SELECT COUNT(DISTINCT user_id) FROM scoped WHERE user_id<>'' AND "+(gifts?"gift_quantity>0":"kind='chat'")).c_str());scope.bind(count);count.row();
        const auto total=count.number(0);
        const std::string order=!gifts?"g.chats DESC,g.user_id ASC":query.sort=="quantity"?"length(g.quantity) DESC,g.quantity DESC,g.user_id ASC":"length(g.value) DESC,g.value DESC,length(g.quantity) DESC,g.quantity DESC,g.user_id ASC";
        Statement rows(db,(grouped+" SELECT g.user_id,g.chats,g.quantity,g.value,g.unknown,g.last_seen FROM grouped g WHERE "+eligible+" ORDER BY "+order+" LIMIT ? OFFSET ?").c_str());
        int i=scope.bind(rows);rows.num(i++,query.scope.limit).num(i,query.offset);
        json items=json::array();
        while(rows.row()) {
            Statement profile(db,(scope.sql+" SELECT user_name,user_level FROM scoped WHERE user_id=? ORDER BY occurred_at_ms DESC,seq DESC LIMIT 1").c_str());
            profile.text(scope.bind(profile),rows.str(0));profile.row();
            items.push_back({{"rank",query.offset+items.size()+1},{"user_id",rows.str(0)},{"user_name",profile.str(0)},
                {"user_level",profile.str(1).empty()?json(nullptr):json(profile.number(1))},{"chat_count",rows.str(1)},{"gift_quantity",rows.str(2)},
                {"known_gift_value",rows.str(3)},{"unknown_price_quantity",rows.str(4)},{"value_complete",rows.str(4)=="0"},{"last_seen_at_ms",rows.number(5)}});
        }
        Statement missing(db,(scope.sql+" SELECT COUNT(*) FROM scoped WHERE user_id='' AND "+(gifts?"gift_quantity>0":"kind='chat'")).c_str());scope.bind(missing);missing.row();
        auto meta=analytics_meta(query,scope);meta["unattributed_event_count"]=missing.str(0);
        const auto next=query.offset+int64_t(items.size());
        return {{"items",items},{"total",std::to_string(total)},{"next_offset",next<total?json(next):json(nullptr)},{"sort",gifts?query.sort:"chat_count"},{"meta",meta}};
    }
    json analytics_user_summary(const AnalyticsQuery& query,const std::string& user) {
        if(user.empty())throw std::invalid_argument("用户 ID 不能为空");
        std::lock_guard<std::mutex> guard(mutex);const auto scope=analytics_scope(query,user);
        Statement row(db,(scope.sql+" SELECT "+analytics_metrics_sql()+" FROM scoped").c_str());scope.bind(row);row.row();
        auto result=analytics_metrics(row);result["user_id"]=user;result["meta"]=analytics_meta(query,scope);
        Statement profile(db,(scope.sql+" SELECT user_name,user_level FROM scoped ORDER BY occurred_at_ms DESC,seq DESC LIMIT 1").c_str());scope.bind(profile);
        result["user_name"]="";result["user_level"]=nullptr;
        if(profile.row()){result["user_name"]=profile.str(0);if(!profile.str(1).empty())result["user_level"]=profile.number(1);}
        return result;
    }
    json analytics_user_rooms(const AnalyticsQuery& query,const std::string& user) {
        if(user.empty())throw std::invalid_argument("用户 ID 不能为空");
        std::lock_guard<std::mutex> guard(mutex);const auto scope=analytics_scope(query,user);
        Statement count(db,(scope.sql+" SELECT COUNT(DISTINCT live_id) FROM scoped").c_str());scope.bind(count);count.row();const auto total=count.number(0);
        Statement rows(db,(scope.sql+" SELECT live_id,"+analytics_metrics_sql()+" FROM scoped GROUP BY live_id ORDER BY MAX(occurred_at_ms) DESC,live_id ASC LIMIT ? OFFSET ?").c_str());
        int i=scope.bind(rows);rows.num(i++,query.scope.limit).num(i,query.offset);json items=json::array();
        while(rows.row()) {
            auto item=analytics_metrics(rows,1);item["live_id"]=rows.str(0);
            Statement profile(db,(scope.sql+" SELECT user_name,fans_club FROM scoped WHERE live_id=? ORDER BY occurred_at_ms DESC,seq DESC LIMIT 1").c_str());int j=scope.bind(profile);profile.text(j,rows.str(0));
            if(profile.row()){item["user_name"]=profile.str(0);item["fans_club"]=json::parse(profile.str(1));}
            items.push_back(std::move(item));
        }
        const auto next=query.offset+int64_t(items.size());
        return {{"user_id",user},{"items",items},{"total",std::to_string(total)},{"next_offset",next<total?json(next):json(nullptr)},{"meta",analytics_meta(query,scope)}};
    }
private:
