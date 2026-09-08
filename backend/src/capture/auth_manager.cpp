#include "capture/auth_manager.h"
#include "utils/logger.h"

#include <random>
#include <regex>
#include <sstream>
#include <mutex>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>

namespace douyin {

// URL decode helper
static std::string url_decode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int val = 0;
            std::istringstream hex(str.substr(i + 1, 2));
            if (hex >> std::hex >> val) {
                result += static_cast<char>(val);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

struct AuthManager::Impl {
    std::string ttwid;
    std::string ms_token;
    std::string ac_nonce;
    std::mutex mutex;
};

// Helper: execute curl command and return stdout
static std::string exec_curl(const std::string& url, const std::vector<std::string>& headers, bool include_headers = false) {
    std::ostringstream cmd;
    cmd << "curl -sS -L --max-time 10";
    if (include_headers) {
        cmd << " -D -";  // Include response headers
    }
    for (const auto& h : headers) {
        cmd << " -H \"" << h << "\"";
    }
    cmd << " \"" << url << "\" 2>&1";

    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.str().c_str(), "r"), pclose);
    if (!pipe) {
        LOG_ERROR("Failed to execute curl");
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

AuthManager::AuthManager()
    : m_impl(std::make_unique<Impl>()) {
    // Generate a default msToken
    m_impl->ms_token = generate_ms_token();
}

AuthManager::~AuthManager() = default;

bool AuthManager::fetch_ttwid(const std::string& user_agent) {
    LOG_INFO("Fetching ttwid cookie...");

    std::vector<std::string> headers = {
        "User-Agent: " + user_agent,
        "Accept: text/html,application/xhtml+xml",
        "Accept-Language: zh-CN,zh;q=0.9"
    };

    std::string response = exec_curl("https://live.douyin.com/", headers, true);
    if (response.empty()) {
        LOG_ERROR("Failed to fetch ttwid: empty response");
        return false;
    }

    // Extract ttwid from response headers
    std::regex ttwid_regex("ttwid=([^;\\s]+)");
    std::smatch match;
    if (std::regex_search(response, match, ttwid_regex)) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->ttwid = url_decode(match[1].str());
        LOG_INFO("ttwid obtained: {}...", m_impl->ttwid.substr(0, 20));
        return true;
    }

    LOG_WARN("ttwid not found in response");
    return false;
}

std::string AuthManager::fetch_room_id(const std::string& live_id, const std::string& user_agent) {
    LOG_INFO("Fetching room_id for live_id={}", live_id);

    std::vector<std::string> headers = {
        "User-Agent: " + user_agent,
        "Cookie: ttwid=" + m_impl->ttwid,
        "Accept: text/html"
    };

    std::string response = exec_curl("https://live.douyin.com/" + live_id, headers);
    if (response.empty()) {
        LOG_ERROR("Failed to fetch live page for live_id={}", live_id);
        return "";
    }

    // Extract room_id from HTML: roomId":"(\d+)
    std::regex room_regex("roomId\\\\?\":\\\\?\"(\\d+)");
    std::smatch match;
    if (std::regex_search(response, match, room_regex)) {
        std::string room_id = match[1].str();
        LOG_INFO("room_id found: {}", room_id);
        return room_id;
    }

    LOG_WARN("room_id not found in page HTML");
    return "";
}

bool AuthManager::fetch_ac_nonce(const std::string& user_agent) {
    LOG_INFO("Fetching __ac_nonce...");

    std::vector<std::string> headers = {
        "User-Agent: " + user_agent,
        "Accept: text/html"
    };

    std::string response = exec_curl("https://www.douyin.com/", headers, true);
    if (response.empty()) {
        LOG_ERROR("Failed to fetch __ac_nonce: empty response");
        return false;
    }

    // Extract __ac_nonce from response headers
    std::regex nonce_regex("__ac_nonce=([^;\\s]+)");
    std::smatch match;
    if (std::regex_search(response, match, nonce_regex)) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->ac_nonce = match[1].str();
        LOG_INFO("__ac_nonce obtained");
        return true;
    }

    LOG_WARN("__ac_nonce not found in response");
    return false;
}

std::string AuthManager::generate_ms_token() {
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

    std::string token;
    token.reserve(182);
    for (int i = 0; i < 182; ++i) {
        token += charset[dist(rng)];
    }
    return token;
}

std::string AuthManager::ttwid() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->ttwid;
}

std::string AuthManager::ms_token() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->ms_token;
}

std::string AuthManager::ac_nonce() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->ac_nonce;
}

std::string AuthManager::build_cookie_header() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    std::ostringstream oss;
    if (!m_impl->ttwid.empty()) {
        oss << "ttwid=" << m_impl->ttwid;
    }
    if (!m_impl->ac_nonce.empty()) {
        if (oss.tellp() > 0) oss << "; ";
        oss << "__ac_nonce=" << m_impl->ac_nonce;
    }
    return oss.str();
}

// __ac_signature implementation based on65599 hash algorithm
static uint32_t cal_one_str(const std::string& s, uint32_t iv) {
    uint32_t k = iv;
    for (char c : s) {
        k = ((k ^ (uint32_t)c) * 65599u);
    }
    return k;
}

static uint32_t cal_one_str_2(const std::string& s, uint32_t iv) {
    uint32_t k = iv;
    for (int i = 0; i < 32; ++i) {
        k = ((k ^ (uint32_t)s[i % s.size()]) * 65599u);
    }
    return k;
}

static uint32_t cal_one_str_3(const std::string& s, uint32_t iv) {
    uint32_t k = iv;
    for (char c : s) {
        k = (k * 65599u + (uint32_t)c);
    }
    return k;
}

static std::string custom_base64_encode(uint32_t val) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (int i = 0; i < 6; ++i) {
        result += charset[val & 0x3F];
        val >>= 6;
    }
    return result;
}

std::string AuthManager::generate_ac_signature(const std::string& site,
                                                const std::string& nonce,
                                                const std::string& user_agent,
                                                int64_t timestamp) {
    if (timestamp == 0) {
        timestamp = std::time(nullptr);
    }

    std::string timestamp_s = std::to_string(timestamp);

    // Step 1: Calculate hash using65599 algorithm
    uint32_t a = cal_one_str(site, cal_one_str(timestamp_s, 0)) % 65521;

    // Step 2: Build binary string
    uint32_t xor_val = timestamp ^ (a * 65521);
    std::ostringstream bin_oss;
    bin_oss << "10000000110000";
    for (int i = 31; i >= 0; --i) {
        bin_oss << ((xor_val >> i) & 1);
    }
    uint64_t b = std::stoull(bin_oss.str(), nullptr, 2);

    // Step 3: Multi-round hash + custom base64
    std::string n;
    uint32_t h = b;
    for (int i = 0; i < 3; ++i) {
        h = cal_one_str_2(std::to_string(h), i);
        n += custom_base64_encode(h);
    }

    // Step 4: Calculate checksum
    uint32_t checksum = cal_one_str_3(n, 0);
    std::string o = std::to_string(checksum);
    o = o.substr(o.size() - 2);

    return n + o;
}

} // namespace douyin
