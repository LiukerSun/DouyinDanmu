// Offline command: dry-run by default. Apply uses the same parser and Store
// transaction as live ingestion; receipts and business event IDs stay intact.
static int replay_quarantine(bool apply) {
    sqlite3* raw_db=nullptr;
    if(sqlite3_open_v2(env("DATABASE_PATH","/data/pipeline.db").c_str(),&raw_db,SQLITE_OPEN_READONLY,nullptr)!=SQLITE_OK)
        throw std::runtime_error("Cannot open replay database");
    std::unique_ptr<sqlite3,decltype(&sqlite3_close)> database(raw_db,sqlite3_close);
    std::unique_ptr<Store> store;if(apply)store=std::make_unique<Store>();
    json report={{"apply",apply},{"frames",0},{"failed_frames",0},{"messages",0},{"methods",json::object()},{"statuses",json::object()}};
    // The read connection holds a stable snapshot while writes use WAL.
    Statement rows(raw_db,"SELECT frame_id,raw FROM quarantine GROUP BY frame_id ORDER BY created_at");
    while(rows.row()) {
        report["frames"]=report["frames"].get<int>()+1;
        const auto raw=rows.binary(1);pipeline::RawFrameEnvelope frame;
        try {
            if(!frame.ParseFromString(raw) || frame.schema_version()!=1 || frame.frame_id()!=rows.str(0) || frame.live_id().empty() || digest(frame.payload())!=frame.payload_sha256())
                throw std::runtime_error("invalid replay envelope");
            const auto parsed=parse(frame);
            if(!parsed.second.empty())report["failed_frames"]=report["failed_frames"].get<int>()+1;
            for(const auto& event:parsed.first) {
                report["messages"]=report["messages"].get<int>()+1;
                auto method=event.value("method","");auto status=event.value("parse_status","legacy");
                report["methods"][method]=report["methods"].value(method,0)+1;
                report["statuses"][status]=report["statuses"].value(status,0)+1;
            }
            if(apply)store->commit(frame,raw,parsed.first,parsed.second,true);
        } catch(const std::exception& failure) {
            report["failed_frames"]=report["failed_frames"].get<int>()+1;
            std::cerr<<"Replay frame "<<rows.str(0)<<": "<<failure.what()<<'\n';
        }
    }
    std::cout<<report.dump(2)<<std::endl;
    return report["failed_frames"].get<int>()?2:0;
}
