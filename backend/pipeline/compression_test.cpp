// Test transparent body compression at the Store boundary, on an owned temporary DB.
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
            directory=base/("douyin-compression-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            if(std::filesystem::create_directory(directory))break;
            directory.clear();
        }
        if(directory.empty())throw std::runtime_error("Cannot create isolated test directory");
#else
        char pattern[] = "/tmp/douyin-compression-test-XXXXXX";
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

void set_compression(const char* value) {
#ifdef _WIN32
    check(_putenv_s("COMPRESSION", value) == 0, "Cannot configure compression mode");
#else
    check(::setenv("COMPRESSION", value, 1) == 0, "Cannot configure compression mode");
#endif
}

class RawSql {
    sqlite3* db=nullptr;
public:
    explicit RawSql(const std::filesystem::path& path) {
        check(sqlite3_open(path.string().c_str(),&db)==SQLITE_OK,"Open isolated compression database");
        register_body_codecs(db);
        check(sqlite3_create_function_v2(db,"feed_visible",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,sqlite_feed_visible,nullptr,nullptr,nullptr)==SQLITE_OK,"Register feed_visible");
    }
    ~RawSql(){sqlite3_close(db);}
    void exec(const std::string& sql){check(sqlite3_exec(db,sql.c_str(),nullptr,nullptr,nullptr)==SQLITE_OK,std::string(sqlite3_errmsg(db))+" in: "+sql);}
    json rows(const std::string& sql) {
        sqlite3_stmt* raw=nullptr;const int prepared=sqlite3_prepare_v2(db,sql.c_str(),-1,&raw,nullptr);
        check(prepared==SQLITE_OK,std::string(sqlite3_errmsg(db))+" (rc "+std::to_string(prepared)+" "+sqlite3_errstr(prepared)+") in: "+sql);
        std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> statement(raw,sqlite3_finalize);
        json rows=json::array();int status;
        while((status=sqlite3_step(raw))==SQLITE_ROW) {
            json row=json::array();
            for(int i=0;i<sqlite3_column_count(raw);++i) {
                const auto value=sqlite3_column_text(raw,i);row.push_back(value?json(reinterpret_cast<const char*>(value)):json(nullptr));
            }
            rows.push_back(std::move(row));
        }
        check(status==SQLITE_DONE,std::string(sqlite3_errmsg(db))+" (status "+std::to_string(status)+") in: "+sql);return rows;
    }
};

pipeline::RawFrameEnvelope compression_frame(const std::string& live, const json& event) {
    static uint64_t sequence = 0;
    pipeline::RawFrameEnvelope result;
    result.set_schema_version(1);
    result.set_frame_id("compression-frame-" + std::to_string(++sequence));
    result.set_live_id(live);
    result.set_room_id("internal-" + live);
    result.set_source("douyin");
    result.set_session_id("compression-test");
    result.set_session_seq(sequence);
    result.set_received_at_ms(1700000000000LL + sequence);
    result.set_kind("upstream_frame");
    result.set_desired_version(1);
    result.set_payload(event.dump());
    result.set_payload_sha256(digest(result.payload()));
    return result;
}

void deliver(Store& store, const std::string& live, const json& event, bool replay=false) {
    const auto envelope = compression_frame(live, event);
    store.commit(envelope, envelope.SerializeAsString(), json::array({event}), json::array(), replay);
}

json chat_event(const std::string& event_id, const std::string& content) {
    return {{"event_id", event_id}, {"type", "chat"}, {"method", "WebcastChatMessage"},
            {"user_id", "1001"}, {"user_name", "压缩测试观众"}, {"content", content},
            {"timestamp", int64_t(1700000000000)},
            // The filler keeps details above the 64-byte compression floor.
            {"_details", {{"payload_base64", "CAE="}, {"decoded", {{"content", content}, {"filler", std::string(96, 'x')}}}}}};
}

json gift_event(const std::string& event_id, const std::string& display_id, int64_t count, bool final=false, const std::string& content="小心心") {
    return {{"event_id", event_id}, {"display_id", display_id}, {"type", "gift"},
            {"user_id", "1001"}, {"user_name", "压缩测试观众"}, {"user_level", nullptr},
            {"fans_club", {{"member", nullptr}, {"level", nullptr}, {"name", ""}, {"status", nullptr}}},
            {"timestamp", int64_t(1700000000000)}, {"content", content},
            {"gift_count", count}, {"gift_final", final}};
}

#include "compression_test.inc"
#include "codec_regression_test.inc"
}

int main() {
    auto run=[](const char* name,void(*test)()){std::cerr<<"[test] "<<name<<std::endl;test();};
    run("round_trips",compression_round_trips_through_store);
    run("udfs",compression_udfs_decode_inside_sql);
    run("legacy",compression_reads_legacy_plain_rows);
    run("replay",compression_replay_hydration_stays_valid);
    run("migration",compression_migrates_legacy_rows_in_batches);
    run("corrupt",compression_rejects_corrupt_blobs);
    run("off_then_migrates",compression_off_stores_plain_then_migrates);
    run("manual_compact",compression_completed_pass_can_be_compacted_manually);
    run("reenable_completed",compression_reenabled_after_completed_pass);
    run("idle_checkpoint",compression_idle_does_not_checkpoint_active_readers);
    run("offline_edits",compression_revisits_offline_edits_before_the_cursor);
    run("large_bodies",compression_large_bodies_round_trip);
    run("frame_limit",compression_frame_limit_is_preserved);
    std::cout << "compression tests passed" << std::endl;
    return 0;
}
