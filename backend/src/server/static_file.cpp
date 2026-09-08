#include "server/static_file.h"
#include <map>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace douyin {

std::string StaticFileHandler::get_mime_type(const std::string& path) {
    // Extract extension
    auto dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos) return "application/octet-stream";

    std::string ext = path.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".htm",  "text/html"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".svg",  "image/svg+xml"},
        {".ico",  "image/x-icon"},
        {".woff", "font/woff"},
        {".woff2","font/woff2"},
        {".ttf",  "font/ttf"},
        {".map",  "application/json"},
        {".txt",  "text/plain"},
        {".xml",  "application/xml"},
        {".pdf",  "application/pdf"},
    };

    auto it = mime_types.find(ext);
    return (it != mime_types.end()) ? it->second : "application/octet-stream";
}

bool StaticFileHandler::is_static_request(const std::string& path) {
    // API routes start with /api
    if (path.substr(0, 4) == "/api") return false;
    if (path == "/ws") return false;
    return true;
}

std::string StaticFileHandler::default_static_dir() {
    // Try to find the frontend dist directory relative to the executable
    namespace fs = std::filesystem;

    // Check common locations
    std::vector<std::string> candidates = {
        "../frontend/dist",
        "../../frontend/dist",
        "./frontend/dist",
        "../dist",
        "./dist"
    };

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate) && fs::is_directory(candidate)) {
            return candidate;
        }
    }

    return "";
}

} // namespace douyin
