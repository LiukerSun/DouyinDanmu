// Explicit offline maintenance only. Restore proven delivery facts without
// replaying receipts, changing grouping, or replacing repaired user profiles.
static json repair_gift_facts(bool apply) {
    json report={{"apply",apply},{"applied",false},{"backup",nullptr},{"failed",0},
        {"scanned_groups",0},{"changed_groups",0},{"changed_events",0},{"changed_analytics",0},
        {"skipped_groups",0},{"skipped_events",0},{"skip_reasons",json::object()}};
    sqlite3* raw=nullptr;
    const auto database_path=std::filesystem::path(env("DATABASE_PATH","/data/pipeline.db"));
    if(sqlite3_open_v2(database_path.string().c_str(),&raw,apply?SQLITE_OPEN_READWRITE:SQLITE_OPEN_READONLY,nullptr)!=SQLITE_OK) {
        if(raw)sqlite3_close(raw);report["failed"]=1;report["error"]="Cannot open gift fact database";return report;
    }
    std::unique_ptr<sqlite3,decltype(&sqlite3_close)> database(raw,sqlite3_close);
    sqlite3_busy_timeout(raw,5000);
    auto exec=[&](const char* sql){if(sqlite3_exec(raw,sql,nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(raw));};
    auto has_table=[&](const char* name){Statement q(raw,"SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");q.text(1,name);return q.row();};
    auto data_version=[&](){Statement q(raw,"PRAGMA data_version");q.row();return q.number(0);};
    bool transaction=false;
    try {
        if(!has_table("events") || !has_table("event_details") || !has_table("analytics_events") || !has_table("analytics_gift_groups"))
            throw std::runtime_error("Gift fact repair requires the event details and analytics schema");
        if(apply) {
            const auto version=data_version();
            std::filesystem::path backup_directory;
            for(int attempt=0;attempt<100;++attempt) {
                auto candidate=database_path.parent_path()/(database_path.filename().string()+".before-gift-facts-"+std::to_string(now_ms())+"-"+std::to_string(attempt));
                if(std::filesystem::create_directory(candidate)){backup_directory=candidate;break;}
            }
            if(backup_directory.empty())throw std::runtime_error("Cannot create an owned gift fact backup directory");
            const auto backup=backup_directory/database_path.filename();
            {Statement q(raw,"VACUUM INTO ?");q.text(1,backup.string()).row();}
            report["backup"]=backup.string();
            exec("BEGIN IMMEDIATE");transaction=true;
            if(data_version()!=version)throw std::runtime_error("Database changed during backup; stop collection before repairing gift facts");
        } else {exec("BEGIN");transaction=true;}
        const bool presentations=has_table("event_presentations");
        struct Row {int64_t seq;std::string live,id,original;json body,detail,overlay;};
        std::map<std::pair<std::string,std::string>,std::vector<Row>> groups;
        {
            const std::string sql="SELECT e.id,e.live_id,e.event_id,e.body,d.body,"+std::string(presentations?"p.fields":"NULL")+
                " FROM events e LEFT JOIN event_details d ON d.event_id=e.event_id "+
                (presentations?"LEFT JOIN event_presentations p ON p.event_id=e.event_id ":"")+
                "WHERE json_extract(e.body,'$.type')='gift' ORDER BY e.id";
            Statement q(raw,sql.c_str());
            while(q.row()) {
                auto body=json::parse(q.str(3));
                groups[{q.str(1),body.value("display_id",q.str(2))}].push_back({q.number(0),q.str(1),q.str(2),q.str(3),std::move(body),
                    q.str(4).empty()?json(nullptr):json::parse(q.str(4)),q.str(5).empty()?json::object():json::parse(q.str(5))});
            }
        }
        struct Change {Row row;json repaired,overlay;int64_t quantity,price,timestamp;bool event_changed,analytics_changed;};
        std::vector<Change> changes;
        for(const auto& [key,rows]:groups) {
            report["scanned_groups"]=report["scanned_groups"].get<int>()+1;
            std::vector<Change> planned;
            std::string reason;
            try {
                int64_t high_water=0;std::string identity;
                for(const auto& row:rows) {
                    const auto& old=row.body;const auto& detail=row.detail;
                    if(!detail.is_object() || !detail.contains("payload_base64"))throw std::runtime_error("missing_payload");
                    if(old.value("method","")!="WebcastGiftMessage" || detail.value("method","")!="WebcastGiftMessage" ||
                       old.value("event_id","")!=row.id || old.value("live_id","")!=row.live || old.value("room_id","").empty())
                        throw std::runtime_error("identity_mismatch");
                    const auto payload=decode_saved_base64(detail.at("payload_base64").get<std::string>());
                    if(old.contains("payload_bytes") && old.at("payload_bytes")!=payload.size())throw std::runtime_error("payload_size_mismatch");
                    const auto message=decode<GiftMessage>(payload);
                    const auto sender=precise_user_id(message.user());
                    if(sender.empty() || sender!=old.value("user_id",""))throw std::runtime_error("identity_mismatch");
                    if(!detail.contains("msg_id") || !detail.contains("wrds_version") || !detail.contains("frame_id"))throw std::runtime_error("missing_identity_evidence");
                    const auto msg_id=detail.at("msg_id").get<std::string>();
                    if(msg_id!="0") {
                        if(row.id!=old.at("room_id").get<std::string>()+":WebcastGiftMessage:"+msg_id+":"+detail.at("wrds_version").get<std::string>())
                            throw std::runtime_error("identity_mismatch");
                    } else {
                        const auto prefix=detail.at("frame_id").get<std::string>()+":";
                        const auto index=row.id.substr(std::min(prefix.size(),row.id.size()));
                        if(row.id.rfind(prefix,0)!=0 || index.empty() || index.find_first_not_of("0123456789")!=std::string::npos)
                            throw std::runtime_error("identity_mismatch");
                    }
                    json decoded={{"user_id",sender}};gift_fields(decoded,message,old.at("room_id").get<std::string>());
                    if(decoded.at("gift_id")!=old.value("gift_id","") || decoded.at("group_id")!=old.value("group_id","") ||
                       (!decoded.at("recipient_id").get<std::string>().empty() && decoded.at("recipient_id")!=old.value("recipient_id","")))
                        throw std::runtime_error("identity_mismatch");
                    const auto this_identity=json::array({old.at("room_id"),sender,decoded.at("gift_id"),decoded.at("group_id"),old.value("recipient_id","")}).dump();
                    if(!identity.empty() && identity!=this_identity)throw std::runtime_error("group_identity_mismatch");
                    identity=this_identity;
                    auto timestamp=message.common().create_time();
                    if(timestamp>0){if(timestamp<100000000000LL)timestamp*=1000;}
                    else {
                        timestamp=old.value("received_at_ms",int64_t(0));
                        if(timestamp<=0)timestamp=old.value("persisted_at_ms",int64_t(0));
                        if(timestamp<=0)throw std::runtime_error("missing_timestamp_evidence");
                    }
                    auto repaired=old;repaired["gift_count"]=decoded.at("gift_count");repaired["gift_unit_price"]=decoded.at("gift_unit_price");repaired["timestamp"]=timestamp;
                    auto overlay=row.overlay;if(!overlay.is_object())throw std::runtime_error("invalid_presentation");
                    for(const auto* field:{"gift_count","gift_unit_price","timestamp"})
                        if(old.contains(field) && old.at(field)!=repaired.at(field) && !overlay.contains(field))overlay[field]=old.at(field);
                    const auto count=repaired.at("gift_count").get<int64_t>();
                    const auto quantity=std::max(int64_t(0),count-high_water);high_water=std::max(high_water,count);
                    const auto price=analytics_price(repaired);
                    Statement fact(raw,"SELECT event_id,live_id,user_id,kind,gift_quantity,gift_unit_price,occurred_at_ms FROM analytics_events WHERE seq=?");fact.num(1,row.seq);
                    if(!fact.row())throw std::runtime_error("missing_analytics");
                    if(fact.str(0)!=row.id || fact.str(1)!=row.live || fact.str(2)!=sender || fact.str(3)!="gift")throw std::runtime_error("analytics_identity_mismatch");
                    const bool analytics_changed=fact.number(4)!=quantity || (fact.str(5).empty()?-1:fact.number(5))!=price || fact.number(6)!=timestamp;
                    planned.push_back({row,std::move(repaired),std::move(overlay),quantity,price,timestamp,false,analytics_changed});
                    planned.back().event_changed=planned.back().repaired!=old;
                }
                Statement group(raw,"SELECT quantity FROM analytics_gift_groups WHERE live_id=? AND display_id=?");group.text(1,key.first).text(2,key.second);
                if(!group.row() || group.number(0)!=high_water)throw std::runtime_error("group_total_mismatch");
            } catch(const std::exception& error) {reason=error.what();}
            if(!reason.empty()) {
                report["skipped_groups"]=report["skipped_groups"].get<int>()+1;report["skipped_events"]=report["skipped_events"].get<int>()+rows.size();
                // Fixed categories keep reports useful without disclosing payloads.
                if(reason.find(' ')!=std::string::npos || reason.rfind("[json.",0)==0)reason="invalid_payload_or_record";
                report["skip_reasons"][reason]=report["skip_reasons"].value(reason,0)+1;continue;
            }
            bool changed=false;
            for(auto& change:planned)if(change.event_changed || change.analytics_changed) {
                changed=true;
                if(change.event_changed)report["changed_events"]=report["changed_events"].get<int>()+1;
                if(change.analytics_changed)report["changed_analytics"]=report["changed_analytics"].get<int>()+1;
                changes.push_back(std::move(change));
            }
            if(changed)report["changed_groups"]=report["changed_groups"].get<int>()+1;
        }
        if(apply) {
            if(!presentations)exec("CREATE TABLE event_presentations(event_id TEXT PRIMARY KEY,fields TEXT NOT NULL)");
            for(const auto& change:changes) {
                if(change.event_changed) {
                    Statement event(raw,"UPDATE events SET body=? WHERE id=? AND event_id=? AND body=?");event.text(1,change.repaired.dump()).num(2,change.row.seq).text(3,change.row.id).text(4,change.row.original).row();
                    if(sqlite3_changes(raw)!=1)throw std::runtime_error("Gift event changed during repair");
                    if(change.overlay!=change.row.overlay) {
                        Statement overlay(raw,"INSERT INTO event_presentations(event_id,fields) VALUES(?,?) ON CONFLICT(event_id) DO UPDATE SET fields=excluded.fields");overlay.text(1,change.row.id).text(2,change.overlay.dump()).row();
                    }
                }
                if(change.analytics_changed) {
                    Statement fact(raw,"UPDATE analytics_events SET gift_quantity=?,gift_unit_price=NULLIF(?,-1),occurred_at_ms=? WHERE seq=? AND event_id=? AND live_id=?");
                    fact.num(1,change.quantity).num(2,change.price).num(3,change.timestamp).num(4,change.row.seq).text(5,change.row.id).text(6,change.row.live).row();
                    if(sqlite3_changes(raw)!=1)throw std::runtime_error("Gift analytics changed during repair");
                }
            }
        }
        exec("COMMIT");transaction=false;report["applied"]=apply;
    } catch(const std::exception& error) {
        if(transaction)sqlite3_exec(raw,"ROLLBACK",nullptr,nullptr,nullptr);
        report["failed"]=1;report["error"]=error.what();
    }
    return report;
}
