// Included by compression_test.cpp; exercises the actual storage and parser seams.
void compression_large_bodies_round_trip() {
    set_compression("on");
    TemporaryDatabase database;
    auto event=chat_event("evt-large-body","large detail");
    const std::string payload(17*1024*1024,'x');
    event["_details"]["large"]=payload;
    const auto detail_bytes=event["_details"].dump().size();
    {
        Store store;store.target("111","douyin",true);
        deliver(store,"111",event);
        check(store.message_detail("evt-large-body")["details"]["large"]==payload,"Large stored detail decodes without the upstream frame limit");
    }
    RawSql sql(database.path());
    check(sql.rows("SELECT typeof(body),length(decode_body(body)) FROM event_details")[0]==json::array({"blob",std::to_string(detail_bytes)}),"SQL decoder reads large compressed details");
    sql.exec("UPDATE event_details SET body=encode_body(body)");
    sql.exec("UPDATE event_details SET body=decode_body(body)");
    {
        Store store;
        while(store.compress_batch(10));
        check(store.message_detail("evt-large-body")["details"]["large"]==payload,"Migrating a large legacy detail preserves its readable contents");
    }
}

void compression_frame_limit_is_preserved() {
    const std::string boundary(16*1024*1024,'x');
    check(inflate_frame(gzip_encode(boundary))==boundary,"Upstream frames at the size limit still decode");
    bool too_large=false;
    try { inflate_frame(gzip_encode(boundary+"x")); } catch(const std::exception&) { too_large=true; }
    check(too_large,"Upstream frames over 16 MiB are still rejected");
    const std::string corrupt("\x1f\x8b\0\0",4);
    bool invalid_frame=false,invalid_body=false;
    try { inflate_frame(corrupt); } catch(const std::exception&) { invalid_frame=true; }
    try { decode_body(corrupt); } catch(const std::exception&) { invalid_body=true; }
    check(invalid_frame && invalid_body,"Both frame and body decoders reject invalid gzip streams");
}
