#include "capture/douyin_client.h"
#include "utils/logger.h"

#include <ixwebsocket/IXWebSocket.h>
#include <mutex>

namespace douyin {

struct DouyinClient::Impl {
    std::unique_ptr<ix::WebSocket> ws;
    RawMessageCallback message_cb;
    StatusCallback status_cb;
    std::mutex mutex;
    bool connected = false;
};

DouyinClient::DouyinClient()
    : m_impl(std::make_unique<Impl>()) {
    m_impl->ws = std::make_unique<ix::WebSocket>();
}

DouyinClient::~DouyinClient() {
    disconnect();
}

bool DouyinClient::connect(const std::string& wss_url, const std::string& cookies) {
    if (m_impl->connected) {
        disconnect();
    }

    m_impl->ws->setUrl(wss_url);

    // Set headers
    ix::WebSocketHttpHeaders headers;
    headers["Cookie"] = cookies;
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36 Edg/140.0.0.0";
    m_impl->ws->setExtraHeaders(headers);

    // Debug: log headers
    LOG_INFO("WS Cookie: {}", cookies);
    LOG_INFO("WS URL length: {}", wss_url.size());

    // Disable per-message deflate (some servers don't support it properly)
    // m_impl->ws->enablePerMessageDeflate();

    // Set up message callback
    m_impl->ws->setOnMessageCallback(
        [this](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                LOG_INFO("DouyinClient: WebSocket connected");
                std::lock_guard<std::mutex> lock(m_impl->mutex);
                m_impl->connected = true;
                if (m_impl->status_cb) {
                    m_impl->status_cb(true);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Close) {
                LOG_INFO("DouyinClient: WebSocket closed: {}", msg->closeInfo.reason);
                std::lock_guard<std::mutex> lock(m_impl->mutex);
                m_impl->connected = false;
                if (m_impl->status_cb) {
                    m_impl->status_cb(false);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Message) {
                if (m_impl->message_cb && !msg->str.empty()) {
                    std::vector<uint8_t> data(msg->str.begin(), msg->str.end());
                    m_impl->message_cb(data);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Error) {
                LOG_ERROR("DouyinClient: WebSocket error: {}", msg->errorInfo.reason);
            }
        });

    // Enable automatic reconnection
    ix::WebSocketPerMessageDeflateOptions deflate_opts;
    m_impl->ws->setPerMessageDeflateOptions(deflate_opts);
    m_impl->ws->enableAutomaticReconnection();

    m_impl->ws->start();
    return true;
}

void DouyinClient::disconnect() {
    if (m_impl->ws) {
        m_impl->ws->disableAutomaticReconnection();
        m_impl->ws->stop();
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->connected = false;
}

bool DouyinClient::is_connected() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->connected;
}

bool DouyinClient::send_binary(const uint8_t* data, size_t len) {
    if (!m_impl->connected || !m_impl->ws) return false;
    std::string payload(reinterpret_cast<const char*>(data), len);
    m_impl->ws->sendBinary(payload);
    return true;
}

void DouyinClient::on_message(RawMessageCallback cb) {
    m_impl->message_cb = std::move(cb);
}

void DouyinClient::on_status(StatusCallback cb) {
    m_impl->status_cb = std::move(cb);
}

} // namespace douyin
