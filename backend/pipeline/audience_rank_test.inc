// Exercise the real Store projection, snapshot and streaming interfaces.
void audience_rank_delivery() {
    TemporaryDatabase database;
    Store store;store.target("111","douyin",true);store.target("222","douyin",true);
    auto online=gift("rank-online-1","rank-online-1",0);online["type"]="online_count";online["online_count"]=15;
    online["audience_ranks"]=json::array({{{"rank",1},{"user_id","42"},{"user_name","在线榜用户"},{"score",nullptr}}});
    deliver(store,"111",online);
    const auto before=store.stats("111");
    auto rank=gift("rank-extended-1","rank-extended-1",0);rank["type"]="audience_rank";rank["audience_rank_source"]="room_rank";
    rank["audience_ranks"]=json::array();rank["audience_ranks_total"]=130;
    for(int i=1;i<=130;++i)rank["audience_ranks"].push_back({{"rank",i},{"user_id",std::to_string(i)},
        {"user_name","扩展用户"+std::to_string(i)},{"score","9007199254740993"}});
    rank["audience_ranks"][1]["hidden"]=true;rank["audience_ranks"][1]["user_name"]="应保密的昵称";
    rank["audience_ranks"][1]["score_description"]="应保密的描述";rank["audience_ranks"][1]["exactly_score"]="应保密的精确值";
    deliver(store,"111",rank);
    rank["event_id"]="rank-foreign";rank["display_id"]="rank-foreign";rank["audience_ranks"][0]["user_name"]="外房观众";deliver(store,"222",rank);
    online["event_id"]="rank-online-2";online["display_id"]="rank-online-2";online["online_count"]=0;deliver(store,"111",online);
    const auto snapshot=store.snapshot("111"),batch=store.frontend_batch("111",0);
    check(snapshot["events"].empty() && batch["events"].empty(),"Audience ranks stay outside the behavior feed");
    check(snapshot["state_events"]==batch["state_events"] && snapshot["state_events"].size()==2,"Snapshot and batches keep both latest room-local states");
    check(snapshot["state_events"][0]["online_count"]==0,"Ranking messages cannot overwrite online totals, including zero");
    const auto& projected=snapshot["state_events"][1];
    check(projected["type"]=="audience_rank" && projected["method"]=="WebcastRoomRankMessage" && !projected.contains("online_count"),"The expanded ranking has an independent compact state");
    check(projected["audience_ranks"].size()==100 && projected["audience_ranks_total"]==130,"Projection is bounded while the observed total is preserved");
    check(projected["audience_ranks"][0]["score"]=="9007199254740993" && projected["audience_ranks"][0]["user_name"]=="扩展用户1","Precision and room isolation survive projection");
    check(projected["audience_ranks"][1]["user_id"]=="" && projected["audience_ranks"][1]["user_name"]=="" && projected["audience_ranks"][1]["score"].is_null() && !projected["audience_ranks"][1].contains("score_description") && !projected["audience_ranks"][1].contains("exactly_score"),"Hidden identities and contributions cannot cross the presentation interface");
    check(batch["through_seq"]==snapshot["through_seq"],"A ranking-only batch advances the complete journal cursor");
    const auto after=store.stats("111");
    for(const char* field:{"chat","gift","enter","like"})check(before[field]==after[field],"Ranking snapshots must not add audience behavior counts");
    MessageSearch query;query.room="111";check(store.search_messages(query)["total"]==0,"Ranking snapshots cannot enter user history");
    rank["event_id"]="rank-empty";rank["display_id"]="rank-empty";rank["audience_ranks"]=json::array();rank["audience_ranks_total"]=0;deliver(store,"111",rank);
    const auto empty=store.frontend_batch("111",std::stoll(batch["through_seq"].get<std::string>()));
    check(empty["state_events"].size()==1 && empty["state_events"][0]["audience_ranks"].empty() && empty["state_events"][0]["audience_ranks_total"]==0,"An explicit empty snapshot clears a previous ranking");
    check(store.snapshot("111")["state_events"][1]==empty["state_events"][0],"Reconnect preserves the latest empty ranking");
}
