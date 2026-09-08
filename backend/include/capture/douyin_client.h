#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <cstdint>

namespace douyin {

// WebSocket client that connects to Douyin's live stream WSS endpoint.
// Uses IXWebSocket for the connection.
class DouyinClient {
public:
    using RawMessageCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using StatusCallback     = std::function<void(bool connected)>;

    DouyinClient();
    ~DouyinClient();

    // Non-copyable
    DouyinClient(const DouyinClient&) = delete;
    DouyinClient& operator=(const DouyinClient&) = delete;

    // Connect to the Douyin WSS endpoint.
    // wss_url: full WSS URL with query parameters
    // cookies: Cookie header string (ttwid etc.)
    bool connect(const std::string& wss_url, const std::string& cookies);

    // Disconnect.
    void disconnect();

    // Check connection status.
    bool is_connected() const;

    // Send a binary frame (e.g., heartbeat PushFrame).
    bool send_binary(const uint8_t* data, size_t len);

    // Set callbacks.
    void on_message(RawMessageCallback cb);
    void on_status(StatusCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace douyin
