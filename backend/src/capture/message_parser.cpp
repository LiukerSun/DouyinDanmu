#include "capture/message_parser.h"
#include "models/message.h"
#include "utils/gzip_utils.h"
#include "utils/logger.h"

#include <mutex>
#include <functional>
#include <map>

#ifdef HAS_PROTOBUF
#include "douyin.pb.h"
#endif

namespace douyin {

struct MessageParser::Impl {
    ParsedCallback parsed_cb;
    AckCallback ack_cb;
    std::mutex mutex;
    std::string room_id;  // Default room_id for this parser instance
};

MessageParser::MessageParser()
    : m_impl(std::make_unique<Impl>()) {
}

MessageParser::~MessageParser() = default;

void MessageParser::set_room_id(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->room_id = room_id;
}

void MessageParser::parse_frame(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

#ifdef HAS_PROTOBUF
    // Step 1: Parse outer PushFrame
    ::PushFrame frame;
    if (!frame.ParseFromArray(data, static_cast<int>(len))) {
        LOG_WARN("MessageParser: PushFrame parse failed ({} bytes)", len);
        return;
    }

    // Step 2: Heartbeat detection
    if (frame.payloadtype() == "hb") {
        LOG_TRACE("MessageParser: heartbeat response received");
        return;
    }

    // Step 3: Gzip decompress payload
    auto decompressed = GzipUtils::decompress(
        reinterpret_cast<const uint8_t*>(frame.payload().data()),
        frame.payload().size());
    if (decompressed.empty()) {
        LOG_ERROR("MessageParser: gzip decompress failed");
        return;
    }

    // Step 4: Parse Response
    ::Response response;
    if (!response.ParseFromArray(decompressed.data(), static_cast<int>(decompressed.size()))) {
        LOG_ERROR("MessageParser: Response parse failed");
        return;
    }

    // Determine room_id: use the one from the parser instance, or from the first message's common
    std::string room_id;
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        room_id = m_impl->room_id;
    }

    // Step 5: ACK if needed
    if (response.needack()) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->ack_cb) {
            m_impl->ack_cb(static_cast<int64_t>(frame.logid()), response.internalext());
        }
    }

    // Step 6: Dispatch messages
    for (const auto& msg : response.messageslist()) {
        // Try to get room_id from message's common field if not set
        std::string msg_room_id = room_id;
        if (msg_room_id.empty()) {
            // We don't parse Common here for performance; room_id should be set by the engine
            msg_room_id = "unknown";
        }
        dispatch_message(msg.method(), msg.payload(), msg_room_id, msg.msgid());
    }
#else
    // Stub: Protobuf not available, log raw frame info
    LOG_DEBUG("MessageParser: received {} bytes (protobuf not available, cannot parse)", len);

    // Check for simple heartbeat marker
    if (len == 2 && data[0] == 'h' && data[1] == 'b') {
        LOG_TRACE("MessageParser: heartbeat response received (simple format)");
        return;
    }
#endif
}

void MessageParser::dispatch_message(const std::string& method, const std::string& payload,
                                      const std::string& room_id, int64_t msg_id) {
#ifdef HAS_PROTOBUF
    // Method handler map
    static const std::map<std::string, std::function<void(MessageParser*, const std::string&, const std::string&)>> handlers = {
        {"WebcastChatMessage",        [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_chat(pl, rid); }},
        {"WebcastGiftMessage",        [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_gift(pl, rid); }},
        {"WebcastMemberMessage",      [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_member(pl, rid); }},
        {"WebcastLikeMessage",        [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_like(pl, rid); }},
        {"WebcastSocialMessage",      [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_social(pl, rid); }},
        {"WebcastRoomUserSeqMessage", [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_room_user_seq(pl, rid); }},
        {"WebcastControlMessage",     [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_control(pl, rid); }},
        {"WebcastFansclubMessage",    [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_fansclub(pl, rid); }},
        {"WebcastEmojiChatMessage",   [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_emoji_chat(pl, rid); }},
        {"WebcastRoomStatsMessage",   [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_room_stats(pl, rid); }},
        {"WebcastRoomMessage",        [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_room(pl, rid); }},
        {"WebcastRoomRankMessage",    [](MessageParser* p, const std::string& pl, const std::string& rid){ p->parse_room_rank(pl, rid); }},
    };

    auto it = handlers.find(method);
    if (it != handlers.end()) {
        it->second(this, payload, room_id);
    } else {
        LOG_DEBUG("MessageParser: unknown message method: {}", method);
    }
#else
    // Stub: Protobuf not available
    LOG_DEBUG("MessageParser: dispatch_message called for method '{}' (protobuf not available)", method);
    (void)payload;
    (void)room_id;
    (void)msg_id;
#endif
}

void MessageParser::emit_message(Message msg) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (m_impl->parsed_cb) {
        m_impl->parsed_cb(msg);
    }
}

// ============================================================
// Individual message parsers (only compiled with protobuf)
// ============================================================

#ifdef HAS_PROTOBUF

void MessageParser::parse_chat(const std::string& payload, const std::string& room_id) {
    ::ChatMessage chat_msg;
    if (!chat_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: ChatMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::Chat;
    msg.room_id = room_id;
    msg.timestamp = chat_msg.common().create_time();
    msg.user_id = std::to_string(chat_msg.user().user_id());
    msg.user_name = chat_msg.user().nick_name();
    msg.content = chat_msg.content();
    msg.trace_id = Logger::generate_trace_id();

    emit_message(std::move(msg));
}

void MessageParser::parse_gift(const std::string& payload, const std::string& room_id) {
    ::GiftMessage gift_msg;
    if (!gift_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: GiftMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::Gift;
    msg.room_id = room_id;
    msg.timestamp = gift_msg.common().create_time();
    msg.user_id = std::to_string(gift_msg.user().user_id());
    msg.user_name = gift_msg.user().nick_name();
    msg.content = gift_msg.gift().name();

    // Parse combo_count from string field
    try {
        msg.gift_count = std::stoll(gift_msg.combo_count());
    } catch (...) {
        msg.gift_count = 1;
    }

    // Build extra JSON with gift details
    nlohmann::json extra;
    extra["gift_id"] = gift_msg.gift_id();
    extra["gift_name"] = gift_msg.gift().name();
    extra["diamond_count"] = gift_msg.gift().diamondcount();
    msg.extra = extra.dump();

    msg.trace_id = Logger::generate_trace_id();
    emit_message(std::move(msg));
}

void MessageParser::parse_member(const std::string& payload, const std::string& room_id) {
    ::MemberMessage member_msg;
    if (!member_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: MemberMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::Enter;
    msg.room_id = room_id;
    msg.timestamp = member_msg.common().create_time();
    msg.user_id = std::to_string(member_msg.user().user_id());
    msg.user_name = member_msg.user().nick_name();
    msg.content = msg.user_name + " entered the room";
    msg.trace_id = Logger::generate_trace_id();

    emit_message(std::move(msg));
}

void MessageParser::parse_like(const std::string& payload, const std::string& room_id) {
    ::LikeMessage like_msg;
    if (!like_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: LikeMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::Like;
    msg.room_id = room_id;
    msg.timestamp = like_msg.common().create_time();
    msg.user_id = std::to_string(like_msg.user().user_id());
    msg.user_name = like_msg.user().nick_name();
    msg.content = msg.user_name + " liked";
    msg.gift_count = static_cast<int64_t>(like_msg.count());
    msg.trace_id = Logger::generate_trace_id();

    emit_message(std::move(msg));
}

void MessageParser::parse_social(const std::string& payload, const std::string& room_id) {
    ::SocialMessage social_msg;
    if (!social_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: SocialMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::Social;
    msg.room_id = room_id;
    msg.timestamp = social_msg.common().create_time();
    msg.user_id = std::to_string(social_msg.user().user_id());
    msg.user_name = social_msg.user().nick_name();

    // action: 1 = follow, 2 = share
    uint64_t action = social_msg.action();
    if (action == 1) {
        msg.content = msg.user_name + " followed";
    } else if (action == 2) {
        msg.content = msg.user_name + " shared";
    } else {
        msg.content = msg.user_name + " social action";
    }
    msg.trace_id = Logger::generate_trace_id();

    emit_message(std::move(msg));
}

void MessageParser::parse_room_user_seq(const std::string& payload, const std::string& room_id) {
    ::RoomUserSeqMessage seq_msg;
    if (!seq_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: RoomUserSeqMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::OnlineCount;
    msg.room_id = room_id;
    msg.timestamp = seq_msg.common().create_time();
    msg.content = "Online: " + std::to_string(seq_msg.total());
    msg.gift_count = seq_msg.total();
    msg.trace_id = Logger::generate_trace_id();

    emit_message(std::move(msg));
}

void MessageParser::parse_control(const std::string& payload, const std::string& room_id) {
    ::ControlMessage ctrl_msg;
    if (!ctrl_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: ControlMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::System;
    msg.room_id = room_id;
    msg.timestamp = ctrl_msg.common().create_time();

    int32_t status = ctrl_msg.status();
    if (status == 3) {
        msg.content = "Live stream ended";
    } else {
        msg.content = "Control message: status=" + std::to_string(status);
    }
    msg.trace_id = Logger::generate_trace_id();

    nlohmann::json extra;
    extra["control_status"] = status;
    msg.extra = extra.dump();

    emit_message(std::move(msg));
}

void MessageParser::parse_fansclub(const std::string& payload, const std::string& room_id) {
    ::FansclubMessage fans_msg;
    if (!fans_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: FansclubMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::System;
    msg.room_id = room_id;
    msg.timestamp = fans_msg.commoninfo().create_time();
    msg.content = fans_msg.content();
    if (fans_msg.has_user()) {
        msg.user_id = std::to_string(fans_msg.user().user_id());
        msg.user_name = fans_msg.user().nick_name();
    }
    msg.trace_id = Logger::generate_trace_id();

    emit_message(std::move(msg));
}

void MessageParser::parse_emoji_chat(const std::string& payload, const std::string& room_id) {
    ::EmojiChatMessage emoji_msg;
    if (!emoji_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: EmojiChatMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::Chat;
    msg.room_id = room_id;
    msg.timestamp = emoji_msg.common().create_time();
    msg.user_id = std::to_string(emoji_msg.user().user_id());
    msg.user_name = emoji_msg.user().nick_name();
    msg.content = emoji_msg.defaultcontent();

    nlohmann::json extra;
    extra["emoji_id"] = emoji_msg.emojiid();
    msg.extra = extra.dump();

    msg.trace_id = Logger::generate_trace_id();
    emit_message(std::move(msg));
}

void MessageParser::parse_room_stats(const std::string& payload, const std::string& room_id) {
    ::RoomStatsMessage stats_msg;
    if (!stats_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: RoomStatsMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::Stats;
    msg.room_id = room_id;
    msg.timestamp = stats_msg.common().create_time();
    msg.content = stats_msg.displaylong();
    msg.gift_count = stats_msg.total();
    msg.trace_id = Logger::generate_trace_id();

    nlohmann::json extra;
    extra["display_short"] = stats_msg.displayshort();
    extra["display_middle"] = stats_msg.displaymiddle();
    extra["display_value"] = stats_msg.displayvalue();
    extra["total"] = stats_msg.total();
    msg.extra = extra.dump();

    emit_message(std::move(msg));
}

void MessageParser::parse_room(const std::string& payload, const std::string& room_id) {
    ::RoomMessage room_msg;
    if (!room_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: RoomMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::System;
    msg.room_id = room_id;
    msg.timestamp = room_msg.common().create_time();
    msg.content = room_msg.content();
    msg.trace_id = Logger::generate_trace_id();

    emit_message(std::move(msg));
}

void MessageParser::parse_room_rank(const std::string& payload, const std::string& room_id) {
    ::RoomRankMessage rank_msg;
    if (!rank_msg.ParseFromString(payload)) {
        LOG_WARN("MessageParser: RoomRankMessage parse failed");
        return;
    }

    Message msg;
    msg.type = MessageType::Stats;
    msg.room_id = room_id;
    msg.timestamp = rank_msg.common().create_time();
    msg.content = "Room rank update";
    msg.trace_id = Logger::generate_trace_id();

    // Build extra JSON with rank info
    nlohmann::json ranks = nlohmann::json::array();
    for (const auto& rank : rank_msg.rankslist()) {
        nlohmann::json r;
        r["user_name"] = rank.user().nick_name();
        r["score"] = rank.scorestr();
        ranks.push_back(r);
    }
    nlohmann::json extra;
    extra["ranks"] = ranks;
    msg.extra = extra.dump();

    emit_message(std::move(msg));
}

#else // !HAS_PROTOBUF

// Stub implementations when protobuf is not available
void MessageParser::parse_chat(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_chat stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_gift(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_gift stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_member(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_member stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_like(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_like stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_social(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_social stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_room_user_seq(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_room_user_seq stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_control(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_control stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_fansclub(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_fansclub stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_emoji_chat(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_emoji_chat stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_room_stats(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_room_stats stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_room(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_room stub (protobuf not available)");
    (void)payload; (void)room_id;
}

void MessageParser::parse_room_rank(const std::string& payload, const std::string& room_id) {
    LOG_DEBUG("MessageParser: parse_room_rank stub (protobuf not available)");
    (void)payload; (void)room_id;
}

#endif // HAS_PROTOBUF

void MessageParser::on_parsed(ParsedCallback cb) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->parsed_cb = std::move(cb);
}

void MessageParser::on_ack(AckCallback cb) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->ack_cb = std::move(cb);
}

} // namespace douyin
