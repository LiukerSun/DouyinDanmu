#include "capture/signature.h"
#include "utils/logger.h"
#include "utils/md5.h"

#include <fstream>
#include <sstream>
#include <mutex>
#include <cstring>
#include <iomanip>
#include <vector>

// QuickJS headers (conditional)
#ifdef HAS_QUICKJS
extern "C" {
#include "quickjs.h"
#include "quickjs-libc.h"
}
#endif

namespace douyin {

#ifdef HAS_QUICKJS

struct Signature::Impl {
    JSRuntime* runtime = nullptr;
    JSContext* context = nullptr;
    bool sign_loaded = false;
    bool a_bogus_loaded = false;
    std::string sign_func_name = "get_sign";
    std::string a_bogus_func_name = "get_ab";
    std::string user_agent;
    std::mutex mutex;

    ~Impl() {
        if (context) JS_FreeContext(context);
        if (runtime) JS_FreeRuntime(runtime);
    }
};

Signature::Signature()
    : m_impl(std::make_unique<Impl>()) {
    m_impl->runtime = JS_NewRuntime();
    m_impl->context = JS_NewContext(m_impl->runtime);

    // Set memory limit
    JS_SetMemoryLimit(m_impl->runtime, 64 * 1024 * 1024); // 64MB
    JS_SetMaxStackSize(m_impl->runtime, 4 * 1024 * 1024);  // 4MB stack

    // Inject browser mock objects for QuickJS compatibility
    const char* mock = R"(
        if (typeof document === 'undefined') { var document = {createElement: function(){return {}}, cookie: ''}; }
        if (typeof window === 'undefined') { var window = {}; }
        if (typeof navigator === 'undefined') { var navigator = {userAgent: '', platform: ''}; }
        if (typeof screen === 'undefined') { var screen = {width: 1536, height: 864}; }
        if (typeof location === 'undefined') { var location = {href: '', protocol: 'https:'}; }
    )";
    JS_Eval(m_impl->context, mock, strlen(mock), "mock", JS_EVAL_TYPE_GLOBAL);
}

Signature::~Signature() = default;

void Signature::set_user_agent(const std::string& ua) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->user_agent = ua;
}

bool Signature::load_sign_js(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open sign.js: {}", path);
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string code = ss.str();

    JSValue result = JS_Eval(m_impl->context, code.c_str(), code.size(),
                              "sign.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(m_impl->context);
        const char* msg = JS_ToCString(m_impl->context, exc);
        LOG_ERROR("sign.js evaluation error: {}", msg ? msg : "unknown");
        JS_FreeCString(m_impl->context, msg);
        JS_FreeValue(m_impl->context, exc);
        JS_FreeValue(m_impl->context, result);
        return false;
    }

    JS_FreeValue(m_impl->context, result);

    // Probe for the sign function name
    JSValue global = JS_GetGlobalObject(m_impl->context);
    JSValue func = JS_GetPropertyStr(m_impl->context, global, "get_sign");
    if (JS_IsFunction(m_impl->context, func)) {
        m_impl->sign_func_name = "get_sign";
        LOG_INFO("sign.js: found function 'get_sign'");
    } else {
        JS_FreeValue(m_impl->context, func);
        func = JS_GetPropertyStr(m_impl->context, global, "sign");
        if (JS_IsFunction(m_impl->context, func)) {
            m_impl->sign_func_name = "sign";
            LOG_INFO("sign.js: found function 'sign'");
        } else {
            LOG_WARN("sign.js: no known sign function found");
        }
    }
    JS_FreeValue(m_impl->context, func);
    JS_FreeValue(m_impl->context, global);

    m_impl->sign_loaded = true;
    LOG_INFO("sign.js loaded successfully from: {}", path);
    return true;
}

bool Signature::load_a_bogus_js(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open a_bogus.js: {}", path);
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string code = ss.str();

    JSValue result = JS_Eval(m_impl->context, code.c_str(), code.size(),
                              "a_bogus.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(m_impl->context);
        const char* msg = JS_ToCString(m_impl->context, exc);
        LOG_ERROR("a_bogus.js evaluation error: {}", msg ? msg : "unknown");
        JS_FreeCString(m_impl->context, msg);
        JS_FreeValue(m_impl->context, exc);
        JS_FreeValue(m_impl->context, result);
        return false;
    }

    JS_FreeValue(m_impl->context, result);

    // Probe for the a_bogus function
    JSValue global = JS_GetGlobalObject(m_impl->context);
    JSValue func = JS_GetPropertyStr(m_impl->context, global, "get_ab");
    if (JS_IsFunction(m_impl->context, func)) {
        m_impl->a_bogus_func_name = "get_ab";
        LOG_INFO("a_bogus.js: found function 'get_ab'");
    }
    JS_FreeValue(m_impl->context, func);
    JS_FreeValue(m_impl->context, global);

    m_impl->a_bogus_loaded = true;
    LOG_INFO("a_bogus.js loaded successfully from: {}", path);
    return true;
}

// Helper: execute a command and return stdout
static std::string exec_command(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

std::string Signature::generate_x_bogus(const std::map<std::string, std::string>& params) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Step 1: Extract 13 key parameters in fixed order and build param string
    static const std::vector<std::string> param_order = {
        "live_id", "aid", "version_code", "webcast_sdk_version",
        "room_id", "sub_room_id", "sub_channel_id", "did_rule",
        "user_unique_id", "device_platform", "device_type", "ac", "identity"
    };

    std::string param_str;
    for (const auto& key : param_order) {
        if (!param_str.empty()) param_str += ",";
        auto it = params.find(key);
        if (it != params.end()) {
            param_str += key + "=" + it->second;
        } else {
            param_str += key + "=";
        }
    }

    // Step 2: Compute MD5 hash
    uint8_t md5_hash[md5::MD5::DIGEST_LENGTH];
    md5::MD5::hash(reinterpret_cast<const uint8_t*>(param_str.c_str()), param_str.size(), md5_hash);

    static const char hex_chars[] = "0123456789abcdef";
    std::string md5_hex;
    md5_hex.reserve(md5::MD5::DIGEST_LENGTH * 2);
    for (size_t i = 0; i < md5::MD5::DIGEST_LENGTH; ++i) {
        md5_hex += hex_chars[(md5_hash[i] >> 4) & 0x0F];
        md5_hex += hex_chars[md5_hash[i] & 0x0F];
    }

    LOG_INFO("Signature param_str: {}", param_str);
    LOG_INFO("Signature MD5: {}", md5_hex);

    // Step 3: Use Node.js to call get_sign(md5_hex) - QuickJS doesn't handle sign.js correctly
    // Escape the MD5 for shell command
    std::string node_cmd = "node -e \""
        "const fs = require('fs');"
        "const code = fs.readFileSync('scripts/sign.js', 'utf8');"
        "eval(code);"
        "const signature = get_sign('" + md5_hex + "');"
        "process.stdout.write(signature);"
        "\"";

    std::string signature = exec_command(node_cmd);

    if (!signature.empty()) {
        LOG_INFO("X-Bogus generated ({} chars): {}...", signature.size(), signature.substr(0, 30));
    } else {
        LOG_ERROR("Failed to generate X-Bogus signature via Node.js");
    }
    return signature;
}

std::string Signature::generate_a_bogus(const std::map<std::string, std::string>& params) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Step 1: Build query parameter string
    std::string query;
    for (const auto& [k, v] : params) {
        if (!query.empty()) query += "&";
        query += k + "=" + v;
    }

    // Step 2: Use Node.js to call get_ab(query, user_agent)
    // Escape strings for shell command
    std::string escaped_query = query;
    // Replace single quotes with escaped single quotes
    size_t pos = 0;
    while ((pos = escaped_query.find('\'', pos)) != std::string::npos) {
        escaped_query.replace(pos, 1, "'\\''");
        pos += 4;
    }

    std::string escaped_ua = m_impl->user_agent;
    pos = 0;
    while ((pos = escaped_ua.find('\'', pos)) != std::string::npos) {
        escaped_ua.replace(pos, 1, "'\\''");
        pos += 4;
    }

    std::string node_cmd = "node -e \""
        "const fs = require('fs');"
        "const code = fs.readFileSync('scripts/a_bogus.js', 'utf8');"
        "eval(code);"
        "const signature = get_ab('" + escaped_query + "', '" + escaped_ua + "');"
        "process.stdout.write(signature);"
        "\"";

    std::string signature = exec_command(node_cmd);

    if (!signature.empty()) {
        LOG_INFO("a_bogus generated ({} chars): {}...", signature.size(), signature.substr(0, 30));
    } else {
        LOG_ERROR("Failed to generate a_bogus signature via Node.js");
    }
    return signature;
}

#else // !HAS_QUICKJS

// Stub implementation when QuickJS is not available
struct Signature::Impl {
    bool sign_loaded = false;
    bool a_bogus_loaded = false;
    std::string user_agent;
    std::mutex mutex;
};

Signature::Signature()
    : m_impl(std::make_unique<Impl>()) {
    LOG_WARN("QuickJS not available, signature generation disabled");
}

Signature::~Signature() = default;

void Signature::set_user_agent(const std::string& ua) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->user_agent = ua;
}

bool Signature::load_sign_js(const std::string& path) {
    LOG_WARN("QuickJS not available, cannot load sign.js");
    return false;
}

bool Signature::load_a_bogus_js(const std::string& path) {
    LOG_WARN("QuickJS not available, cannot load a_bogus.js");
    return false;
}

std::string Signature::generate_x_bogus(const std::map<std::string, std::string>& params) {
    LOG_WARN("QuickJS not available, cannot generate X-Bogus");
    return "";
}

std::string Signature::generate_a_bogus(const std::map<std::string, std::string>& params) {
    LOG_WARN("QuickJS not available, cannot generate a_bogus");
    return "";
}

#endif // HAS_QUICKJS

} // namespace douyin
