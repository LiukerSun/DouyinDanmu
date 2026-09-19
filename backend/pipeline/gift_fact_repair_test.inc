// Included by gift_store_test.cpp after its isolated DB and assertion helpers.
class GiftRepairSql {
    sqlite3* db=nullptr;
public:
    explicit GiftRepairSql(const std::filesystem::path& path) {check(sqlite3_open(path.string().c_str(),&db)==SQLITE_OK,"Open isolated gift repair database");}
    ~GiftRepairSql(){sqlite3_close(db);}
    void exec(const std::string& sql){check(sqlite3_exec(db,sql.c_str(),nullptr,nullptr,nullptr)==SQLITE_OK,sqlite3_errmsg(db));}
    json rows(const std::string& sql) {
        sqlite3_stmt* raw=nullptr;check(sqlite3_prepare_v2(db,sql.c_str(),-1,&raw,nullptr)==SQLITE_OK,sqlite3_errmsg(db));
        std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> statement(raw,sqlite3_finalize);
        json rows=json::array();int status;
        while((status=sqlite3_step(raw))==SQLITE_ROW) {
            json row=json::array();
            for(int i=0;i<sqlite3_column_count(raw);++i) {
                const auto value=sqlite3_column_text(raw,i);row.push_back(value?json(reinterpret_cast<const char*>(value)):json(nullptr));
            }
            rows.push_back(std::move(row));
        }
        check(status==SQLITE_DONE,sqlite3_errmsg(db));return rows;
    }
    json saved() {
        json saved=json::object();
        for(const char* table:{"events","event_details","event_presentations","analytics_events","analytics_gift_groups","message_views","gift_totals","rooms","receipts"})
            saved[table]=rows(std::string("SELECT * FROM ")+table+" ORDER BY 1");
        return saved;
    }
};

static std::string repair_test_gift(Store& store,int message_id,int group,int count,int price,int64_t seconds,bool final=false) {
    GiftMessage gift;gift.set_gift_id(4095);gift.set_group_id(group);gift.set_repeat_count(count);gift.set_repeat_end(final?1:0);
    gift.mutable_common()->set_create_time(seconds);gift.mutable_user()->set_id(9007199254740993LL);gift.mutable_user()->set_nickname("payload profile");
    gift.mutable_to_user()->set_id(456);gift.mutable_gift()->set_id(4095);gift.mutable_gift()->set_name("repair gift");gift.mutable_gift()->set_combo(true);
    if(price>=0)gift.mutable_gift()->set_diamondcount(price);
    Response response;auto* item=response.add_messageslist();item->set_method("WebcastGiftMessage");item->set_msgid(message_id);item->set_payload(gift.SerializeAsString());
    PushFrame push;push.set_payloadtype("msg");push.set_payload(response.SerializeAsString());
    pipeline::RawFrameEnvelope envelope;envelope.set_schema_version(1);envelope.set_frame_id("gift-repair-"+std::to_string(message_id));envelope.set_live_id("111");envelope.set_room_id("room-111");
    envelope.set_source("douyin");envelope.set_kind("upstream_frame");envelope.set_session_id("repair-fixture");envelope.set_session_seq(message_id);envelope.set_received_at_ms(1700000200000LL+message_id);envelope.set_desired_version(1);
    envelope.set_payload(push.SerializeAsString());envelope.set_payload_sha256(digest(envelope.payload()));
    const auto parsed=parse(envelope);check(parsed.second.empty() && parsed.first.size()==1,"Real gift fixture parses without quarantine");
    store.commit(envelope,envelope.SerializeAsString(),parsed.first,parsed.second);
    return parsed.first[0].at("event_id");
}

void gift_fact_repair_restores_proven_groups() {
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);
        repair_test_gift(store,101,42,1,10,1700000000);
        repair_test_gift(store,102,42,2,-1,1700000001);
        repair_test_gift(store,103,42,5,0,1700000002,true);
        repair_test_gift(store,104,42,3,-1,1700000001);
        repair_test_gift(store,105,42,6,7,1700000003);
        // Missing protocol time must use the saved reception, never wall clock.
        repair_test_gift(store,106,43,1,-1,0);
    }
    GiftRepairSql sql(database.path());
    sql.exec("DELETE FROM event_presentations");
    sql.exec("UPDATE events SET body=json_set(body,'$.fans_club',json('{\"name\":\"manual repair\",\"level\":19,\"member\":true}'),'$.user_name','repaired profile')");
    sql.exec("UPDATE analytics_events SET fans_club='{\"name\":\"manual repair\",\"level\":19,\"member\":true}',user_name='repaired profile'");
    sql.exec("UPDATE message_views SET body=json_set(body,'$.fans_club',json('{\"name\":\"manual repair\",\"level\":19,\"member\":true}'))");
    sql.exec("UPDATE events SET body=json_set(body,'$.gift_unit_price',10) WHERE event_id='room-111:WebcastGiftMessage:102:0'");
    sql.exec("UPDATE analytics_events SET gift_unit_price=10 WHERE event_id='room-111:WebcastGiftMessage:102:0'");
    sql.exec("UPDATE events SET body=json_set(body,'$.gift_unit_price',0,'$.gift_count',5,'$.timestamp',1700000002000) WHERE event_id='room-111:WebcastGiftMessage:104:0'");
    sql.exec("UPDATE analytics_events SET gift_unit_price=0,occurred_at_ms=1700000002000 WHERE event_id='room-111:WebcastGiftMessage:104:0'");
    sql.exec("UPDATE events SET body=json_set(body,'$.timestamp',1700000002000) WHERE event_id='room-111:WebcastGiftMessage:105:0'");
    sql.exec("UPDATE analytics_events SET occurred_at_ms=1700000002000 WHERE event_id='room-111:WebcastGiftMessage:105:0'");
    sql.exec("INSERT INTO event_presentations VALUES('room-111:WebcastGiftMessage:105:0','{\"content\":\"retained display\",\"timestamp\":1700000100000}')");
    const auto before=sql.saved();const auto identities=sql.rows("SELECT id,event_id FROM events ORDER BY id");
    json before_stream,before_snapshot;
    {Store store;before_stream=store.frontend_batch("111",0);before_snapshot=store.snapshot("111");}
    const auto dry=repair_gift_facts(false);
    check(dry["failed"]==0 && dry["changed_groups"]==2 && dry["changed_events"]==4 && dry["skipped_groups"]==0,"Dry run finds all proven corrupt facts including missing protocol time");
    check(dry["backup"].is_null() && sql.saved()==before,"Dry run creates no backup and writes no rows");
    const auto applied=repair_gift_facts(true);
    check(applied["failed"]==0 && applied["applied"]==true,"Gift repair applies successfully");
    check(std::filesystem::exists(applied.at("backup").get<std::string>()),"Apply creates its backup before writing");
    GiftRepairSql backup(applied.at("backup").get<std::string>());check(backup.saved()==before,"Backup contains every original table before repair");
    const auto after=sql.saved();
    for(const char* table:{"event_details","analytics_gift_groups","message_views","gift_totals","rooms","receipts"})check(after[table]==before[table],std::string("Repair preserves ")+table);
    check(sql.rows("SELECT id,event_id FROM events ORDER BY id")==identities,"Event identities and sequence values stay fixed");
    check(sql.rows("SELECT gift_unit_price,gift_quantity,occurred_at_ms FROM analytics_events WHERE event_id='room-111:WebcastGiftMessage:102:0'")[0]==json::array({nullptr,"1","1700000001000"}),"Sparse price is unknown in analytics");
    check(sql.rows("SELECT gift_unit_price FROM analytics_events WHERE event_id='room-111:WebcastGiftMessage:103:0'")[0][0]=="0","Explicit zero remains a known price");
    check(sql.rows("SELECT json_extract(body,'$.gift_count'),json_extract(body,'$.timestamp') FROM events WHERE event_id='room-111:WebcastGiftMessage:104:0'")[0]==json::array({"3","1700000001000"}),"Late count and time are restored from this payload");
    check(sql.rows("SELECT occurred_at_ms FROM analytics_events WHERE event_id='room-111:WebcastGiftMessage:106:0'")[0][0]=="1700000200106","Missing protocol time uses saved receipt time");
    check(sql.rows("SELECT DISTINCT json_extract(body,'$.fans_club.level'),json_extract(body,'$.user_name') FROM events")[0]==json::array({"19","repaired profile"}),"Historical manual profile repair survives event changes");
    check(sql.rows("SELECT DISTINCT json_extract(fans_club,'$.level'),user_name FROM analytics_events")[0]==json::array({"19","repaired profile"}),"Analytics profile columns are preserved");
    {
        Store store;check(store.frontend_batch("111",0)==before_stream && store.snapshot("111")==before_snapshot,"Existing and reconstructed display overlays preserve WS and snapshots");
        const auto ranking=store.analytics_ranking(AnalyticsQuery{},true)["items"][0];
        check(ranking["known_gift_value"]=="17" && ranking["unknown_price_quantity"]=="2" && ranking["gift_quantity"]=="7","Repaired rankings agree with the original per-delivery price evidence");
    }
    const auto repeated=repair_gift_facts(true);
    check(repeated["failed"]==0 && repeated["changed_events"]==0 && repeated["changed_analytics"]==0 && sql.saved()==after,"Repeated apply is idempotent");
}

void gift_fact_repair_skips_incomplete_groups_and_rolls_back() {
    TemporaryDatabase database;
    {
        Store store;store.target("111","douyin",true);
        repair_test_gift(store,201,51,1,10,1700000000);repair_test_gift(store,202,51,2,-1,1700000001);
        repair_test_gift(store,203,52,1,10,1700000000);repair_test_gift(store,204,52,2,-1,1700000001);
        repair_test_gift(store,205,53,1,-1,1700000000);repair_test_gift(store,206,54,1,-1,1700000000);
    }
    GiftRepairSql sql(database.path());
    sql.exec("UPDATE events SET body=json_set(body,'$.gift_unit_price',99) WHERE event_id IN ('room-111:WebcastGiftMessage:202:0','room-111:WebcastGiftMessage:204:0','room-111:WebcastGiftMessage:205:0','room-111:WebcastGiftMessage:206:0')");
    sql.exec("UPDATE analytics_events SET gift_unit_price=99 WHERE event_id IN ('room-111:WebcastGiftMessage:202:0','room-111:WebcastGiftMessage:204:0','room-111:WebcastGiftMessage:205:0','room-111:WebcastGiftMessage:206:0')");
    sql.exec("DELETE FROM event_details WHERE event_id='room-111:WebcastGiftMessage:203:0'");
    sql.exec("UPDATE events SET body=json_set(body,'$.user_id','42') WHERE event_id='room-111:WebcastGiftMessage:205:0'");
    sql.exec("UPDATE event_details SET body=json_set(body,'$.payload_base64','invalid') WHERE event_id='room-111:WebcastGiftMessage:206:0'");
    const auto dry=repair_gift_facts(false);
    check(dry["failed"]==0 && dry["skipped_groups"]==3 && dry["skipped_events"]==4 && dry["changed_groups"]==1,"Missing evidence, conflicting identity and corrupt payload skip their entire groups");
    sql.exec("CREATE TRIGGER reject_gift_repair BEFORE UPDATE OF gift_unit_price ON analytics_events BEGIN SELECT RAISE(ABORT,'injected analytics failure'); END");
    const auto before=sql.saved();const auto failed=repair_gift_facts(true);
    check(failed["failed"]==1 && failed["applied"]==false && sql.saved()==before,"Projection failure rolls back all event and overlay writes");
    GiftRepairSql backup(failed.at("backup").get<std::string>());check(backup.saved()==before,"Failed repair still has a complete prior backup");
    sql.exec("DROP TRIGGER reject_gift_repair");
    const auto applied=repair_gift_facts(true);
    check(applied["failed"]==0 && applied["changed_events"]==1 && applied["skipped_groups"]==3,"Only the independently proven complete group is repaired");
    check(sql.rows("SELECT json_extract(body,'$.gift_unit_price') FROM events WHERE event_id IN ('room-111:WebcastGiftMessage:204:0','room-111:WebcastGiftMessage:205:0','room-111:WebcastGiftMessage:206:0') ORDER BY id")==json::array({json::array({"99"}),json::array({"99"}),json::array({"99"})}),"All skipped groups remain untouched");
}
