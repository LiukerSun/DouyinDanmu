// Re-decode saved payloads without re-ingesting events, advancing cursors, or
// replaying counters. Each changed detail and its display metadata are atomic.
static std::string decode_saved_base64(const std::string& input) {
    if(input.size()>24*1024*1024 || input.size()%4)throw std::runtime_error("invalid saved base64 size");
    const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;uint32_t bits=0;int available=0;
    for(char c:input) {
        if(c=='=')break;
        const auto n=alphabet.find(c);if(n==std::string::npos)throw std::runtime_error("invalid saved base64 character");
        bits=(bits<<6)|uint32_t(n);available+=6;
        if(available>=8){available-=8;out+=char((bits>>available)&255);}
    }
    if(payload_base64(out)!=input)throw std::runtime_error("noncanonical saved base64");
    return out;
}
static json redecode_details(bool apply) {
    sqlite3* raw=nullptr;
    if(sqlite3_open_v2(env("DATABASE_PATH","/data/pipeline.db").c_str(),&raw,apply?SQLITE_OPEN_READWRITE:SQLITE_OPEN_READONLY,nullptr)!=SQLITE_OK) {
        if(raw)sqlite3_close(raw);throw std::runtime_error("Cannot open detail database");
    }
    std::unique_ptr<sqlite3,decltype(&sqlite3_close)> db(raw,sqlite3_close);sqlite3_busy_timeout(raw,5000);
    auto exec=[&](const char* sql){if(sqlite3_exec(raw,sql,nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(raw));};
    json report={{"apply",apply},{"scanned",0},{"updated",0},{"failed",0},{"promoted",0},{"analyzed_entries",0},{"methods",json::object()}};
    // Materialize identifiers only, so writes do not invalidate an active scan.
    std::vector<std::string> ids;
    {Statement q(raw,"SELECT d.event_id FROM event_details d JOIN events e ON e.event_id=d.event_id WHERE COALESCE(json_extract(d.body,'$.field_analysis.version'),0)<1 ORDER BY e.id");while(q.row())ids.push_back(q.str(0));}
    for(const auto& id:ids) {
        report["scanned"]=report["scanned"].get<int>()+1;
        bool transaction=false;
        try {
            std::string original,body;
            {Statement q(raw,"SELECT e.body,d.body FROM events e JOIN event_details d ON d.event_id=e.event_id WHERE e.event_id=?");q.text(1,id);if(!q.row())continue;body=q.str(0);original=q.str(1);}
            auto old=json::parse(body),detail=json::parse(original);
            const auto payload=decode_saved_base64(detail.at("payload_base64").get<std::string>());
            const auto method=detail.at("method").get<std::string>();
            auto candidate=observed_event(id,method,payload,old.value("timestamp",int64_t(0)));decode_observed(candidate,payload);
            if(!candidate["_details"].contains("field_analysis"))candidate["_details"]["field_analysis"]=field_analysis(nullptr,payload);
            detail.update(candidate["_details"]);
            json patch={{"parse_status",candidate["parse_status"]},{"parser_version",3},{"has_details",true}};
            if(old.value("type","")=="unknown" && candidate["type"]!="unknown") {
                patch["type"]=candidate["type"];patch["content"]=candidate["content"];
                report["promoted"]=report["promoted"].get<int>()+1;
            }
            report["methods"][method]=report["methods"].value(method,0)+1;
            report["analyzed_entries"]=report["analyzed_entries"].get<int>()+int(detail["field_analysis"]["entries"].size());
            if(apply) {
                exec("BEGIN IMMEDIATE");transaction=true;
                Statement update(raw,"UPDATE event_details SET body=? WHERE event_id=? AND body=?");update.text(1,detail.dump()).text(2,id).text(3,original).row();
                if(sqlite3_changes(raw)!=1)throw std::runtime_error("detail changed during re-decode");
                Statement event(raw,"UPDATE events SET body=json_patch(body,?) WHERE event_id=?");event.text(1,patch.dump()).text(2,id).row();
                Statement view(raw,"UPDATE message_views SET body=json_patch(body,?) WHERE live_id=? AND json_extract(body,'$.event_id')=?");view.text(1,patch.dump()).text(2,old.at("live_id").get<std::string>()).text(3,id).row();
                exec("COMMIT");transaction=false;
            }
            report["updated"]=report["updated"].get<int>()+1;
        } catch(const std::exception& error) {
            if(transaction)sqlite3_exec(raw,"ROLLBACK",nullptr,nullptr,nullptr);
            report["failed"]=report["failed"].get<int>()+1;std::cerr<<"Detail "<<id<<": "<<error.what()<<'\n';
        }
    }
    return report;
}
