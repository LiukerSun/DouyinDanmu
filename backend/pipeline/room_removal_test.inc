// Exercise target removal against production Store transactions and history APIs.
static json removal_room(Store& store,const std::string& live) {
    for(const auto& room:store.rooms())if(room.at("live_id")==live)return room;
    return nullptr;
}

void room_removal_preserves_history_and_readd_versions() {
    TemporaryDatabase database;
    json snapshot,events,detail,ranking,metadata,other;
    int64_t original_version=0;
    {
        Store store;store.target("111","douyin",true);store.target("222","douyin",true);
        auto event=gift("remove-gift","remove-gift",3);event["gift_unit_price"]=10;
        event["_details"]={{"payload_base64","CAE="},{"decoded",{{"name","original payload"}}}};
        const auto gift_frame=deliver(store,"111",event);
        auto chat=gift("remove-chat","remove-chat",0);chat["type"]="chat";chat["content"]="Retained history";
        const auto chat_frame=deliver(store,"111",chat);
        auto meta=frame("111",json::object());meta.set_kind("room_metadata");
        metadata={{"checked_at_ms",now_ms()},{"title","Retained room metadata"}};meta.set_payload(metadata.dump());
        store.commit(meta,meta.SerializeAsString(),json::array(),json::array());
        snapshot=store.snapshot("111");events=store.events("111",0);detail=store.message_detail("remove-gift");
        ranking=store.analytics_ranking(AnalyticsQuery{},true);other=removal_room(store,"222");
        original_version=removal_room(store,"111").at("version");
        check(store.remove_target("111"),"Known room removal succeeds");
        check(removal_room(store,"111").is_null() && store.rooms().size()==1,"Removed room disappears while the other room remains");
        check(store.remove_target("111"),"Repeated removal is successful and idempotent");
        check(!store.remove_target("999") && store.rooms().size()==1,"Unknown removal does not create a room");
        check(store.snapshot("111")==snapshot && store.events("111",0)==events && store.message_detail("remove-gift")==detail,
              "Removing a target preserves messages, presentation, details, counters and sequence cursors");
        check(store.analytics_ranking(AnalyticsQuery{},true)==ranking && store.analytics_user_summary(AnalyticsQuery{},uid)["chat_count"]=="1",
              "Removed-room facts remain in gift and chat statistics");
        MessageSearch search;search.room="111";
        check(store.search_messages(search)["total"]==2,"Removed-room history remains searchable");
        check(store.receipts(json::array({gift_frame.frame_id(),chat_frame.frame_id()})).size()==2,"Removal preserves delivery receipts");
        check(removal_room(store,"222")==other,"Removal must leave other rooms untouched");
    }
    {
        Store reopened;
        check(removal_room(reopened,"111").is_null(),"Removal persists across restart");
        check(reopened.snapshot("111")==snapshot && reopened.events("111",0)==events && reopened.analytics_ranking(AnalyticsQuery{},true)==ranking,
              "Restart keeps removed-room history and statistics intact");
        reopened.target("111","douyin",true);
        const auto room=removal_room(reopened,"111");
        check(room["enabled"]==true && room["status"]=="connecting" && room["version"]==original_version+2,
              "Re-add restores membership with a monotonically increasing version; duplicate remove does not increase it twice");
        check(room["metadata"]==metadata && reopened.snapshot("111")==snapshot && reopened.message_detail("remove-gift")==detail,
              "Re-add retains metadata and original history");
        reopened.target("111","douyin",false);
        check(removal_room(reopened,"111")["status"]=="stopped","Pause keeps a re-added room visible");
        check(reopened.remove_target("111"),"A paused room can also be removed");
        reopened.target("111","douyin",false);
        check(removal_room(reopened,"111").is_null(),"A late pause request cannot restore a removed room");
    }
}

void room_removal_rejects_late_state_without_losing_buffered_events() {
    TemporaryDatabase database;Store store;store.target("111","douyin",true);
    const auto observed=now_ms();
    auto status=frame("111",json::object());status.set_kind("collector_status");status.set_received_at_ms(observed);
    status.set_payload(json{{"status","collecting"},{"detail","old session"}}.dump());
    store.commit(status,status.SerializeAsString(),json::array(),json::array());
    store.remove_target("111");
    auto late=gift("removed-buffered","removed-buffered",0);late["type"]="chat";
    auto upstream=frame("111",late);upstream.set_received_at_ms(observed+1);deliver(store,upstream,late);
    status.set_frame_id("removed-old-status");status.set_session_seq(upstream.session_seq()+1);status.set_received_at_ms(observed+2);
    store.commit(status,status.SerializeAsString(),json::array(),json::array());
    auto metadata=frame("111",json::object());metadata.set_kind("room_metadata");metadata.set_received_at_ms(observed+3);
    metadata.set_payload(json{{"checked_at_ms",observed+3},{"title","obsolete metadata"}}.dump());
    store.commit(metadata,metadata.SerializeAsString(),json::array(),json::array());
    json heartbeat={{"live_id","111"},{"desired_version",1},{"session_id",status.session_id()},{"session_seq","10000"},
                    {"observed_at_ms",observed+4},{"status","collecting"},{"detail","obsolete heartbeat"}};
    store.heartbeat({{"room_states",json::array({heartbeat})}});
    check(store.rooms().empty(),"Late upstream, state, metadata and heartbeat cannot restore removed membership");
    check(store.stats("111")["chat"]==1 && store.events("111",0).size()==1 && store.analytics_user_summary(AnalyticsQuery{},uid)["chat_count"]=="1",
          "Already buffered messages still persist exactly once into removed-room history and statistics");
    store.target("111","douyin",true);
    const auto active=removal_room(store,"111");
    check(active["version"]==3 && active["status"]=="connecting" && active["metadata"].is_null(),"Re-add rejects metadata from the removed version");
    status.set_frame_id("readded-old-status");status.set_session_seq(status.session_seq()+1);
    store.commit(status,status.SerializeAsString(),json::array(),json::array());
    metadata.set_frame_id("readded-old-metadata");store.commit(metadata,metadata.SerializeAsString(),json::array(),json::array());
    upstream.set_frame_id("readded-old-data");store.commit(upstream,upstream.SerializeAsString(),json::array(),json::array());
    store.heartbeat({{"room_states",json::array({heartbeat})}});
    check(removal_room(store,"111")["status"]=="connecting" && removal_room(store,"111")["metadata"].is_null(),
          "Old target-version deliveries cannot affect a re-added room even if they arrive last");
    heartbeat["desired_version"]=3;heartbeat["session_id"]="new-session";heartbeat["session_seq"]="1";
    store.heartbeat({{"room_states",json::array({heartbeat})}});
    check(removal_room(store,"111")["status"]=="collecting","Current-version state still works after re-add");
}
