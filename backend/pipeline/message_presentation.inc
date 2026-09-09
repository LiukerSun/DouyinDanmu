// Collection is lossless. Presentation is opt-in: a decoded schema alone does
// not establish an audience action. Keep configuration/unknown payloads in the
// journal and event_details, never in the ordinary feed or history catalogue.
static bool feed_visible(const json& event) {
    const auto type=event.value("type","");
    if(event.value("parse_status","")=="failed")return false;
    for(const char* behavior:{"chat","gift","enter","like","social","follow","share","emoji","fansclub","episode_chat","audio_chat","screen_chat","system"})
        if(type==behavior)return true;
    const auto method=event.value("method","");
    const auto content=event.value("content","");
    if(type=="room_notice" && (method.empty() || method=="WebcastRoomMessage"))
        return content.find_first_not_of(" \t\r\n\f\v")!=std::string::npos && content!="房间通知";
    if(type=="notice" && (method.empty() || method=="WebcastCommonTextMessage" || method=="WebcastNotifyMessage"))
        return content.find_first_not_of(" \t\r\n\f\v")!=std::string::npos && content!="公共文本" && content!="通知";
    return false;
}

static void sqlite_feed_visible(sqlite3_context* context,int,sqlite3_value** values) noexcept {
    try {
        const auto* body=sqlite3_value_text(values[0]);
        sqlite3_result_int(context,body && feed_visible(json::parse(reinterpret_cast<const char*>(body))));
    } catch(...) { sqlite3_result_int(context,0); }
}

// Only the metric used by the room header crosses the wire. Protocol metadata,
// raw payloads and repeated stream/ranking/configuration messages stay server-side.
static json projected_room_state(const json& event) {
    if(event.value("type","")!="online_count" || !event.contains("online_count"))return nullptr;
    return {{"seq",event.at("seq")},{"live_id",event.at("live_id")},
        {"type","online_count"},{"method","WebcastRoomUserSeqMessage"},
        {"online_count",event.at("online_count")},{"received_at_ms",event.value("received_at_ms",int64_t(0))}};
}
