// Shared isolated database/delivery fixtures are defined in gift_store_test.cpp.
void audience_analytics() {
    TemporaryDatabase database;
    json saved_stats,saved_journal,saved_result;
    {
        Store store;store.target("111","douyin",true);store.target("222","douyin",true);store.target("demo","demo",true);
        auto message=[&](const std::string& id,const std::string& user,const std::string& type,int64_t time,const std::string& room="111") {
            auto e=gift(id,id,0);e["type"]=type;e["user_id"]=user;e["timestamp"]=time;e["user_name"]="用户 "+user;
            deliver(store,room,e);return e;
        };
        const auto original=message("chat-one",uid,"chat",1000);
        deliver(store,"111",original); // Duplicate event through a different frame.
        message("chat-two",uid,"emoji",1100);
        message("chat-three","2222","chat",1200);
        message("not-a-chat",uid,"like",1250);
        message("not-a-normal-chat",uid,"audio_chat",1260);
        message("room-two",uid,"chat",1400,"222");
        message("demo-chat",uid,"chat",1400,"demo");
        message("anonymous","","chat",1400);
        auto failed=original;failed["event_id"]="failed-chat";failed["parse_status"]="failed";deliver(store,"111",failed);
        auto progress=gift("progress-1","streak",1);progress["gift_unit_price"]=10;progress["timestamp"]=1000;progress["recipient_id"]="3333";
        auto frame_one=deliver(store,"111",progress);
        progress["event_id"]="progress-2";progress["gift_count"]=2;progress["timestamp"]=2000;deliver(store,"111",progress);
        progress["event_id"]="progress-5";progress["gift_count"]=5;progress["timestamp"]=2100;deliver(store,"111",progress);
        progress["event_id"]="progress-final";progress["gift_final"]=true;progress["timestamp"]=2200;progress["gift_unit_price"]=nullptr;deliver(store,"111",progress);
        progress["event_id"]="progress-late";progress["gift_count"]=3;progress["timestamp"]=1500;progress["gift_unit_price"]=10;deliver(store,"111",progress);
        auto unknown=gift("unpriced","unpriced",2);unknown["timestamp"]=2300;deliver(store,"111",unknown);
        auto free=gift("free","free",1);free["timestamp"]=2400;free["gift_unit_price"]=0;deliver(store,"111",free);
        auto second=gift("second-recipient","second-recipient",1);second["timestamp"]=2400;second["gift_unit_price"]=20;second["recipient_id"]="4444";deliver(store,"111",second);
        AnalyticsQuery all;
        auto chat=store.analytics_ranking(all,false);
        check(chat["total"]=="2" && chat["items"][0]["user_id"]==uid && chat["items"][0]["chat_count"]=="3","Chat ranking deduplicates and excludes notifications, special chat, demo and missing UID");
        check(chat["meta"]["unattributed_event_count"]=="1","Unknown identities must be disclosed without forming one fake user");
        auto gifts=store.analytics_ranking(all,true);
        check(gifts["items"][0]["gift_quantity"]=="9" && gifts["items"][0]["known_gift_value"]=="70","Gift ranking sums combo increments and keeps recipients separate");
        check(gifts["items"][0]["unknown_price_quantity"]=="2" && gifts["items"][0]["value_complete"]==false,"Omitted unit price is unknown; explicit zero is known");
        AnalyticsQuery window;window.scope.room="111";window.scope.from_ms=2000;window.scope.to_ms=2200;
        auto interval=store.analytics_ranking(window,true);
        check(interval["items"][0]["gift_quantity"]=="4" && interval["items"][0]["known_gift_value"]=="40","Half-open time filtering runs after full-history combo high-water accounting");
        auto summary=store.analytics_user_summary(all,uid);
        check(summary["chat_count"]=="3" && summary["gift_quantity"]=="9" && summary["room_count"]=="2","User summary and rankings share the same counting policy");
        check(store.analytics_user_summary(all,"9999")["event_count"]=="0","Unknown user returns an empty collected history");
        auto rooms=store.analytics_user_rooms(all,uid);
        check(rooms["total"]=="2" && rooms["items"][0]["live_id"]=="111" && rooms["items"][0]["known_gift_value"]=="70","User-room breakdown reconciles with aggregate");
        all.scope.limit=1;auto page=store.analytics_ranking(all,false);all.scope.as_of_seq=std::stoll(page["meta"]["as_of_seq"].get<std::string>());
        message("new-chat-1","2222","chat",3000);message("new-chat-2","2222","chat",3001);message("new-chat-3","2222","chat",3002);
        all.offset=1;auto next=store.analytics_ranking(all,false);
        check(next["items"][0]["user_id"]=="2222" && next["items"][0]["chat_count"]=="1" && next["items"][0]["rank"]==2,"Frozen sequence and stable tie-breaker keep pagination stable under ingestion");
        MessageSearch history;history.user=uid;history.deliveries=true;history.from_ms=2000;history.to_ms=2200;history.as_of_seq=all.scope.as_of_seq;
        check(store.search_messages(history)["total"]==2,"History drilldown uses the same time bounds and snapshot");
        saved_stats=store.stats("111");saved_journal=store.events("111",0);saved_result=store.analytics_ranking(AnalyticsQuery{},true);
        store.commit(frame_one,frame_one.SerializeAsString(),json::array({gift("progress-1","streak",1)}),json::array(),true);
        check(store.analytics_ranking(AnalyticsQuery{},true)==saved_result,"Replay must not duplicate statistics");
    }
    {
        Store reopened;
        check(reopened.stats("111")==saved_stats && reopened.events("111",0)==saved_journal,"Analytics and restart preserve source records, counters and cursors");
        check(reopened.analytics_ranking(AnalyticsQuery{},true)==saved_result,"Statistics persist through restart without double counting");
    }
}

void analytics_backfill_and_precision() {
    TemporaryDatabase database;json original_stats,original_events;
    {
        Store store;store.target("111","douyin",true);
        auto e=gift("legacy-1","legacy-combo",1);e["timestamp"]=1000;
        e["_details"]={{"decoded",{{"gift",{{"diamondCount",7}}}}}};
        deliver(store,"111",e);e["event_id"]="legacy-3";e["gift_count"]=3;e["timestamp"]=2000;deliver(store,"111",e);
        original_stats=store.stats("111");original_events=store.events("111",0);
    }
    // Simulate a database created before the statistics projection existed.
    sqlite3* raw=nullptr;check(sqlite3_open(database.path().string().c_str(),&raw)==SQLITE_OK,"Open migration fixture");
    check(sqlite3_exec(raw,"DROP TABLE analytics_events; DROP TABLE analytics_gift_groups; DELETE FROM schema_migrations WHERE name='analytics_v1'",nullptr,nullptr,nullptr)==SQLITE_OK,"Prepare pre-statistics database");sqlite3_close(raw);
    {
        Store store;AnalyticsQuery query;
        check(store.analytics_ranking(query,true)["items"][0]["known_gift_value"]=="21","Backfill recovers historical price from saved decoded payload and counts each combo once");
        check(store.stats("111")==original_stats && store.events("111",0)==original_events,"Backfill only writes its own projection");
        auto huge=gift("huge","huge",9007199254740993LL);huge["user_id"]="12345";huge["gift_unit_price"]=uint64_t(UINT32_MAX);deliver(store,"111",huge);
        const auto result=store.analytics_ranking(query,true);
        check(result["items"][0]["known_gift_value"]=="38685626218660938630823935","Multiplication and aggregation above int64 stay exact and sort numerically");
        query.sort="quantity";check(store.analytics_ranking(query,true)["items"][0]["user_id"]=="12345","Quantity sorting is numerical, not lexical");
    }
    Store again;
    check(again.analytics_user_summary(AnalyticsQuery{},uid)["known_gift_value"]=="21","Migration marker prevents repeat backfill");
}

void analytics_query_validation() {
    for(const auto& parameters:std::vector<httplib::Params>{
        {{"from_ms","200"},{"to_ms","100"}},{{"from_ms","-1"}},{{"as_of_seq","9223372036854775808"}},
        {{"limit","101"}},{{"offset","1000001"}},{{"offset","1.5"}},{{"sort","price desc;"}},{{"room","x'"}},{{"view","events"}},{{"q","nickname"}}}) {
        httplib::Request request;request.params=parameters;bool rejected=false;
        try{read_analytics_query(request);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Invalid analytics parameters must be rejected, not silently change scope");
    }
    httplib::Request request;request.params={{"from_ms","0"},{"to_ms","2000"},{"as_of_seq","0"},{"limit","1"},{"offset","0"}};
    check(read_analytics_query(request).scope.as_of_seq==0,"Explicit zero snapshot is distinct from unspecified");
}
