// Included after gift_fact_repair_test.inc for its isolated SQL inspection helper.
// Feed real protobuf through the production parser, then reproduce the old
// non-combo Store identity policy while keeping all saved protocol evidence.
static std::string group_repair_test_gift(Store& store,const std::string& live,int message_id,
        int group,int count,uint64_t recipient=456,int price=30000,bool old_split=true) {
    GiftMessage gift;gift.set_gift_id(4095);gift.set_group_id(group);gift.set_repeat_count(count);gift.set_combo_count(count);
    gift.mutable_common()->set_create_time(1700000000+message_id);gift.mutable_user()->set_id(9007199254740993LL);
    gift.mutable_user()->set_nickname("protocol profile");if(recipient)gift.mutable_to_user()->set_id(recipient);
    gift.mutable_gift()->set_id(4095);gift.mutable_gift()->set_name("group repair gift");gift.mutable_gift()->set_combo(false);
    if(price>=0)gift.mutable_gift()->set_diamondcount(price);
    Response response;auto* message=response.add_messageslist();message->set_method("WebcastGiftMessage");
    message->set_msgid(message_id);message->set_payload(gift.SerializeAsString());
    PushFrame push;push.set_payloadtype("msg");push.set_payload(response.SerializeAsString());
    pipeline::RawFrameEnvelope envelope;envelope.set_schema_version(1);envelope.set_frame_id("group-repair-"+live+"-"+std::to_string(message_id));
    envelope.set_live_id(live);envelope.set_room_id("room-"+live);envelope.set_source("douyin");envelope.set_kind("upstream_frame");
    envelope.set_session_id("group-repair-fixture");envelope.set_session_seq(message_id);envelope.set_received_at_ms(1700000200000LL+message_id);
    envelope.set_desired_version(1);envelope.set_payload(push.SerializeAsString());envelope.set_payload_sha256(digest(envelope.payload()));
    auto parsed=parse(envelope);check(parsed.second.empty() && parsed.first.size()==1,"Real group repair fixture parses without quarantine");
    auto& event=parsed.first[0];const auto id=event.at("event_id").get<std::string>();const auto key=event.value("gift_group_key","");
    if(old_split){event["display_id"]=id;event.erase("gift_group_key");}
    store.commit(envelope,envelope.SerializeAsString(),parsed.first,parsed.second);
    if(old_split && !key.empty()) {
        GiftRepairSql sql(std::filesystem::path(env("DATABASE_PATH","/data/pipeline.db")));
        for(const auto* table:{"events","message_views"})sql.exec(std::string("UPDATE ")+table+
            " SET body=json_set(body,'$.gift_group_key','"+key+"') WHERE json_extract(body,'$.event_id')='"+id+"'");
    }
    return id;
}

void gift_group_repair_restores_split_groups() {
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);
        for(int i=1;i<=10;++i)group_repair_test_gift(store,"111",i,42,i);
        check_stats(store,"111",10,55);
        check(store.analytics_ranking(AnalyticsQuery{},true)["items"][0]["known_gift_value"]=="1650000","Old split fixture really overcounts cumulative gift value");
    }
    GiftRepairSql sql(database.path());
    sql.exec("UPDATE events SET body=json_set(body,'$.user_name','manual profile','$.fans_club',json('{\"level\":19,\"name\":\"manual club\"}'))");
    sql.exec("UPDATE analytics_events SET user_name='manual profile',fans_club='{\"level\":19,\"name\":\"manual club\"}'");
    sql.exec("UPDATE message_views SET body=json_set(body,'$.user_name','manual profile','$.fans_club',json('{\"level\":19,\"name\":\"manual club\"}'))");
    sql.exec("INSERT INTO event_presentations VALUES('room-111:WebcastGiftMessage:5:0','{\"content\":\"manual display label\"}')");
    const auto before=sql.saved();
    const auto identities=sql.rows("SELECT id,event_id FROM events ORDER BY id");
    const auto facts=sql.rows("SELECT id,json_extract(body,'$.gift_count'),json_extract(body,'$.gift_unit_price'),json_extract(body,'$.timestamp'),json_extract(body,'$.gift_combo'),json_extract(body,'$.user_name'),json_extract(body,'$.fans_club') FROM events ORDER BY id");
    const auto analytics_facts=sql.rows("SELECT seq,event_id,live_id,user_id,user_name,occurred_at_ms,gift_unit_price,user_level,fans_club FROM analytics_events ORDER BY seq");
    const auto dry=repair_gift_groups(false);
    check(dry["failed"]==0 && dry["changed_groups"]==1 && dry["removed_display_rows"]==9 && dry["gift_quantity_delta"]==-45 && dry["skipped_groups"]==0,"Dry run diagnoses the complete 55-to-10 repair");
    check(dry["backup"].is_null() && sql.saved()==before,"Group dry run leaves every table unchanged and makes no backup");
    const auto applied=repair_gift_groups(true);
    check(applied["failed"]==0 && applied["applied"]==true && applied["changed_events"]==10 && applied["changed_analytics"]==9,"Group repair applies identity and cumulative quantity changes");
    check(std::filesystem::exists(applied.at("backup").get<std::string>()),"Group apply creates a full backup");
    {GiftRepairSql backup(applied.at("backup").get<std::string>());check(backup.saved()==before,"Group backup contains original facts, projections and counters");}
    const auto after=sql.saved();
    for(const char* table:{"event_details","receipts"})check(after[table]==before[table],std::string("Group repair preserves ")+table);
    check(sql.rows("SELECT id,event_id FROM events ORDER BY id")==identities,"Group repair preserves delivery sequence and identities");
    check(sql.rows("SELECT id,json_extract(body,'$.gift_count'),json_extract(body,'$.gift_unit_price'),json_extract(body,'$.timestamp'),json_extract(body,'$.gift_combo'),json_extract(body,'$.user_name'),json_extract(body,'$.fans_club') FROM events ORDER BY id")==facts,"Group repair preserves quantities, prices, times, original combo and manual profiles");
    check(sql.rows("SELECT seq,event_id,live_id,user_id,user_name,occurred_at_ms,gift_unit_price,user_level,fans_club FROM analytics_events ORDER BY seq")==analytics_facts,"Group repair changes no analytics price, time or profile fact");
    check(sql.rows("SELECT DISTINCT gift_quantity FROM analytics_events")==json::array({json::array({"1"})}),"All ten analytics deliveries now contribute exactly one unit");
    check(sql.rows("SELECT json_extract(fields,'$.content') FROM event_presentations WHERE event_id='room-111:WebcastGiftMessage:5:0'")[0][0]=="manual display label","Unrelated presentation repair survives regrouping");
    {
        Store store;check_stats(store,"111",1,10);
        const auto snapshot=store.snapshot("111");const auto stream=store.frontend_batch("111",0).at("events");
        check(snapshot["events"].size()==1 && stream.size()==10,"One visible gift keeps all ten original journal deliveries");
        const auto id=snapshot["events"][0]["display_id"];
        for(size_t i=0;i<stream.size();++i)check(stream[i]["display_id"]==id && stream[i]["gift_count"]==i+1,"Reconstructed WS history uses one identity with monotone cumulative progress");
        const auto rank=store.analytics_ranking(AnalyticsQuery{},true)["items"][0];
        check(rank["gift_quantity"]=="10" && rank["known_gift_value"]=="300000","Ranking and snapshot agree after regrouping");
    }
    const auto again=repair_gift_groups(true);
    check(again["failed"]==0 && again["changed_groups"]==0 && again["changed_events"]==0 && sql.saved()==after,"Repeated group apply is idempotent");
    {
        Store store;group_repair_test_gift(store,"111",11,42,11,456,30000,false);
        check_stats(store,"111",1,11);
    }
}

void gift_group_repair_separates_recipients_and_skips_unproven_groups() {
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);store.target("222","douyin",true);store.target("333","douyin",true);
        group_repair_test_gift(store,"111",101,42,1,11,10);group_repair_test_gift(store,"111",102,42,3,11,10);
        group_repair_test_gift(store,"111",103,42,2,22,10);group_repair_test_gift(store,"111",104,42,4,22,10);
        group_repair_test_gift(store,"222",105,42,2,11,10);group_repair_test_gift(store,"222",106,42,5,11,10);
        // No recipient may be guessed when the same coarse group names two.
        group_repair_test_gift(store,"111",107,43,1,11,10);group_repair_test_gift(store,"111",108,43,1,22,10);
        group_repair_test_gift(store,"111",109,43,3,0,10);
        // A uniquely identified group with no recipient retains its first ID.
        group_repair_test_gift(store,"111",110,44,1,0,10);group_repair_test_gift(store,"111",111,44,4,0,-1);
        group_repair_test_gift(store,"111",112,44,2,0,10);
        group_repair_test_gift(store,"111",113,45,1,11,10);group_repair_test_gift(store,"111",114,45,2,11,10);
        // Omission before and after the one proven recipient resolves to that
        // identity, including the historical frame that arrived before it.
        group_repair_test_gift(store,"333",115,46,1,0,10);group_repair_test_gift(store,"333",116,46,3,456,10);
        group_repair_test_gift(store,"333",117,46,5,0,10);check_stats(store,"333",3,9);
    }
    GiftRepairSql sql(database.path());sql.exec("DELETE FROM event_details WHERE event_id='room-111:WebcastGiftMessage:113:0'");
    const auto skipped=sql.rows("SELECT id,body FROM events WHERE json_extract(body,'$.group_id') IN ('43','45') ORDER BY id");
    const auto mixed_payloads=sql.rows("SELECT event_id,body FROM event_details WHERE event_id LIKE 'room-333:%' ORDER BY event_id");
    const auto applied=repair_gift_groups(true);
    check(applied["failed"]==0 && applied["changed_groups"]==5 && applied["skipped_groups"]==2 && applied["skipped_events"]==5,"Only independently proven recipient and room partitions repair");
    check(applied["skip_reasons"]["ambiguous_recipient"]==1 && applied["skip_reasons"]["missing_payload"]==1,"Skipped groups retain explicit evidence failure reasons");
    check(sql.rows("SELECT id,body FROM events WHERE json_extract(body,'$.group_id') IN ('43','45') ORDER BY id")==skipped,"Ambiguous and incomplete groups stay byte-for-byte untouched");
    check(sql.rows("SELECT DISTINCT json_extract(body,'$.recipient_id'),json_extract(body,'$.display_id') FROM events WHERE live_id='111' AND json_extract(body,'$.group_id')='42'").size()==2,"Different recipients remain distinct even with the same platform group ID");
    check(sql.rows("SELECT DISTINCT json_extract(body,'$.display_id'),json_extract(body,'$.recipient_id') FROM events WHERE live_id='111' AND json_extract(body,'$.group_id')='44'")==json::array({json::array({"room-111:WebcastGiftMessage:110:0",""})}),"All-missing recipient group retains first identity without inventing a user");
    check(sql.rows("SELECT gift_quantity,gift_unit_price FROM analytics_events WHERE event_id='room-111:WebcastGiftMessage:111:0'")[0]==json::array({"3",nullptr}),"Sparse price remains unknown while its cumulative increment is repaired");
    check(sql.rows("SELECT gift_quantity FROM analytics_events WHERE event_id='room-111:WebcastGiftMessage:112:0'")[0][0]=="0","Late smaller progress adds zero units");
    const auto mixed_id="gift:"+digest(json::array({"room-333",uid,"456","4095","46"}).dump());
    check(sql.rows("SELECT DISTINCT json_extract(body,'$.recipient_id'),json_extract(body,'$.display_id') FROM events WHERE live_id='333'")==json::array({json::array({"456",mixed_id})}),"Missing-known-missing observations share the unique proven recipient identity");
    check(sql.rows("SELECT gift_quantity,recipient_id FROM analytics_events WHERE live_id='333' ORDER BY seq")==json::array({json::array({"1","456"}),json::array({"2","456"}),json::array({"2","456"})}),"Mixed-recipient repair synchronizes analytics identities and cumulative increments");
    check(sql.rows("SELECT event_id,body FROM event_details WHERE event_id LIKE 'room-333:%' ORDER BY event_id")==mixed_payloads,"Resolving an omitted recipient never rewrites the saved raw payload");
    {
        Store store;check_stats(store,"111",8,19);check_stats(store,"222",1,5);check_stats(store,"333",1,5);
        const auto combined=display_event(store.snapshot("111"),"room-111:WebcastGiftMessage:110:0");
        check(combined["gift_count"]==4 && combined["gift_unit_price"]==10,"Late progress preserves cumulative display and known presentation price");
        const auto raw=store.events("111",0,200);
        bool late=false;for(const auto& event:raw)if(event["event_id"]=="room-111:WebcastGiftMessage:112:0")late=event["gift_count"]==2;
        check(late,"Late original quantity remains an immutable fact");
        const auto mixed_stream=store.frontend_batch("333",0).at("events");
        check(mixed_stream.size()==3 && mixed_stream[0]["gift_count"]==1 && mixed_stream[1]["gift_count"]==3 && mixed_stream[2]["gift_count"]==5,"Mixed-recipient WS history retains all original cumulative observations");
        for(const auto& event:mixed_stream)check(event["display_id"]==mixed_id && event["recipient_id"]=="456","WS replay resolves every mixed-recipient event to one group");
    }
    {
        // Reopen the Store, then test both an omitted and an explicit recipient
        // against the repaired identity using the unmodified current parser.
        Store store;group_repair_test_gift(store,"333",118,46,6,0,10,false);
        group_repair_test_gift(store,"333",119,46,7,456,10,false);check_stats(store,"333",1,7);
        check(display_event(store.snapshot("333"),mixed_id)["gift_count"]==7,"Post-restart sparse and explicit progress continue the repaired mixed-recipient group");
    }
}

void gift_group_repair_rolls_back_all_projections() {
    TemporaryDatabase database;
    {Store store;store.target("111","douyin",true);group_repair_test_gift(store,"111",201,61,1);group_repair_test_gift(store,"111",202,61,2);}
    GiftRepairSql sql(database.path());
    // Fail after event/analytics/view mutations, to exercise the whole transaction.
    sql.exec("CREATE TRIGGER reject_group_repair BEFORE UPDATE OF quantity ON gift_totals BEGIN SELECT RAISE(ABORT,'injected group counter failure'); END");
    const auto before=sql.saved();const auto failed=repair_gift_groups(true);
    check(failed["failed"]==1 && failed["applied"]==false && sql.saved()==before,"Counter failure rolls back every identity, overlay, analytics, projection and room write");
    {GiftRepairSql backup(failed.at("backup").get<std::string>());check(backup.saved()==before,"Failed group repair retains a complete prior backup");}
    sql.exec("DROP TRIGGER reject_group_repair");
    const auto applied=repair_gift_groups(true);check(applied["failed"]==0 && applied["changed_groups"]==1,"Group repair succeeds after the injected failure is removed");
    {Store store;check_stats(store,"111",1,2);}
}
