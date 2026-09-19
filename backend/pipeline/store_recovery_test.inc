// Regressions exercise production Store transactions, projections and cursors.
void gift_delivery_facts_stay_separate_from_display() {
    TemporaryDatabase database;Store store;store.target("111","douyin",true);
    auto first=gift("fact-first","fact-combo",1);first["timestamp"]=1000;first["gift_unit_price"]=10;
    deliver(store,"111",first);
    auto sparse=first;sparse["event_id"]="fact-sparse";sparse["gift_count"]=2;sparse["timestamp"]=2000;sparse["gift_unit_price"]=nullptr;
    deliver(store,"111",sparse);
    auto ranking=store.analytics_ranking(AnalyticsQuery{},true)["items"][0];
    check(ranking["known_gift_value"]=="10" && ranking["unknown_price_quantity"]=="1" && ranking["value_complete"]==false,
          "Sparse delivery adds unknown-priced quantity, never an inherited display price");
    check(store.events("111",0).back()["gift_unit_price"].is_null(),"Delivery history keeps an omitted price unknown");
    check(store.frontend_batch("111",0)["events"].back()["gift_unit_price"]==10 && store.snapshot("111")["events"][0]["gift_unit_price"]==10,
          "Live and snapshot presentations retain the established display price");
    auto final=sparse;final["event_id"]="fact-final";final["gift_count"]=5;final["gift_final"]=true;final["timestamp"]=4000;final["gift_unit_price"]=10;final["user_name"]="terminal name";
    deliver(store,"111",final);
    auto late=first;late["event_id"]="fact-late";late["gift_count"]=3;late["timestamp"]=1500;late["user_name"]="original late name";
    deliver(store,"111",late);
    auto continuation=late;continuation["event_id"]="fact-continuation";continuation["gift_count"]=6;continuation["timestamp"]=6000;
    deliver(store,"111",continuation);
    const auto history=store.events("111",0);
    check(history[3]["timestamp"]==1500 && history[3]["gift_count"]==3 && history[3]["user_name"]=="original late name",
          "Late delivery preserves its own time, observed quantity and profile");
    check(history[4]["timestamp"]==6000 && history[4]["gift_final"]==false,"New progress after terminal preserves its delivery facts");
    AnalyticsQuery interval;interval.scope.from_ms=6000;interval.scope.to_ms=7000;
    auto items=store.analytics_ranking(interval,true)["items"];
    check(items.size()==1 && items[0]["gift_quantity"]=="1" && items[0]["known_gift_value"]=="10","New terminal-following increment belongs to its actual observation window");
    interval.scope.from_ms=1000;interval.scope.to_ms=2000;
    check(store.analytics_user_summary(interval,uid)["event_count"]=="2","Time-filtered history includes the late original delivery");
    const auto visible=store.frontend_batch("111",0)["events"].back();
    check(visible["gift_final"]==true && visible["gift_count"]==6 && visible["user_name"]=="terminal name", "Live presentation preserves terminal richness and high-water quantity");
    Store reopened;
    check(reopened.events("111",0)==history && reopened.frontend_batch("111",0)["events"].back()==visible,"Facts and per-delivery presentation survive restart");
}

void replay_promotes_failed_placeholders_atomically() {
    TemporaryDatabase database;Store store;store.target("111","douyin",true);
    auto repaired=gift("repair-chat","repair-chat",0);repaired["type"]="chat";repaired["method"]="WebcastChatMessage";
    repaired["content"]="Recovered chat";repaired["parse_status"]="decoded";repaired["has_details"]=true;repaired["payload_bytes"]=3;repaired["parser_version"]=event_parser_version;
    repaired["_details"]={{"payload_base64","CAE="},{"decoded",{{"content","Recovered chat"}}}};
    const auto envelope=frame("111",repaired);const auto raw=envelope.SerializeAsString();
    auto failed=repaired;failed["type"]="parse_error";failed["parse_status"]="failed";failed["content"]="Old parser failure";
    failed["_details"]={{"payload_base64","CAE="},{"error","Old parser failure"}};
    store.commit(envelope,raw,json::array({failed}),json::array({"old parse error"}));
    const auto cursor=std::stoll(store.snapshot("111")["through_seq"].get<std::string>());
    check(store.snapshot("111")["events"].empty() && store.overview()["quarantine"]==1,"Failed placeholder remains hidden and quarantined");
    auto invalid=repaired;invalid["type"]="gift";invalid["gift_count"]=0;
    bool rejected=false;try{store.commit(envelope,raw,json::array({invalid}),json::array(),true);}catch(...){rejected=true;}
    check(rejected && store.message_detail("repair-chat")["event"]["type"]=="parse_error" && store.overview()["quarantine"]==1,
          "Failed recovery rolls back placeholder, details and quarantine resolution together");
    store.commit(envelope,raw,json::array({repaired}),json::array(),true);
    const auto detail=store.message_detail("repair-chat"),stats=store.stats("111"),snapshot=store.snapshot("111");
    check(detail["event"]["type"]=="chat" && detail["details"]["decoded"]["content"]=="Recovered chat" && !detail["details"].contains("error"),"Recovery replaces failed content and diagnostics");
    check(snapshot["events"].size()==1 && stats["chat"]==1 && stats["total"]==1,"Recovery replaces the placeholder logical count and adds the business count once");
    check(store.analytics_ranking(AnalyticsQuery{},false)["items"][0]["chat_count"]=="1" && store.overview()["quarantine"]==0,"Recovery projects statistics before resolving quarantine");
    check(store.frontend_batch("111",cursor)["events"].size()==1,"Clients beyond the failed cursor receive the recovered message");
    store.commit(envelope,raw,json::array({repaired}),json::array(),true);
    check(store.stats("111")==stats && store.snapshot("111")==snapshot && store.events("111",0).size()==1,"Repeated recovery does not recount or move its cursor again");
}

void heartbeat_reports_backpressure_without_spool() {
    TemporaryDatabase database;Store store;store.target("111","douyin",true);
    const auto observed=now_ms();
    json state={{"live_id","111"},{"desired_version",1},{"session_id","heartbeat-session"},{"session_seq","2"},{"observed_at_ms",observed},{"status","backpressured"},{"detail","buffer full"}};
    store.heartbeat({{"room_states",json::array({state})}});
    check(store.rooms()[0]["status"]=="backpressured","Independent heartbeat reports spool backpressure");
    auto older=state;older["session_seq"]="1";older["status"]="collecting";
    store.heartbeat({{"room_states",json::array({older})}});
    check(store.rooms()[0]["status"]=="backpressured","Old queued status cannot hide heartbeat backpressure");
    auto data=frame("111",json::object());data.set_session_id("heartbeat-session");data.set_session_seq(3);data.set_received_at_ms(observed+1);
    store.commit(data,data.SerializeAsString(),json::array(),json::array());
    check(store.rooms()[0]["status"]=="collecting","New persisted data clears backpressure");
    store.target("111","douyin",false);state["session_seq"]="4";store.heartbeat({{"room_states",json::array({state})}});
    check(store.rooms()[0]["status"]=="stopped","Heartbeat cannot restart a disabled target");
    store.target("111","douyin",true);state["session_seq"]="5";store.heartbeat({{"room_states",json::array({state})}});
    check(store.rooms()[0]["status"]=="connecting","Heartbeat from old desired version cannot affect restarted target");
}
