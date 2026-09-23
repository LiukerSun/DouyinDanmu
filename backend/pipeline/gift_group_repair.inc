#include <set>

// Explicit, offline repair of split delivery groups. It never rewrites the
// saved quantity, price, timestamp, payload, sequence or user profile facts.
static json repair_gift_groups(bool apply) {
    json report={{"apply",apply},{"applied",false},{"backup",nullptr},{"failed",0},
        {"scanned_groups",0},{"changed_groups",0},{"changed_events",0},{"changed_analytics",0},
        {"removed_display_rows",0},{"gift_quantity_delta",0},{"skipped_groups",0},{"skipped_events",0},
        {"unchanged_groups",0},{"skip_reasons",json::object()}};
    sqlite3* raw=nullptr;
    const auto path=std::filesystem::path(env("DATABASE_PATH","/data/pipeline.db"));
    if(sqlite3_open_v2(path.string().c_str(),&raw,apply?SQLITE_OPEN_READWRITE:SQLITE_OPEN_READONLY,nullptr)!=SQLITE_OK) {
        if(raw)sqlite3_close(raw);report["failed"]=1;report["error"]="Cannot open gift group database";return report;
    }
    std::unique_ptr<sqlite3,decltype(&sqlite3_close)> database(raw,sqlite3_close);
    sqlite3_busy_timeout(raw,5000);
    auto exec=[&](const char* sql){if(sqlite3_exec(raw,sql,nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(raw));};
    auto table=[&](const char* name){Statement q(raw,"SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");q.text(1,name);return q.row();};
    auto version=[&](){Statement q(raw,"PRAGMA data_version");q.row();return q.number(0);};
    auto sum=[](int64_t a,int64_t b){if(b<0 || a>std::numeric_limits<int64_t>::max()-b)throw std::runtime_error("quantity_overflow");return a+b;};
    auto skip=[&](size_t count,std::string reason) {
        if(reason.find(' ')!=std::string::npos || reason.rfind("[json.",0)==0)reason="invalid_payload_or_record";
        report["skipped_groups"]=report["skipped_groups"].get<int64_t>()+1;
        report["skipped_events"]=report["skipped_events"].get<int64_t>()+count;
        report["skip_reasons"][reason]=report["skip_reasons"].value(reason,int64_t(0))+1;
    };
    bool transaction=false;
    try {
        for(const char* name:{"events","event_details","analytics_events","analytics_gift_groups","message_views","gift_totals","rooms","receipts"})
            if(!table(name))throw std::runtime_error("Gift group repair requires the complete event and analytics schema");
        if(apply) {
            const auto before=version();std::filesystem::path directory;
            for(int attempt=0;attempt<100;++attempt) {
                auto candidate=path.parent_path()/(path.filename().string()+".before-gift-groups-"+std::to_string(now_ms())+"-"+std::to_string(attempt));
                if(std::filesystem::create_directory(candidate)){directory=candidate;break;}
            }
            if(directory.empty())throw std::runtime_error("Cannot create an owned gift group backup directory");
            const auto backup=directory/path.filename();
            {Statement q(raw,"VACUUM INTO ?");q.text(1,backup.string()).row();}
            report["backup"]=backup.string();exec("BEGIN IMMEDIATE");transaction=true;
            if(version()!=before)throw std::runtime_error("Database changed during backup; stop collection before repairing gift groups");
        } else {exec("BEGIN");transaction=true;}
        const bool presentations=table("event_presentations");
        struct Row {int64_t seq;std::string live,id,original;json body,detail,overlay,decoded;std::string recipient,error;};
        std::map<std::string,std::vector<Row>> groups;
        std::set<std::string> conflicting_groups;
        auto identity=[](const std::string& live,const json& body) {
            return json::array({live,body.value("room_id",""),body.value("user_id",""),body.value("gift_id",""),body.value("group_id","")}).dump();
        };
        {
            const auto sql="SELECT e.id,e.live_id,e.event_id,e.body,d.body,"+std::string(presentations?"p.fields":"NULL")+
                " FROM events e LEFT JOIN event_details d ON d.event_id=e.event_id "+
                (presentations?"LEFT JOIN event_presentations p ON p.event_id=e.event_id ":"")+
                "WHERE json_extract(e.body,'$.type')='gift' ORDER BY e.id";
            Statement q(raw,sql.c_str());
            while(q.row()) {
                Row row{q.number(0),q.str(1),q.str(2),q.str(3),json::parse(q.str(3)),nullptr,json::object(),nullptr,"",""};
                const auto key=identity(row.live,row.body);
                try {
                    row.detail=q.str(4).empty()?json(nullptr):json::parse(q.str(4));
                    row.overlay=q.str(5).empty()?json::object():json::parse(q.str(5));
                    const auto& old=row.body;const auto& detail=row.detail;
                    if(old.value("room_id","").empty() || old.value("user_id","").empty() || old.value("gift_id","0")=="0" ||
                       old.value("gift_id","").empty() || old.value("group_id","0")=="0" || old.value("group_id","").empty())
                        throw std::runtime_error("incomplete_group_identity");
                    if(!detail.is_object() || !detail.contains("payload_base64"))throw std::runtime_error("missing_payload");
                    if(!row.overlay.is_object())throw std::runtime_error("invalid_presentation");
                    if(old.value("method","")!="WebcastGiftMessage" || detail.value("method","")!="WebcastGiftMessage" ||
                       old.value("event_id","")!=row.id || old.value("live_id","")!=row.live || row.live.empty())
                        throw std::runtime_error("identity_mismatch");
                    const auto payload=decode_saved_base64(detail.at("payload_base64").get<std::string>());
                    if(!old.contains("payload_bytes") || old.at("payload_bytes")!=payload.size())throw std::runtime_error("payload_size_mismatch");
                    const auto message=decode<GiftMessage>(payload);const auto sender=precise_user_id(message.user());
                    row.decoded={{"user_id",sender},{"room_id",old.at("room_id")}};
                    gift_fields(row.decoded,message,old.at("room_id").get<std::string>());
                    const auto decoded_key=identity(row.live,row.decoded);
                    if(key!=decoded_key) {
                        conflicting_groups.insert(key);conflicting_groups.insert(decoded_key);
                        throw std::runtime_error("identity_mismatch");
                    }
                    if(sender.empty() || !row.decoded.contains("gift_group_key"))throw std::runtime_error("incomplete_group_identity");
                    if(message.common().room_id()!=0 && std::to_string(message.common().room_id())!=old.at("room_id"))throw std::runtime_error("room_identity_mismatch");
                    if(!detail.contains("msg_id") || !detail.contains("wrds_version") || detail.value("frame_id","").empty())
                        throw std::runtime_error("missing_identity_evidence");
                    const auto msg=detail.at("msg_id").get<std::string>();
                    if(msg!="0") {
                        if(row.id!=old.at("room_id").get<std::string>()+":WebcastGiftMessage:"+msg+":"+detail.at("wrds_version").get<std::string>())
                            throw std::runtime_error("identity_mismatch");
                    } else {
                        const auto prefix=detail.at("frame_id").get<std::string>()+":";
                        const auto index=row.id.substr(std::min(prefix.size(),row.id.size()));
                        if(row.id.rfind(prefix,0)!=0 || index.empty() || index.find_first_not_of("0123456789")!=std::string::npos)
                            throw std::runtime_error("identity_mismatch");
                    }
                    {Statement receipt(raw,"SELECT 1 FROM receipts WHERE frame_id=?");receipt.text(1,detail.at("frame_id"));if(!receipt.row())throw std::runtime_error("missing_receipt");}
                    row.recipient=row.decoded.value("recipient_id","");
                    if(!row.recipient.empty() && row.recipient!=old.value("recipient_id",""))throw std::runtime_error("recipient_identity_mismatch");
                    // Repairing a prior fact overwrite belongs to --repair-gift-facts.
                    if(old.at("gift_count")!=row.decoded.at("gift_count"))throw std::runtime_error("stored_count_mismatch");
                } catch(const std::exception& error){row.error=error.what();}
                groups[key].push_back(std::move(row));
            }
        }
        struct Change {Row* row;json body,overlay;int64_t quantity;std::string recipient;bool event_changed,analytics_changed;};
        struct Plan {std::string live,display;std::set<std::string> old_ids;std::vector<Change> changes;json view;int64_t high_water=0,old_quantity=0;};
        std::vector<Plan> plans;
        for(auto& [key,rows]:groups) {
            report["scanned_groups"]=report["scanned_groups"].get<int64_t>()+1;
            try {
                if(conflicting_groups.count(key))throw std::runtime_error("identity_mismatch");
                std::set<std::string> recipients;bool missing=false;
                for(const auto& row:rows) {
                    if(!row.error.empty())throw std::runtime_error(row.error);
                    if(row.recipient.empty())missing=true;else recipients.insert(row.recipient);
                }
                if(missing && recipients.size()>1)throw std::runtime_error("ambiguous_recipient");
                std::map<std::string,std::vector<Row*>> partitions;
                for(auto& row:rows) {
                    const auto recipient=row.recipient.empty() && recipients.size()==1?*recipients.begin():row.recipient;
                    if(!row.body.value("recipient_id","").empty() && row.body.at("recipient_id")!=recipient)
                        throw std::runtime_error("unproven_recipient");
                    partitions[recipient].push_back(&row);
                }
                // Validate and plan every recipient before accepting any partition.
                std::vector<Plan> candidate_plans;
                for(auto& [recipient,members]:partitions) {
                    Plan plan;plan.live=members.front()->live;
                    std::map<std::string,std::vector<Row*>> old_groups;
                    for(auto* row:members)old_groups[row->body.value("display_id",row->id)].push_back(row);
                    if(old_groups.size()<2)continue;
                    const auto& first=members.front()->body;
                    plan.display=recipient.empty()?members.front()->body.value("display_id",members.front()->id):
                        "gift:"+digest(json::array({first.at("room_id"),first.at("user_id"),recipient,first.at("gift_id"),first.at("group_id")}).dump());
                    for(const auto& [id,old_members]:old_groups) {
                        plan.old_ids.insert(id);int64_t maximum=0;
                        for(const auto* row:old_members)maximum=std::max(maximum,row->body.at("gift_count").get<int64_t>());
                        Statement ownership(raw,"SELECT COUNT(*) FROM events WHERE live_id=? AND COALESCE(json_extract(body,'$.display_id'),event_id)=?");
                        ownership.text(1,plan.live).text(2,id);ownership.row();
                        if(ownership.number(0)!=int64_t(old_members.size()))throw std::runtime_error("shared_display_identity");
                        Statement view(raw,"SELECT last_seq,body FROM message_views WHERE live_id=? AND display_id=?");view.text(1,plan.live).text(2,id);
                        if(!view.row())throw std::runtime_error("missing_display");
                        const auto body=json::parse(view.str(1));
                        if(view.number(0)!=old_members.back()->seq || body.value("event_id","")!=old_members.back()->id ||
                           body.value("type","")!="gift" || body.value("gift_count",int64_t(0))!=maximum)
                            throw std::runtime_error("display_projection_mismatch");
                        Statement analytics(raw,"SELECT quantity FROM analytics_gift_groups WHERE live_id=? AND display_id=?");analytics.text(1,plan.live).text(2,id);
                        if(!analytics.row() || analytics.number(0)!=maximum)throw std::runtime_error("group_total_mismatch");
                        plan.old_quantity=sum(plan.old_quantity,maximum);
                    }
                    if(!plan.old_ids.count(plan.display)) {
                        for(const char* sql:{"SELECT 1 FROM events WHERE live_id=? AND COALESCE(json_extract(body,'$.display_id'),event_id)=? LIMIT 1",
                            "SELECT 1 FROM message_views WHERE live_id=? AND display_id=?","SELECT 1 FROM analytics_gift_groups WHERE live_id=? AND display_id=?"}) {
                            Statement conflict(raw,sql);conflict.text(1,plan.live).text(2,plan.display);if(conflict.row())throw std::runtime_error("target_display_conflict");
                        }
                    }
                    {Statement room(raw,R"SQL(SELECT r.gift,r.total,g.quantity,
                        (SELECT COUNT(*) FROM message_views WHERE live_id=r.live_id AND json_extract(body,'$.type')='gift'),
                        (SELECT COUNT(*) FROM message_views WHERE live_id=r.live_id),
                        (SELECT COALESCE(SUM(json_extract(body,'$.gift_count')),0) FROM message_views WHERE live_id=r.live_id AND json_extract(body,'$.type')='gift')
                        FROM rooms r JOIN gift_totals g ON g.live_id=r.live_id WHERE r.live_id=?)SQL");room.text(1,plan.live);
                        if(!room.row() || room.number(0)!=room.number(3) || room.number(1)!=room.number(4) || room.number(2)!=room.number(5))
                            throw std::runtime_error("room_counters_mismatch");}
                    json previous;
                    for(auto* row:members) {
                        auto body=row->body;body["display_id"]=plan.display;body["recipient_id"]=recipient;body["gift_group_key"]=row->decoded.at("gift_group_key");
                        auto presentation=body;presentation.update(row->overlay);
                        for(const char* field:{"display_id","recipient_id","gift_group_key"})presentation[field]=body.at(field);
                        const auto observed=body.at("gift_count").get<int64_t>();
                        if(!previous.is_null()) {
                            if(presentation.value("gift_name","")=="礼物 "+body.value("gift_id",""))
                                for(const char* field:{"gift_name","content"})if(previous.contains(field))presentation[field]=previous.at(field);
                            if((!presentation.contains("gift_unit_price") || presentation.at("gift_unit_price").is_null()) &&
                               previous.contains("gift_unit_price") && !previous.at("gift_unit_price").is_null())presentation["gift_unit_price"]=previous.at("gift_unit_price");
                            if(previous.value("gift_combo",false) && !body.value("gift_combo",false)) {
                                presentation["gift_combo"]=true;presentation["gift_final"]=previous.value("gift_final",false)||body.value("repeat_end",uint32_t(0))==1;
                            }
                            if(observed<plan.high_water || (previous.value("gift_final",false)&&!presentation.value("gift_final",false)))
                                for(const char* field:{"user_name","user_level","fans_club","gift_name","content","timestamp"})
                                    if(previous.contains(field))presentation[field]=previous.at(field);
                        }
                        const auto quantity=std::max(int64_t(0),observed-plan.high_water);plan.high_water=std::max(plan.high_water,observed);
                        presentation["gift_count"]=plan.high_water;
                        presentation["gift_final"]=(previous.is_object()&&previous.value("gift_final",false))||presentation.value("gift_final",false);
                        json overlay=json::object();for(auto it=presentation.begin();it!=presentation.end();++it)
                            if(!body.contains(it.key()) || body.at(it.key())!=it.value())overlay[it.key()]=it.value();
                        Statement fact(raw,"SELECT event_id,live_id,user_id,kind,gift_quantity,recipient_id FROM analytics_events WHERE seq=?");fact.num(1,row->seq);
                        if(!fact.row() || fact.str(0)!=row->id || fact.str(1)!=row->live || fact.str(2)!=body.value("user_id","") || fact.str(3)!="gift")
                            throw std::runtime_error("analytics_identity_mismatch");
                        const bool event_changed=body!=row->body;
                        plan.changes.push_back({row,std::move(body),std::move(overlay),quantity,recipient,event_changed,fact.number(4)!=quantity||fact.str(5)!=recipient});
                        previous=std::move(presentation);
                    }
                    plan.view=std::move(previous);candidate_plans.push_back(std::move(plan));
                }
                if(candidate_plans.empty())report["unchanged_groups"]=report["unchanged_groups"].get<int64_t>()+1;
                for(auto& plan:candidate_plans) {
                    report["changed_groups"]=report["changed_groups"].get<int64_t>()+1;
                    report["removed_display_rows"]=report["removed_display_rows"].get<int64_t>()+int64_t(plan.old_ids.size())-1;
                    report["gift_quantity_delta"]=report["gift_quantity_delta"].get<int64_t>()+plan.high_water-plan.old_quantity;
                    for(const auto& change:plan.changes) {
                        report["changed_events"]=report["changed_events"].get<int64_t>()+int(change.event_changed);
                        report["changed_analytics"]=report["changed_analytics"].get<int64_t>()+int(change.analytics_changed);
                    }
                    plans.push_back(std::move(plan));
                }
            } catch(const std::exception& error){skip(rows.size(),error.what());}
        }
        if(apply && !plans.empty()) {
            if(!presentations)exec("CREATE TABLE event_presentations(event_id TEXT PRIMARY KEY,fields TEXT NOT NULL)");
            for(const auto& plan:plans) {
                for(const auto& change:plan.changes) {
                    if(change.event_changed) {
                        Statement event(raw,"UPDATE events SET body=? WHERE id=? AND event_id=? AND body=?");
                        event.text(1,change.body.dump()).num(2,change.row->seq).text(3,change.row->id).text(4,change.row->original).row();
                        if(sqlite3_changes(raw)!=1)throw std::runtime_error("Gift event changed during group repair");
                    }
                    if(change.overlay!=change.row->overlay) {
                        if(change.overlay.empty()){Statement erase(raw,"DELETE FROM event_presentations WHERE event_id=?");erase.text(1,change.row->id).row();}
                        else {Statement overlay(raw,"INSERT INTO event_presentations VALUES(?,?) ON CONFLICT(event_id) DO UPDATE SET fields=excluded.fields");overlay.text(1,change.row->id).text(2,change.overlay.dump()).row();}
                    }
                    if(change.analytics_changed) {
                        Statement fact(raw,"UPDATE analytics_events SET gift_quantity=?,recipient_id=? WHERE seq=? AND event_id=? AND live_id=?");
                        fact.num(1,change.quantity).text(2,change.recipient).num(3,change.row->seq).text(4,change.row->id).text(5,change.row->live).row();
                        if(sqlite3_changes(raw)!=1)throw std::runtime_error("Gift analytics changed during group repair");
                    }
                }
                for(const auto& id:plan.old_ids)for(const char* sql:{"DELETE FROM message_views WHERE live_id=? AND display_id=?","DELETE FROM analytics_gift_groups WHERE live_id=? AND display_id=?"}) {
                    Statement erase(raw,sql);erase.text(1,plan.live).text(2,id).row();if(sqlite3_changes(raw)!=1)throw std::runtime_error("Gift projection changed during group repair");
                }
                {Statement view(raw,"INSERT INTO message_views VALUES(?,?,?,?)");view.text(1,plan.live).text(2,plan.display).num(3,plan.changes.back().row->seq).text(4,plan.view.dump()).row();}
                {Statement group(raw,"INSERT INTO analytics_gift_groups VALUES(?,?,?)");group.text(1,plan.live).text(2,plan.display).num(3,plan.high_water).row();}
                {Statement room(raw,"UPDATE rooms SET gift=gift+?,total=total+? WHERE live_id=?");
                    const auto delta=1-int64_t(plan.old_ids.size());room.num(1,delta).num(2,delta).text(3,plan.live).row();
                    if(sqlite3_changes(raw)!=1)throw std::runtime_error("Gift room disappeared during group repair");}
                {Statement quantity(raw,"UPDATE gift_totals SET quantity=quantity+? WHERE live_id=?");quantity.num(1,plan.high_water-plan.old_quantity).text(2,plan.live).row();
                    if(sqlite3_changes(raw)!=1)throw std::runtime_error("Gift totals disappeared during group repair");}
            }
        }
        exec("COMMIT");transaction=false;report["applied"]=apply;
    } catch(const std::exception& error) {
        if(transaction)sqlite3_exec(raw,"ROLLBACK",nullptr,nullptr,nullptr);
        report["failed"]=1;report["error"]=error.what();
    }
    return report;
}
