#include <algorithm>
#include <limits>

static std::string inflate_frame(const std::string& input) {
    if(input.size()<2 || (unsigned char)input[0]!=0x1f || (unsigned char)input[1]!=0x8b) return input;
    z_stream stream{}; stream.next_in=(Bytef*)input.data(); stream.avail_in=input.size();
    if(inflateInit2(&stream,16+MAX_WBITS)!=Z_OK) throw std::runtime_error("gzip init failed");
    std::string result; char chunk[16384]; int rc=Z_OK;
    while(rc==Z_OK) { stream.next_out=(Bytef*)chunk; stream.avail_out=sizeof(chunk); rc=inflate(&stream,Z_NO_FLUSH); result.append(chunk,sizeof(chunk)-stream.avail_out); if(result.size()>16*1024*1024) { inflateEnd(&stream); throw std::runtime_error("decompressed frame exceeds 16 MB"); } }
    inflateEnd(&stream); if(rc!=Z_STREAM_END) throw std::runtime_error("invalid gzip frame"); return result;
}
template<class T> static T decode(const std::string& b) { T m; if(!m.ParseFromString(b)) throw std::runtime_error("invalid protobuf payload"); return m; }
static std::string precise_user_id(const User& user) {
    if(user.id()!=0)return std::to_string(user.id());
    const auto& value=user.idstr();
    if(!value.empty() && value.find_first_not_of("0123456789")==std::string::npos && value.find_first_not_of('0')!=std::string::npos)return value;
    return "";
}
static void user_fields(json& e, const User& user, const Common& common) {
    e["user_id"]=precise_user_id(user); e["user_name"]=user.nickname();
    e["user_level"]=nullptr;
    if(user.has_paygrade() && user.paygrade().has_level() && user.paygrade().level()>=0)e["user_level"]=user.paygrade().level();
    json fans={{"member",nullptr},{"level",nullptr},{"name",nullptr},{"status",nullptr},{"member_inferred",false}};
    if(user.has_fansclub() && user.fansclub().has_data()) {
        const auto& data=user.fansclub().data();
        if(data.has_level() && data.level()>=0)fans["level"]=data.level();
        if(!data.clubname().empty())fans["name"]=data.clubname();
        if(data.has_userfansclubstatus()) {
            const auto status=data.userfansclubstatus(); fans["status"]=status;
            // 0/1 is an application inference from observed status values and
            // third-party references, not a complete official Douyin enum.
            if(status==0 || status==1) { fans["member"]=status==1; fans["member_inferred"]=true; }
        }
        fans["anchor_id"]=data.anchorid()>0?json(std::to_string(data.anchorid())):json(nullptr);
    }
    e["fans_club"]=std::move(fans);
    auto t=common.create_time(); e["timestamp"]=t>0 ? (t<100000000000LL ? t*1000 : t) : now_ms();
}
static void gift_fields(json& e, const GiftMessage& gift, const std::string& room_id) {
    const auto count=std::max({gift.combo_count(),gift.repeat_count(),uint64_t(1)});
    if(count>uint64_t(std::numeric_limits<int64_t>::max()))throw std::runtime_error("gift count exceeds signed 64-bit storage limit");
    const auto gift_id=std::to_string(gift.gift_id()!=0?gift.gift_id():gift.gift().id());
    const auto group_id=std::to_string(gift.group_id());
    auto recipient=precise_user_id(gift.to_user());
    if(recipient.empty() && gift.to_user_ids_size()==1 && gift.to_user_ids(0)!=0)recipient=std::to_string(gift.to_user_ids(0));
    const auto sender=e.at("user_id").get<std::string>();
    const auto name=gift.gift().name().empty()?"礼物 "+gift_id:gift.gift().name();
    e["type"]="gift"; e["content"]=name; e["gift_name"]=name;
    e["gift_count"]=int64_t(count);
    // Presence matters: an omitted price is unknown, not a free gift.
    e["gift_unit_price"]=gift.gift().has_diamondcount()?json(gift.gift().diamondcount()):json(nullptr);
    e["gift_combo"]=gift.gift().combo();
    e["gift_final"]=!gift.gift().combo() || gift.repeat_end()==1;
    e["gift_id"]=gift_id; e["group_id"]=group_id; e["recipient_id"]=recipient;
    // Store resolves recipient omissions only against one compatible group.
    if(!room_id.empty() && !sender.empty() && gift_id!="0" && gift.group_id()!=0)
        e["gift_group_key"]="gift-group:"+digest(json::array({room_id,sender,gift_id,group_id}).dump());
    e["recipient_ids"]=json::array();
    for(const auto id:gift.to_user_ids())e["recipient_ids"].push_back(std::to_string(id));
    e["group_count"]=gift.group_count(); e["repeat_count"]=gift.repeat_count();
    e["combo_count"]=gift.combo_count(); e["repeat_end"]=gift.repeat_end();
    e["total_count"]=gift.total_count(); e["fan_ticket_count"]=gift.fan_ticket_count();
    e["gift_raw_counts"]={{"group_count",std::to_string(gift.group_count())},{"repeat_count",std::to_string(gift.repeat_count())},
        {"combo_count",std::to_string(gift.combo_count())},{"repeat_end",std::to_string(gift.repeat_end())},
        {"total_count",std::to_string(gift.total_count())},{"fan_ticket_count",std::to_string(gift.fan_ticket_count())}};
    // The gift metadata's combo flag is not group identity: high-value gifts
    // can report cumulative repeat counts with combo=false. Require the room,
    // gift, group and both participants, while preserving the observed flag.
    // JSON array encoding prevents delimiter collisions between identity parts.
    if(!room_id.empty() && gift_id!="0" && gift.group_id()!=0 && !sender.empty() && !recipient.empty())
        e["display_id"]="gift:"+digest(json::array({room_id,sender,recipient,gift_id,group_id}).dump());
}
#include "message_monitor.inc"

static std::pair<json,json> parse(const pipeline::RawFrameEnvelope& envelope) {
    json events=json::array(), errors=json::array();
    if(envelope.kind()=="collector_status") { auto status=json::parse(envelope.payload()); if(!status.is_object())throw std::runtime_error("invalid collector status"); return {events,errors}; }
    if(envelope.kind()=="room_metadata") {
        auto metadata=json::parse(envelope.payload());
        const auto state=metadata.at("live_status").get<std::string>();
        if(state!="live" && state!="offline" && state!="unknown")throw std::runtime_error("invalid live status");
        for(const auto* field:{"room_id","title"}) { if(!metadata.at(field).is_string())throw std::runtime_error("invalid room metadata field"); }
        const auto& anchor=metadata.at("anchor");
        for(const auto* field:{"id","sec_uid","nickname","avatar_url","signature","display_id"}) { if(!anchor.at(field).is_string())throw std::runtime_error("invalid anchor metadata field"); }
        if(!metadata.at("checked_at_ms").is_number_integer())throw std::runtime_error("invalid metadata observation time");
        const auto checked=metadata.at("checked_at_ms").get<int64_t>();
        if(checked<=0 || checked>now_ms()+60000)throw std::runtime_error("invalid metadata observation time");
        if(!metadata.at("live_started_at_ms").is_null() && (!metadata.at("live_started_at_ms").is_number_integer() || metadata.at("live_started_at_ms").get<int64_t>()<=0))throw std::runtime_error("invalid live start time");
        return {events,errors};
    }
    auto frame=decode<PushFrame>(envelope.payload());
    if(frame.payloadtype()=="hb" || frame.payloadtype()=="ack") return {events,errors};
    auto response=decode<Response>(inflate_frame(frame.payload())); int index=0;
    for(const auto& message:response.messageslist()) {
        auto key=message.msgid()!=0 ? envelope.room_id()+":"+message.method()+":"+std::to_string(message.msgid())+":"+std::to_string(message.wrdsversion()) : envelope.frame_id()+":"+std::to_string(index);
        ++index;
        auto e=observed_event(key,message.method(),message.payload(),envelope.received_at_ms());
        e["_details"]["msg_id"]=std::to_string(message.msgid());
        e["_details"]["wrds_version"]=std::to_string(message.wrdsversion());
        e["_details"]["frame_id"]=envelope.frame_id();
        try {
            decode_observed(e,message.payload());
            if(message.method()=="WebcastChatMessage") { auto m=decode<ChatMessage>(message.payload()); user_fields(e,m.user(),m.common()); e["type"]="chat"; e["content"]=m.content(); }
            else if(message.method()=="WebcastGiftMessage") { auto m=decode<GiftMessage>(message.payload()); user_fields(e,m.user(),m.common()); gift_fields(e,m,envelope.room_id()); }
            else if(message.method()=="WebcastMemberMessage") { auto m=decode<MemberMessage>(message.payload()); user_fields(e,m.user(),m.common()); if(e["user_id"]=="" && m.user_id()>0)e["user_id"]=std::to_string(m.user_id()); e["type"]="enter"; e["content"]="进入直播间"; }
            else if(message.method()=="WebcastLikeMessage") { auto m=decode<LikeMessage>(message.payload()); user_fields(e,m.user(),m.common()); e["type"]="like"; e["content"]="为主播点赞"; e["like_count"]=m.count(); }
            else if(message.method()=="WebcastSocialMessage") {
                auto m=decode<SocialMessage>(message.payload()); user_fields(e,m.user(),m.common());
                const auto& display=m.common().display_text();
                const auto& pattern=display.default_pattern();
                // Display text identifies social actions; action 1 is the follow fallback.
                const bool share=display.key().find("room_share") == 0 || pattern.find("分享")!=std::string::npos;
                const bool follow=display.key()=="room_follow_msg" || (display.key().empty() && pattern.empty() && m.action()==1);
                e["type"]="social";e["action"]=m.action();e["social_action"]=share?"share":follow?"follow":"unknown";
                e["content"]=share?"分享了直播间":follow?"关注了主播":"其他互动（动作 "+std::to_string(m.action())+"）";
                e["action_evidence"]=display.key();e["share_type"]=m.sharetype();
            }
            else if(message.method()=="WebcastRoomUserSeqMessage") {
                auto m=decode<RoomUserSeqMessage>(message.payload());
                e["type"]="online_count"; e["online_count"]=m.total(); e["content"]="在线人数更新";
                // The server ships the room's contribution ranking with every online
                // update. Persist the observed entries; hidden identities stay marked.
                json ranks=json::array();
                for(const auto& contributor:m.rankslist()) {
                    if(ranks.size()>=20)break;
                    ranks.push_back({{"rank",contributor.rank()},{"user_id",precise_user_id(contributor.user())},
                        {"user_name",contributor.user().nickname()},{"score",contributor.score()}});
                    if(contributor.ishidden())ranks.back()["hidden"]=true;
                }
                if(!ranks.empty())e["audience_ranks"]=ranks;
            }
            else if(message.method()=="WebcastControlMessage") { auto m=decode<ControlMessage>(message.payload()); e["type"]="system"; e["control_status"]=m.status(); e["content"]=m.status()==3?"直播已结束":"直播状态更新"; }
        } catch(const std::exception& failure) {
            errors.push_back(message.method()+": "+failure.what());
            e["type"]="parse_error";e["parse_status"]="failed";
            e["content"]="解析失败："+message.method();e["_details"]["error"]=failure.what();
        }
        events.push_back(e);
    }
    return {events,errors};
}
