// Test live session derivation and per-session statistics at the Store boundary.
#define main pipeline_application_main
#include "main.cpp"
#undef main

#include <cstdlib>
#include <optional>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class TemporaryDatabase {
    std::filesystem::path directory;
    std::optional<std::string> previous;
public:
    TemporaryDatabase() {
        if (const char* value = std::getenv("DATABASE_PATH")) previous = value;
#ifdef _WIN32
        const auto base=std::filesystem::temp_directory_path();
        for(int attempt=0;attempt<100;attempt++) {
            directory=base/("douyin-session-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            if(std::filesystem::create_directory(directory))break;
            directory.clear();
        }
        if(directory.empty())throw std::runtime_error("Cannot create isolated test directory");
#else
        char pattern[] = "/tmp/douyin-session-test-XXXXXX";
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
    std::filesystem::path path() const { return directory / "pipeline.db"; }
};

const int64_t T = 1700000000000LL;

pipeline::RawFrameEnvelope frame(const std::string& live, const std::string& room, const json& event) {
    static uint64_t sequence = 0;
    pipeline::RawFrameEnvelope result;
    result.set_schema_version(1);
    result.set_frame_id("session-store-frame-" + std::to_string(++sequence));
    result.set_live_id(live);
    result.set_room_id(room);
    result.set_source("douyin");
    result.set_session_id("session-store-test");
    result.set_session_seq(sequence);
    result.set_received_at_ms(T + sequence);
    result.set_kind("upstream_frame");
    result.set_desired_version(1);
    result.set_payload(event.dump());
    result.set_payload_sha256(digest(result.payload()));
    return result;
}

pipeline::RawFrameEnvelope metadata_frame(const std::string& live, const std::string& status,
                                          const std::string& room, int64_t checked, const json& started) {
    auto envelope = frame(live, room, json::object());
    envelope.set_kind("room_metadata");
    const json body = {{"live_status", status}, {"room_id", room}, {"title", "测试直播间"},
        {"anchor", {{"id",""},{"sec_uid",""},{"nickname","主播"},{"avatar_url",""},{"signature",""},{"display_id",""}}},
        {"checked_at_ms", checked}, {"live_started_at_ms", started}};
    envelope.set_payload(body.dump());
    envelope.set_payload_sha256(digest(envelope.payload()));
    return envelope;
}

void deliver(Store& store, const pipeline::RawFrameEnvelope& envelope, const json& events, bool replay=false) {
    store.commit(envelope, envelope.SerializeAsString(), events.is_array()?events:json::array({events}), json::array(), replay);
}

json behavior(const std::string& id, const std::string& type, int64_t at) {
    json e = {{"event_id", id}, {"display_id", id}, {"type", type}, {"user_id", "123"},
            {"user_name", "观众"}, {"content", "内容 " + id}, {"timestamp", at}, {"received_at_ms", at}};
    if (type == "like") e["like_count"] = 5;
    return e;
}
json gift(const std::string& id, int64_t count, bool final, int64_t at) {
    auto e = behavior(id, "gift", at);
    e["gift_count"] = count; e["gift_final"] = final; e["gift_unit_price"] = 10;
    return e;
}
json online(const std::string& id, int64_t count, int64_t at) {
    auto e = behavior(id, "online_count", at); e["online_count"] = count; return e;
}
json control(const std::string& id, int64_t at) {
    auto e = behavior(id, "system", at);
    e["method"] = "WebcastControlMessage"; e["control_status"] = 3; e["content"] = "直播已结束";
    return e;
}

void sessions_track_boundaries_and_stats() {
    TemporaryDatabase database;
    Store store;
    store.target("111", "douyin", true);
    check(store.sessions("111", {}).at("total") == "0", "No activity must mean no sessions");

    const auto r1 = "room-alpha", r2 = "room-beta", r3 = "room-gamma", r4 = "room-delta";
    deliver(store, frame("111", r1, behavior("c1", "chat", T)), behavior("c1", "chat", T));
    deliver(store, frame("111", r1, gift("g1", 2, true, T + 10000)), gift("g1", 2, true, T + 10000));
    deliver(store, frame("111", r1, behavior("l1", "like", T + 20000)), behavior("l1", "like", T + 20000));
    deliver(store, frame("111", r1, online("o1", 8, T + 30000)), online("o1", 8, T + 30000));
    deliver(store, frame("111", r1, online("o2", 12, T + 40000)), online("o2", 12, T + 40000));
    deliver(store, frame("111", r1, behavior("sc1", "screen_chat", T + 45000)), behavior("sc1", "screen_chat", T + 45000));
    auto list = store.sessions("111", {});
    check(list.at("total") == "1" && list["items"][0]["status"] == "live", "First activity must open a live session");
    check(list["items"][0]["started_at_ms"] == T && list["items"][0]["start_source"] == "activity" &&
          list["items"][0]["room_id"] == r1, "Session must anchor at the first observed event");
    auto stats = list["items"][0]["stats"];
    check(stats["chat_count"] == "2" && stats["gift_events"] == "1" && stats["gift_quantity"] == "2" &&
          stats["known_gift_value"] == "20" && stats["value_complete"] == true,
          "Open session must aggregate chat, screen chat and priced gift quantity so far");
    check(stats["like_count"] == "5" && stats["peak_online"] == 12 && stats["enter_count"] == "0",
          "Likes must sum and peak online must track the maximum observation");

    deliver(store, frame("111", r1, control("s1", T + 50000)), control("s1", T + 50000));
    list = store.sessions("111", {});
    check(list["items"][0]["status"] == "ended" && list["items"][0]["ended_at_ms"] == T + 50000 &&
          list["items"][0]["end_source"] == "control_message", "Control status 3 must end the open session");
    check(list["items"][0]["stats"]["peak_online"] == 12, "Closing a session must retain its peak");

    deliver(store, frame("111", r2, behavior("c2", "chat", T + 3600000)), behavior("c2", "chat", T + 3600000));
    deliver(store, frame("111", r3, behavior("c3", "chat", T + 3700000)), behavior("c3", "chat", T + 3700000));
    list = store.sessions("111", {});
    check(list.at("total") == "3", "A new room identity after an ended session must open another session");
    check(list["items"][0]["room_id"] == r3 && list["items"][0]["start_source"] == "room_id_change",
          "A changed room identity must rotate the open session");
    check(list["items"][1]["room_id"] == r2 && list["items"][1]["ended_at_ms"] == T + 3700000 &&
          list["items"][1]["end_source"] == "room_id_change", "The superseded session must close at the boundary");
    check(list["items"][1]["stats"]["chat_count"] == "1", "Session windows must not leak events across the boundary");

    deliver(store, metadata_frame("111", "live", r4, T + 3800000, T + 3790000), json::array());
    deliver(store, metadata_frame("111", "offline", r4, T + 3900000, nullptr), json::array());
    deliver(store, metadata_frame("111", "live", r4, T + 4000000, nullptr), json::array());
    list = store.sessions("111", {});
    check(list.at("total") == "5", "Metadata live/offline transitions must close and open sessions");
    check(list["items"][0]["status"] == "live" && list["items"][0]["start_source"] == "metadata_live" &&
          list["items"][0]["started_at_ms"] == T + 4000000, "A fresh live observation opens a session at its check time");
    check(list["items"][1]["ended_at_ms"] == T + 3900000 && list["items"][1]["end_source"] == "metadata_offline",
          "An offline observation must end the open session");
    check(list["items"][2]["room_id"] == r3 && list["items"][2]["end_source"] == "room_id_change",
          "A new room identity from metadata must rotate the previous session");

    const auto journal_before = store.events("111", 0).size();
    deliver(store, frame("111", r4, behavior("c9", "chat", T + 4100000)), behavior("c9", "chat", T + 4100000), true);
    list = store.sessions("111", {});
    check(store.events("111", 0).size() == journal_before + 1, "Replay must still persist the recovered event");
    check(list.at("total") == "5" && list["items"][0]["status"] == "live" &&
          list["items"][0]["started_at_ms"] == T + 4000000, "Replay must never open, close or rotate sessions");

    const auto duplicate = frame("111", r4, behavior("c9", "chat", T + 4100000));
    deliver(store, duplicate, behavior("c9", "chat", T + 4100000));
    check(store.sessions("111", {}).at("total") == "5", "Frame receipts must keep redelivery idempotent");

    store.target("demo", "demo", true);
    deliver(store, frame("demo", "room-demo", behavior("dc1", "chat", T + 4200000)), behavior("dc1", "chat", T + 4200000));
    check(store.sessions("demo", {}).at("total") == "0", "Non-douyin rooms must not collect sessions");

    Store::SessionQuery page; page.limit = 2;
    auto first = store.sessions("111", page);
    check(first["items"].size() == 2 && first["next_offset"] == 2, "Session pages must honor the limit");
    page.offset = 4;
    auto last = store.sessions("111", page);
    check(last["items"].size() == 1 && last["next_offset"].is_null(), "The final page must not promise more items");
    bool rejected = false;
    try { page.limit = 0; store.sessions("111", page); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "A zero page size must be rejected");
    rejected = false;
    try { Store::SessionQuery bad; bad.offset = -1; store.sessions("111", bad); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "A negative offset must be rejected");
    httplib::Request request; request.params.emplace("from_ms", "1");
    rejected = false;
    try { Store::read_session_query(request); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Unknown session query parameters must be rejected");
    httplib::Request malformed; malformed.params.emplace("limit", "2x");
    rejected = false;
    try { Store::read_session_query(malformed); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Malformed pagination must be rejected");
    httplib::Request valid; valid.params.emplace("limit", "30"); valid.params.emplace("offset", "5");
    const auto parsed = Store::read_session_query(valid);
    check(parsed.limit == 30 && parsed.offset == 5, "Valid pagination must parse");
}

void sessions_backfill_from_journal() {
    TemporaryDatabase database;
    const std::string rooms_sql = "CREATE TABLE rooms(live_id TEXT PRIMARY KEY,source TEXT NOT NULL,title TEXT NOT NULL,enabled INTEGER NOT NULL DEFAULT 1,version INTEGER NOT NULL DEFAULT 1,status TEXT NOT NULL DEFAULT 'connecting',detail TEXT NOT NULL DEFAULT '',chat INTEGER DEFAULT 0,gift INTEGER DEFAULT 0,enter_count INTEGER DEFAULT 0,likes INTEGER DEFAULT 0,online INTEGER DEFAULT 0,total INTEGER DEFAULT 0,updated_at INTEGER DEFAULT 0)";
    auto body = [](const std::string& id, const std::string& live, const std::string& room,
                   const std::string& type, int64_t at) {
        return json{{"event_id", id}, {"display_id", id}, {"live_id", live}, {"room_id", room}, {"type", type},
                    {"user_id", "123"}, {"user_name", "观众"}, {"content", "历史 " + id},
                    {"timestamp", at}, {"received_at_ms", at}, {"persisted_at_ms", at}};
    };
    {
        sqlite3* legacy = nullptr;
        if (sqlite3_open(database.path().string().c_str(), &legacy) != SQLITE_OK)
            throw std::runtime_error("Cannot open owned legacy fixture database");
        try {
            const std::string schema = std::string(rooms_sql) + ";" + R"SQL(
              CREATE TABLE events(id INTEGER PRIMARY KEY AUTOINCREMENT,event_id TEXT UNIQUE NOT NULL,live_id TEXT NOT NULL,body TEXT NOT NULL);
              CREATE TABLE analytics_events(seq INTEGER PRIMARY KEY,event_id TEXT NOT NULL UNIQUE,live_id TEXT NOT NULL,user_id TEXT NOT NULL,user_name TEXT NOT NULL,occurred_at_ms INTEGER NOT NULL,kind TEXT NOT NULL,gift_quantity INTEGER NOT NULL,gift_unit_price INTEGER,recipient_id TEXT NOT NULL,user_level INTEGER,fans_club TEXT NOT NULL);
              CREATE TABLE schema_migrations(name TEXT PRIMARY KEY);
            )SQL";
            if (sqlite3_exec(legacy, schema.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(legacy));
            auto room = [&](const std::string& live, const std::string& source, const std::string& status) {
                Statement q(legacy, "INSERT INTO rooms(live_id,source,title,status) VALUES(?,?,?,?)");
                q.text(1, live).text(2, source).text(3, live + "直播间").text(4, status).row();
            };
            room("aaa", "douyin", "stopped"); room("bbb", "douyin", "collecting"); room("demo", "demo", "collecting");
            struct Row { int64_t id; std::string live; json event; };
            auto e1 = body("e1", "aaa", "room-a1", "chat", T + 1000);
            auto e2 = body("e2", "aaa", "room-a1", "online_count", T + 2000); e2["online_count"] = 30;
            auto e3 = body("e3", "aaa", "room-a1", "system", T + 3000); e3["content"] = "直播已结束";
            auto e4 = body("e4", "aaa", "room-a2", "like", T + 4000); e4["like_count"] = 7;
            auto e5 = body("e5", "aaa", "room-a2", "chat", T + 5000);
            auto e6 = body("e6", "bbb", "room-b1", "online_count", T + 6000); e6["online_count"] = 42;
            auto e7 = body("e7", "demo", "room-d1", "chat", T + 7000);
            auto e8 = body("e8", "aaa", "room-a2", "screen_chat", T + 4500);
            // Backfill replays in journal order; keep the timeline ordered by id.
            const Row rows[] = {{1,"aaa",e1},{2,"aaa",e2},{3,"aaa",e3},{4,"aaa",e4},{5,"aaa",e8},{6,"aaa",e5},{7,"bbb",e6},{8,"demo",e7}};
            for (const auto& row : rows) {
                Statement q(legacy, "INSERT INTO events(id,event_id,live_id,body) VALUES(?,?,?,?)");
                q.num(1, row.id).text(2, row.event.at("event_id").get<std::string>()).text(3, row.live).text(4, row.event.dump()).row();
            }
            {
                // One fact row projected before like_count existed; the v2 migration must repair it.
                Statement q(legacy, "INSERT INTO analytics_events(seq,event_id,live_id,user_id,user_name,occurred_at_ms,kind,gift_quantity,gift_unit_price,recipient_id,user_level,fans_club) VALUES(4,'e4','aaa','123','观众',?,'like',0,NULL,'',NULL,'null')");
                q.num(1, T + 4000).row();
            }
            {
                // A screen chat fact projected before it counted as chat; v3 must reclassify it.
                Statement q(legacy, "INSERT INTO analytics_events(seq,event_id,live_id,user_id,user_name,occurred_at_ms,kind,gift_quantity,gift_unit_price,recipient_id,user_level,fans_club) VALUES(5,'e8','aaa','123','观众',?,'screen_chat',0,NULL,'',NULL,'null')");
                q.num(1, T + 4500).row();
            }
            if (sqlite3_exec(legacy, "INSERT INTO schema_migrations VALUES('analytics_v1')", nullptr, nullptr, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(legacy));
            sqlite3_close(legacy);
        } catch (...) { sqlite3_close(legacy); throw; }
    }

    {
        Store store;
        const auto aaa = store.sessions("aaa", {});
        check(aaa.at("total") == "2", "Journal boundaries must split historical broadcasts");
        check(aaa["items"][0]["started_at_ms"] == T + 4000 && aaa["items"][0]["ended_at_ms"] == T + 5000 &&
              aaa["items"][0]["start_source"] == "backfill" && aaa["items"][0]["end_source"] == "backfill",
              "A stopped room must have its last session closed at the final observed event");
        check(aaa["items"][0]["stats"]["like_count"] == "7", "The v2 migration must repair like counts on old fact rows");
        check(aaa["items"][0]["stats"]["chat_count"] == "1", "The v3 migration must reclassify screen chat facts as chat");
        check(aaa["items"][1]["started_at_ms"] == T + 1000 && aaa["items"][1]["ended_at_ms"] == T + 3000 &&
              aaa["items"][1]["end_source"] == "backfill", "The labeled end-of-live event must close the first session");
        check(aaa["items"][1]["stats"]["peak_online"] == 30, "Backfill must recover peak online from the journal");
        check(aaa["items"][1]["room_id"] == "room-a1" && aaa["items"][0]["room_id"] == "room-a2",
              "Sessions must keep their room identities");

        const auto bbb = store.sessions("bbb", {});
        check(bbb.at("total") == "1" && bbb["items"][0]["status"] == "live" &&
              bbb["items"][0]["stats"]["peak_online"] == 42,
              "A collecting room must keep its last session open with its peak");
        check(store.sessions("demo", {}).at("total") == "0", "Non-douyin history must not produce sessions");
    }
    {
        Store reopened;
        check(reopened.sessions("aaa", {}).at("total") == "2" && reopened.sessions("bbb", {}).at("total") == "1",
              "Reopening must not rerun the backfill or duplicate sessions");
    }
}
} // namespace

int main() {
    try {
        sessions_track_boundaries_and_stats();
        sessions_backfill_from_journal();
        std::cout << "session_store tests passed: boundaries, rotation, metadata, replay, backfill and pagination\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "session_store test failed: " << error.what() << '\n';
        return 1;
    }
}
