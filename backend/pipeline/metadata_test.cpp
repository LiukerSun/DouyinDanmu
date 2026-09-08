// Exercise the production parser and SQLite transaction without starting any services.
#define main pipeline_application_main
#include "main.cpp"
#undef main

#include <cstdlib>
#include <optional>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Fn> void rejects(Fn operation, const std::string& message) {
    try { operation(); }
    catch (const std::exception&) { return; }
    throw std::runtime_error("Expected rejection: " + message);
}

// mkdtemp owns a newly created directory; no running application's files are used.
class TemporaryDatabase {
    std::filesystem::path directory;
    std::optional<std::string> previous;
public:
    TemporaryDatabase() {
        if (const char* value = std::getenv("DATABASE_PATH")) previous = value;
#ifdef _WIN32
        const auto base=std::filesystem::temp_directory_path();
        for(int attempt=0;attempt<100;attempt++) {
            directory=base/("douyin-native-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            if(std::filesystem::create_directory(directory))break;
            directory.clear();
        }
        if(directory.empty())throw std::runtime_error("Cannot create isolated test directory");
#else
        char pattern[] = "/tmp/douyin-metadata-test-XXXXXX";
        const char* created = ::mkdtemp(pattern);
        if (!created) throw std::runtime_error("Cannot create isolated test directory");
        directory = created;
#endif
        const auto path = (directory / "pipeline.db").string();
#ifdef _WIN32
        if (_putenv_s("DATABASE_PATH", path.c_str()) != 0)
#else
        if (::setenv("DATABASE_PATH", path.c_str(), 1) != 0)
#endif
            throw std::runtime_error("Cannot configure isolated test database");
    }
    ~TemporaryDatabase() {
#ifdef _WIN32
        _putenv_s("DATABASE_PATH", previous ? previous->c_str() : "");
#else
        if (previous) ::setenv("DATABASE_PATH", previous->c_str(), 1);
        else ::unsetenv("DATABASE_PATH");
#endif
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};

const std::string live_id = "123456789012";
const std::string room_id = "7000000000000000001";
const std::string anchor_id = "9007199254740993123";

json fixture(int64_t checked) {
    return {
        {"room_id", room_id}, {"title", "测试直播间 · 音乐"},
        {"live_status", "live"}, {"checked_at_ms", checked},
        {"live_started_at_ms", checked - 3600000},
        {"anchor", {
            {"id", anchor_id}, {"sec_uid", "test-sec-uid"},
            {"nickname", "测试主播"}, {"avatar_url", "https://example.invalid/avatar.png"},
            {"signature", "只用于离线测试"}, {"display_id", "music_anchor_01"}
        }}
    };
}

pipeline::RawFrameEnvelope frame(const std::string& kind, const std::string& payload,
                                 uint64_t version = 1) {
    static uint64_t sequence = 0;
    pipeline::RawFrameEnvelope result;
    result.set_schema_version(1);
    result.set_frame_id("metadata-test-" + std::to_string(++sequence));
    result.set_live_id(live_id);
    result.set_room_id(room_id);
    result.set_session_id("offline-test-session");
    result.set_session_seq(sequence);
    result.set_received_at_ms(now_ms());
    result.set_source("douyin");
    result.set_kind(kind);
    result.set_desired_version(version);
    result.set_payload(payload);
    result.set_payload_sha256(digest(payload));
    return result;
}

pipeline::RawFrameEnvelope metadata_frame(const json& metadata, uint64_t version = 1) {
    return frame("room_metadata", metadata.dump(), version);
}

void deliver(Store& store, const pipeline::RawFrameEnvelope& envelope) {
    const auto parsed = parse(envelope);
    store.commit(envelope, envelope.SerializeAsString(), parsed.first, parsed.second);
}

json room(Store& store) {
    for (const auto& candidate : store.rooms())
        if (candidate.at("live_id") == live_id) return candidate;
    throw std::runtime_error("Test target missing from rooms()");
}

void seed_chat(Store& store) {
    ChatMessage chat;
    chat.set_content("已有弹幕");
    chat.mutable_user()->set_id(9007199254740993ULL);
    chat.mutable_user()->set_nickname("测试观众");
    chat.mutable_common()->set_create_time(now_ms() / 1000);
    Response response;
    auto* message = response.add_messageslist();
    message->set_method("WebcastChatMessage");
    message->set_msgid(321);
    message->set_payload(chat.SerializeAsString());
    PushFrame push;
    push.set_payload(response.SerializeAsString());
    deliver(store, frame("upstream_frame", push.SerializeAsString()));
}

void persistence_and_ordering() {
    TemporaryDatabase database;
    const int64_t observed = now_ms() - 60000;
    json expected_room, expected_snapshot, expected_overview;
    {
        Store store;
        store.target(live_id, "douyin", true);
        seed_chat(store);
        deliver(store, frame("collector_status",
            json({{"status", "collecting"}, {"detail", "测试连接已建立"}}).dump()));
        const auto original_room = room(store);
        const auto original_snapshot = store.snapshot(live_id);
        check(original_snapshot.at("stats").at("chat") == 1,
              "Existing chat must establish nonempty statistics before metadata tests");
        check(original_room.at("metadata").is_null(), "New target must start without metadata");
        check(original_room.at("metadata_stale") == true, "Missing metadata must be stale");

        auto metadata = fixture(observed);
        const auto first = metadata_frame(metadata);
        const auto parsed = parse(first);
        check(parsed.first.empty() && parsed.second.empty(),
              "Valid metadata must produce neither chat events nor quarantine errors");
        deliver(store, first);
        check(room(store).at("metadata") == metadata, "Every metadata field must round-trip");
        check(room(store).at("metadata").at("anchor").at("id").is_string(),
              "Anchor ID must remain a JSON string");
        check(room(store).at("metadata").at("anchor").at("id") == anchor_id,
              "19-digit anchor ID must preserve every digit");
        check(room(store).at("metadata_stale") == false, "Recent observation must be fresh");

        const auto after_first = store.overview();
        deliver(store, first);
        check(store.overview() == after_first, "Duplicate frame must not create another receipt or event");
        check(room(store).at("metadata") == metadata, "Duplicate frame must not alter metadata");
        auto conflicting = first;
        auto altered = metadata;
        altered["title"] = "conflicting duplicate";
        conflicting.set_payload(altered.dump());
        conflicting.set_payload_sha256(digest(conflicting.payload()));
        rejects([&] { deliver(store, conflicting); }, "Same frame ID with a different body");
        check(room(store).at("metadata") == metadata, "Conflicting duplicate must leave metadata intact");

        int64_t latest_time = observed;
        for (const std::string state : {"offline", "unknown", "live"}) {
            metadata["live_status"] = state;
            metadata["checked_at_ms"] = ++latest_time;
            metadata["live_started_at_ms"] = state == "live" ? json(observed - 3600000) : json(nullptr);
            metadata["title"] = "状态: " + state;
            deliver(store, metadata_frame(metadata));
            const auto updated = room(store);
            check(updated.at("metadata") == metadata, "Latest live status must be stored: " + state);
            check(updated.at("status") == original_room.at("status") &&
                  updated.at("detail") == original_room.at("detail") &&
                  updated.at("updated_at") == original_room.at("updated_at"),
                  "Live status must not overwrite collector status, detail, or activity timestamp: " + state);
            check(store.snapshot(live_id) == original_snapshot,
                  "Metadata must not affect message counts, events, or replay cursor: " + state);
        }

        auto older = fixture(observed);
        older["title"] = "过期资料";
        const auto stale_frame = metadata_frame(older);
        deliver(store, stale_frame);
        check(room(store).at("metadata") == metadata, "An older observation must not overwrite newer metadata");
        check(store.receipts(json::array({stale_frame.frame_id()})).size() == 1,
              "Ignored older observation must still receive a durable receipt");
        older["checked_at_ms"] = latest_time;
        deliver(store, metadata_frame(older));
        check(room(store).at("metadata") == metadata, "Equal observation timestamps must not overwrite metadata");

        store.target(live_id, "douyin", true);
        check(room(store).at("version") == 2, "Re-enabled target must advance desired version");
        auto next = fixture(++latest_time);
        next["title"] = "重连后的资料";
        deliver(store, metadata_frame(next, 1));
        check(room(store).at("metadata") == metadata, "Previous desired version must not overwrite metadata");
        deliver(store, metadata_frame(next, 2));
        metadata = next;
        check(room(store).at("metadata") == metadata, "Current desired version must update metadata");
        check(room(store).at("status") == "connecting", "Metadata must not mark a reconnect as collecting");

        store.target(live_id, "douyin", false);
        check(room(store).at("version") == 3, "Disabled target must advance desired version");
        next["checked_at_ms"] = ++latest_time;
        next["title"] = "停用后到达的资料";
        deliver(store, metadata_frame(next, 3));
        check(room(store).at("metadata") == metadata, "Disabled target must reject metadata even with matching version");
        check(room(store).at("status") == "stopped", "Metadata must not reactivate a stopped collector");
        check(store.snapshot(live_id) == original_snapshot, "All metadata deliveries must leave message history unchanged");

        expected_room = room(store);
        expected_snapshot = store.snapshot(live_id);
        expected_overview = store.overview();
    }
    {
        Store reopened;
        check(room(reopened) == expected_room, "Room metadata and desired state must survive database reopen");
        check(reopened.snapshot(live_id) == expected_snapshot, "Existing message history must survive metadata writes and reopen");
        check(reopened.overview() == expected_overview, "Metadata receipts must survive database reopen");
    }
}

void invalid_metadata_is_rejected() {
    const auto valid = fixture(now_ms() - 1000);
    std::vector<std::pair<std::string, json>> invalid;
    const auto changed = [&](const std::string& field, const json& value, const std::string& label) {
        auto candidate = valid;
        candidate[field] = value;
        invalid.emplace_back(label, std::move(candidate));
    };
    changed("live_status", "collecting", "collector status is not a live status");
    changed("live_status", 2, "numeric live status");
    changed("room_id", 7000000000000000001ULL, "numeric room ID");
    changed("title", nullptr, "null room title");
    changed("anchor", json::array(), "anchor is not an object");
    for (const char* field : {"id", "sec_uid", "nickname", "avatar_url", "signature", "display_id"}) {
        auto candidate = valid;
        candidate["anchor"][field] = 9007199254740993123ULL;
        invalid.emplace_back(std::string("numeric anchor field: ") + field, std::move(candidate));
    }
    changed("checked_at_ms", "1788775887439", "string observation timestamp");
    changed("checked_at_ms", 123.5, "fractional observation timestamp");
    changed("checked_at_ms", true, "boolean observation timestamp");
    changed("checked_at_ms", 0, "zero observation timestamp");
    changed("checked_at_ms", -1, "negative observation timestamp");
    changed("checked_at_ms", now_ms() + 120000, "observation too far in the future");
    changed("live_started_at_ms", "1788775887439", "string live start timestamp");
    changed("live_started_at_ms", 12.5, "fractional live start timestamp");
    changed("live_started_at_ms", true, "boolean live start timestamp");
    changed("live_started_at_ms", 0, "zero live start timestamp");
    changed("live_started_at_ms", -1, "negative live start timestamp");
    for (const char* field : {"room_id", "title", "live_status", "anchor", "checked_at_ms", "live_started_at_ms"}) {
        auto candidate = valid;
        candidate.erase(field);
        invalid.emplace_back(std::string("missing required field: ") + field, std::move(candidate));
    }
    for (const auto& [label, candidate] : invalid)
        rejects([&] { parse(metadata_frame(candidate)); }, label);
    rejects([&] { parse(frame("room_metadata", "{invalid JSON")); }, "malformed JSON");
    rejects([&] { parse(metadata_frame(json::array())); }, "array instead of metadata object");
}
} // namespace

int main() {
    try {
        persistence_and_ordering();
        invalid_metadata_is_rejected();
        std::cout << "room_metadata tests passed: persistence, ordering, isolation, and validation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "room_metadata test failed: " << error.what() << '\n';
        return 1;
    }
}
