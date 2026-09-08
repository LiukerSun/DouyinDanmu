#pragma once

#include <string>

namespace douyin {

// Utility for serving static files (frontend dist).
class StaticFileHandler {
public:
    // Get the MIME type for a file extension.
    static std::string get_mime_type(const std::string& path);

    // Check if a path is a static file request (not an API route).
    static bool is_static_request(const std::string& path);

    // Get the default static files directory (relative to executable).
    static std::string default_static_dir();
};

} // namespace douyin
