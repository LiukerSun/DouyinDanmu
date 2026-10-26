// Included by compression_test.cpp after its isolated DB, fixtures and helpers.

void compression_round_trips_through_store() {
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);
        deliver(store,"111",chat_event("evt-1","你好世界"));
        // A late smaller gift progress forces a presentation overlay row, and
        // the established long content keeps the overlay above the 64-byte floor.
        deliver(store,"111",gift_event("evt-g1","disp-g",5,true,"一开始就送出的大礼物组合·"+std::string(96,'x')));
        deliver(store,"111",gift_event("evt-g2","disp-g",2));
    }
    RawSql sql(database.path());
    check(sql.rows("SELECT typeof(body) FROM events WHERE event_id='evt-1'")[0][0]=="blob","events bodies are stored compressed");
    check(sql.rows("SELECT typeof(body) FROM event_details WHERE event_id='evt-1'")[0][0]=="blob","event_details bodies are stored compressed");
    check(sql.rows("SELECT typeof(fields) FROM event_presentations WHERE event_id='evt-g2'")[0][0]=="blob","presentation overlays are stored compressed");
    check(sql.rows("SELECT DISTINCT typeof(body) FROM message_views")==json::array({json::array({"text"})}),"message_views bodies stay plain text");
    {
        Store store;
        const auto detail=store.message_detail("evt-1");
        check(detail["event"]["content"]=="你好世界" && detail["details"]["payload_base64"]=="CAE=","Detail API decodes compressed event and payload");
        const auto batch=store.frontend_batch("111",0);
        check(batch["events"].size()==3 && batch["events"][0]["content"]=="你好世界","WS batch decodes compressed events");
        // The overlay keeps the established cumulative count over the late fact.
        bool overlay=false;
        for(const auto& event:store.events("111",0))if(event["event_id"]=="evt-g2")overlay=event["gift_count"]==2;
        check(overlay,"Plain event reads keep the late delivery fact");
        MessageSearch deliveries;deliveries.deliveries=true;
        check(store.search_messages(deliveries)["events"].size()==3,"Deliveries search decodes compressed events");
        MessageSearch views;
        check(store.search_messages(views)["events"].size()==2,"Snapshot search keeps reading plain views");
        const auto methods=store.message_methods("111");
        bool catalogued=false;for(const auto& m:methods)if(m["method"]=="WebcastChatMessage")catalogued=true;
        check(catalogued,"Method catalogue decodes bodies");
        const auto snapshot=store.snapshot("111");
        check(snapshot["events"].size()==2,"Snapshot feed is unchanged");
    }
}

void compression_udfs_decode_inside_sql() {
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);
        deliver(store,"111",chat_event("evt-udf","函数内解码"));
    }
    RawSql sql(database.path());
    check(sql.rows("SELECT json_extract(decode_body(body),'$.type') FROM events WHERE event_id='evt-udf'")[0][0]=="chat","decode_body works inside json_extract");
    check(sql.rows("SELECT feed_visible(body) FROM events WHERE event_id='evt-udf'")[0][0]=="1","feed_visible accepts compressed bodies");
    const auto encoded=sql.rows("SELECT typeof(encode_body(body)),length(decode_body(encode_body(body)))=length(decode_body(body)) FROM events WHERE event_id='evt-udf'")[0];
    check(encoded[0]=="blob" && encoded[1]=="1","encode_body round-trips compressed values exactly once");
}

void compression_reads_legacy_plain_rows() {
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);
        deliver(store,"111",chat_event("evt-legacy","旧格式"));
    }
    RawSql sql(database.path());
    sql.exec("UPDATE events SET body=decode_body(body)");
    sql.exec("UPDATE event_details SET body=decode_body(body)");
    check(sql.rows("SELECT typeof(body) FROM events")[0][0]=="text","Fixture really holds legacy plain rows");
    {
        Store store;
        check(store.message_detail("evt-legacy")["event"]["content"]=="旧格式","Store reads legacy plain bodies");
        check(store.frontend_batch("111",0)["events"].size()==1,"WS batch reads legacy plain bodies");
    }
    check(sql.rows("SELECT json_extract(decode_body(body),'$.type') FROM events")[0][0]=="chat","UDF passes legacy text through");
    check(sql.rows("SELECT feed_visible(body) FROM events")[0][0]=="1","feed_visible passes legacy text through");
}

void compression_replay_hydration_stays_valid() {
    TemporaryDatabase database;
    auto hydrated=json::object();
    {
        Store store;store.target("111","douyin",true);
        deliver(store,"111",chat_event("evt-replay","回放补水"));
        auto replay=chat_event("evt-replay","回放补水");replay.erase("_details");
        replay["has_details"]=true;replay["parse_status"]="ok";replay["payload_bytes"]=2;
        // The hydration patch exercises encode_body(json_patch(decode_body(body))).
        auto envelope=compression_frame("111",replay);
        store.commit(envelope,envelope.SerializeAsString(),json::array({replay}),json::array(),true);
        hydrated=store.message_detail("evt-replay")["event"];
    }
    check(hydrated.value("has_details",false) && hydrated.value("parse_status","")=="ok","Hydration patch applies on compressed bodies");
    RawSql sql(database.path());
    check(sql.rows("SELECT typeof(body) FROM events WHERE event_id='evt-replay'")[0][0]=="blob","Hydrated body stays compressed");
}

void compression_migrates_legacy_rows_in_batches() {
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);
        for(int i=0;i<5;++i)deliver(store,"111",chat_event("evt-m"+std::to_string(i),"迁移第"+std::to_string(i)+"条"));
    }
    RawSql sql(database.path());
    // Rewind to a legacy all-text database, as an upgrade finds it.
    sql.exec("UPDATE events SET body=decode_body(body)");
    sql.exec("UPDATE event_details SET body=decode_body(body)");
    sql.exec("DELETE FROM compression_state");
    const auto before=sql.rows("SELECT event_id,decode_body(body) FROM events ORDER BY id");
    const auto before_details=sql.rows("SELECT event_id,decode_body(body) FROM event_details ORDER BY event_id");
    {
        Store store;
        int passes=0;
        while(store.compress_batch(2) && passes++<100);
        check(passes<100,"Batch migration terminates");
        const auto status=store.overview()["maintenance"];
        check(status["vacuum_pending"]==true,"Completed migration queues the one-time vacuum");
        store.vacuum();
        check(store.overview()["maintenance"]["vacuum_pending"]==false,"Vacuum clears its pending flag");
        check(store.frontend_batch("111",0)["events"].size()==5,"Migrated events still feed the WS outbox");
    }
    check(sql.rows("SELECT event_id,decode_body(body) FROM events ORDER BY id")==before,"Migration preserves every event byte-for-byte after decoding");
    check(sql.rows("SELECT event_id,decode_body(body) FROM event_details ORDER BY event_id")==before_details,"Migration preserves every detail byte-for-byte after decoding");
    check(sql.rows("SELECT typeof(body) FROM events")[0][0]=="blob","Migrated events are compressed");
    check(sql.rows("SELECT typeof(body) FROM message_views")[0][0]=="text","Migration never touches plain views");
    check(sql.rows("SELECT scope,done FROM compression_state ORDER BY scope")==json::array({json::array({"event_details","1"}),json::array({"event_presentations","1"}),json::array({"events","1"}),json::array({"vacuum","1"})}),"All compression scopes complete including the vacuum");
}

void compression_rejects_corrupt_blobs() {
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);
        deliver(store,"111",chat_event("evt-corrupt","损坏"));
    }
    RawSql sql(database.path());
    sql.exec("UPDATE events SET body=x'01020304' WHERE event_id='evt-corrupt'");
    bool cpp_threw=false;
    {
        Store store;
        try { store.events("111",0); } catch(const std::exception&) { cpp_threw=true; }
    }
    check(cpp_threw,"C++ reads reject non-gzip blobs");
    bool sql_failed=false;
    try { sql.rows("SELECT json_extract(decode_body(body),'$.type') FROM events"); } catch(...) { sql_failed=true; }
    check(sql_failed,"SQL decode rejects non-gzip blobs");
}

void compression_off_stores_plain_then_migrates() {
    set_compression("off");
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);
        deliver(store,"111",chat_event("evt-off","未压缩"));
    }
    RawSql sql(database.path());
    check(sql.rows("SELECT typeof(body) FROM events")[0][0]=="text","COMPRESSION=off stores plain text");
    set_compression("on");
    {
        Store store;
        while(store.compress_batch(10));
    }
    check(sql.rows("SELECT typeof(body) FROM events")[0][0]=="blob","Re-enabling compression migrates offline rows");
    set_compression("on");
}

void compression_completed_pass_can_be_compacted_manually() {
    TemporaryDatabase database;
    set_compression("off");
    { Store store;store.target("111","douyin",true);deliver(store,"111",chat_event("manual","manual compact")); }
    set_compression("on");
    Store store;
    while(store.compress_batch(2));
    check(store.vacuum_pending(),"Migration leaves startup compaction pending");
    check(store.request_compact()==0,"Startup compaction pending must not block a manual request");
    check(store.request_compact()==1,"Duplicate manual requests stay deduplicated");
    store.vacuum();
    check(!store.vacuum_pending(),"Manual compaction clears the startup flag");
    vacuum_requested=false;
}

void compression_reenabled_after_completed_pass() {
    TemporaryDatabase database;
    { Store store;store.target("111","douyin",true);deliver(store,"111",chat_event("first","first"));while(store.compress_batch(2)); }
    set_compression("off");
    { Store store;deliver(store,"111",chat_event("second","second")); }
    set_compression("on");
    { Store store;while(store.compress_batch(2)); }
    RawSql sql(database.path());
    check(sql.rows("SELECT typeof(body) FROM events WHERE event_id='second'")[0][0]=="blob","Re-enabled compression revisits rows written after a completed pass");
}

void compression_idle_does_not_checkpoint_active_readers() {
    TemporaryDatabase database;
    set_compression("off");
    { Store store;store.target("111","douyin",true);deliver(store,"111",chat_event("idle","idle")); }
    set_compression("on");
    Store store;
    while(store.compress_batch(2));
    store.vacuum();
    RawSql reader(database.path());
    reader.exec("BEGIN");reader.rows("SELECT * FROM rooms");
    store.target("222","douyin",true);
    const auto start=std::chrono::steady_clock::now();
    check(!store.compress_batch(2),"Completed migration stays idle");
    const auto elapsed=std::chrono::steady_clock::now()-start;
    reader.exec("ROLLBACK");
    check(elapsed<std::chrono::seconds(1),"Idle maintenance must not wait for readers to checkpoint the WAL");
}

void compression_revisits_offline_edits_before_the_cursor() {
    TemporaryDatabase database;
    set_compression("off");
    { Store store;store.target("111","douyin",true);for(int i=0;i<5;++i)deliver(store,"111",chat_event("edit-"+std::to_string(i),"offline fixture")); }
    set_compression("on");
    { Store store;check(store.compress_batch(2),"Start a partial migration"); }
    RawSql sql(database.path());
    sql.exec("UPDATE event_details SET body=json_set(decode_body(body),'$.offline_edit',1) WHERE event_id='edit-0'");
    {
        Store store;
        while(store.compress_batch(2));
        check(store.message_detail("edit-0")["details"]["offline_edit"]==1,"Restart preserves an offline edit before the old cursor");
    }
    check(sql.rows("SELECT typeof(body) FROM event_details WHERE event_id='edit-0'")[0][0]=="blob","A partial pass revisits an edited earlier row");
    sql.exec("UPDATE events SET body=json_set(decode_body(body),'$.offline_edit',2) WHERE event_id='edit-0'");
    {
        Store store;
        while(store.compress_batch(2));
        check(store.message_detail("edit-0")["event"]["offline_edit"]==2,"Completed passes also preserve offline edits");
    }
    check(sql.rows("SELECT typeof(body) FROM events WHERE event_id='edit-0'")[0][0]=="blob","A completed pass is dirtied by an offline TEXT update");
}
