#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace douyin {

struct Message;
class Room;
class RoomManager;

enum class RoomStatus;

// Abstract interface for the data capture engine.
// Implementations handle connecting to live stream platforms
// and emitting parsed messages via callbacks.
class CaptureEngine {
public:
    using MessageCallback = std::function<void(const Message& msg)>;
    using StatusCallback  = std::function<void(const std::string& live_id, RoomStatus status)>;

    virtual ~CaptureEngine() = default;

    // Connect to a live stream by live_id (URL numeric part).
    virtual bool connect(const std::string& live_id) = 0;

    // Disconnect from a live stream.
    virtual void disconnect(const std::string& live_id) = 0;

    // Register the message callback (called for every parsed message).
    virtual void on_message(MessageCallback cb) = 0;

    // Register the status callback (called when room status changes).
    virtual void on_status(StatusCallback cb) = 0;

    // Check if connected to a given live_id.
    virtual bool is_connected(const std::string& live_id) const = 0;

    // Get list of all connected live_ids.
    virtual std::vector<std::string> connected_rooms() const = 0;

    // Get the room manager (for HTTP API access).
    virtual RoomManager* room_manager() = 0;
};

// Factory to create the default Douyin capture engine.
std::unique_ptr<CaptureEngine> create_capture_engine(const std::string& sign_js_path = "",
                                                      const std::string& a_bogus_js_path = "",
                                                      const std::string& user_agent = "");

} // namespace douyin
