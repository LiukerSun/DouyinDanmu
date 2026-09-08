#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <memory>

namespace douyin {

struct Message;

// Parses raw protobuf messages from Douyin's WebSocket protocol.
// Handles: PushFrame -> gzip decompress -> Response -> individual Message dispatch.
class MessageParser {
public:
    using ParsedCallback = std::function<void(const Message& msg)>;
    using AckCallback    = std::function<void(int64_t log_id, const std::string& internal_ext)>;

    MessageParser();
    ~MessageParser();

    // Non-copyable
    MessageParser(const MessageParser&) = delete;
    MessageParser& operator=(const MessageParser&) = delete;

    // Parse a raw binary WebSocket frame.
    // Calls parsed_callback for each decoded message.
    // Calls ack_callback if the server requests an ACK.
    void parse_frame(const uint8_t* data, size_t len);

    // Set callbacks.
    void on_parsed(ParsedCallback cb);
    void on_ack(AckCallback cb);

    // Set the room_id for messages parsed by this instance.
    void set_room_id(const std::string& room_id);

private:
    // Dispatch a single message based on its method field.
    void dispatch_message(const std::string& method, const std::string& payload,
                          const std::string& room_id, int64_t msg_id);

    // Individual message type parsers
    void parse_chat(const std::string& payload, const std::string& room_id);
    void parse_gift(const std::string& payload, const std::string& room_id);
    void parse_member(const std::string& payload, const std::string& room_id);
    void parse_like(const std::string& payload, const std::string& room_id);
    void parse_social(const std::string& payload, const std::string& room_id);
    void parse_room_user_seq(const std::string& payload, const std::string& room_id);
    void parse_control(const std::string& payload, const std::string& room_id);
    void parse_fansclub(const std::string& payload, const std::string& room_id);
    void parse_emoji_chat(const std::string& payload, const std::string& room_id);
    void parse_room_stats(const std::string& payload, const std::string& room_id);
    void parse_room(const std::string& payload, const std::string& room_id);
    void parse_room_rank(const std::string& payload, const std::string& room_id);

    // Helper to emit a parsed message
    void emit_message(Message msg);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace douyin
