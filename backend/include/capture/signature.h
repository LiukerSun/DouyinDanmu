#pragma once

#include <string>
#include <map>
#include <memory>

namespace douyin {

// Generates X-Bogus and a_bogus signatures using an embedded QuickJS engine
// that executes the sign.js / a_bogus.js scripts.
class Signature {
public:
    Signature();
    ~Signature();

    // Non-copyable
    Signature(const Signature&) = delete;
    Signature& operator=(const Signature&) = delete;

    // Load the sign.js script. Returns true on success.
    bool load_sign_js(const std::string& path);

    // Load the a_bogus.js script. Returns true on success.
    bool load_a_bogus_js(const std::string& path);

    // Set the user agent string (used by a_bogus generation).
    void set_user_agent(const std::string& ua);

    // Generate X-Bogus signature for the given URL parameters.
    // params: the query string parameters as key-value pairs.
    // Returns the X-Bogus value, or empty string on failure.
    std::string generate_x_bogus(const std::map<std::string, std::string>& params);

    // Generate a_bogus signature for HTTP API requests.
    std::string generate_a_bogus(const std::map<std::string, std::string>& params);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace douyin
