#pragma once

#include <string>
#include <map>
#include <memory>
#include <cstdint>

namespace douyin {

// Manages authentication tokens and cookies for Douyin API access.
// Handles: ttwid, msToken, __ac_nonce, __ac_signature
class AuthManager {
public:
    AuthManager();
    ~AuthManager();

    // Non-copyable
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    // Step 1: Fetch ttwid cookie from live.douyin.com
    bool fetch_ttwid(const std::string& user_agent);

    // Step 2: Extract room_id from live page HTML given a live_id.
    // Returns the room_id string, or empty on failure.
    std::string fetch_room_id(const std::string& live_id, const std::string& user_agent);

    // Step 3: Fetch __ac_nonce from www.douyin.com
    bool fetch_ac_nonce(const std::string& user_agent);

    // Generate a random msToken (182 chars, base64url charset).
    static std::string generate_ms_token();

    // Generate __ac_signature from the nonce, site, and user agent.
    // Uses the 65599 hash algorithm as per Douyin's implementation.
    static std::string generate_ac_signature(const std::string& site,
                                              const std::string& nonce,
                                              const std::string& user_agent,
                                              int64_t timestamp = 0);

    // Getters
    std::string ttwid() const;
    std::string ms_token() const;
    std::string ac_nonce() const;

    // Build cookie header string for WSS connection.
    std::string build_cookie_header() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace douyin
