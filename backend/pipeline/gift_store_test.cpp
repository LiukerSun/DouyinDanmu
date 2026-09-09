// Test the production Store at its public boundary, using only an owned temporary DB.
#define main pipeline_application_main
#include "main.cpp"
#undef main

#include <cstdlib>
#include <optional>
#include <set>

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
            directory=base/("douyin-native-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            if(std::filesystem::create_directory(directory))break;
            directory.clear();
        }
        if(directory.empty())throw std::runtime_error("Cannot create isolated test directory");
#else
        char pattern[] = "/tmp/douyin-gift-store-test-XXXXXX";
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

const std::string room_a = "gift-store-room-a";
const std::string room_b = "gift-store-room-b";
const std::string uid = "9007199254740993";

json gift(const std::string& event_id, const std::string& display_id,
          int64_t count, bool final = false) {
    return {{"event_id", event_id}, {"display_id", display_id}, {"type", "gift"},
            {"user_id", uid}, {"user_name", "离线测试观众"}, {"user_level", nullptr},
            {"fans_club", {{"member", nullptr}, {"level", nullptr}, {"name", ""}, {"status", nullptr}}},
            {"timestamp", int64_t(1700000000000)}, {"content", "小心心"},
            {"gift_count", count}, {"gift_final", final}};
}

pipeline::RawFrameEnvelope frame(const std::string& live, const json& event) {
    static uint64_t sequence = 0;
    pipeline::RawFrameEnvelope result;
    result.set_schema_version(1);
    result.set_frame_id("gift-store-frame-" + std::to_string(++sequence));
    result.set_live_id(live);
    result.set_room_id("internal-" + live);
    result.set_source("douyin");
    result.set_session_id("isolated-store-test");
    result.set_session_seq(sequence);
    result.set_received_at_ms(1700000000000LL + sequence);
    result.set_kind("upstream_frame");
    result.set_desired_version(1);
    // Parsing is tested separately. This payload identifies the raw test delivery.
    result.set_payload(event.dump());
    result.set_payload_sha256(digest(result.payload()));
    return result;
}

void deliver(Store& store, const pipeline::RawFrameEnvelope& envelope, const json& event) {
    store.commit(envelope, envelope.SerializeAsString(), json::array({event}), json::array());
}

pipeline::RawFrameEnvelope deliver(Store& store, const std::string& live, const json& event) {
    const auto envelope = frame(live, event);
    deliver(store, envelope, event);
    return envelope;
}

json display_event(const json& snapshot, const std::string& display_id) {
    json found;
    size_t count = 0;
    for (const auto& event : snapshot.at("events")) {
        if (event.at("display_id") == display_id) { found = event; ++count; }
    }
    check(count == 1, "Snapshot must contain exactly one row for " + display_id);
    return found;
}

void check_stats(Store& store, const std::string& live, int64_t groups, int64_t quantity) {
    const auto stats = store.stats(live);
    check(stats.at("gift") == groups, "Gift statistic must count groups for " + live);
    check(stats.at("gift_quantity") == quantity, "Gift quantity must sum cumulative high-water marks for " + live);
    check(stats.at("total") == groups, "Gift progress events must not inflate logical total for " + live);
    check(stats.at("chat") == 0 && stats.at("enter") == 0 && stats.at("like") == 0,
          "Gift progress must leave unrelated statistics unchanged");
    check(store.snapshot(live).at("stats") == stats, "Snapshot and stats must agree");
    bool found = false;
    for (const auto& room : store.rooms()) {
        if (room.at("live_id") == live) {
            found = true;
            check(room.at("stats") == stats, "Room list and stats must agree");
        }
    }
    check(found, "Test room must exist in rooms()");
}

void check_journal_cursor(Store& store, const std::string& live) {
    const auto journal = store.events(live, 0, 200);
    const auto snapshot = store.snapshot(live);
    check(!journal.empty(), "Fixture must create a nonempty journal");
    check(snapshot.at("through_seq") == journal.back().at("seq"),
          "Snapshot cursor must cover all committed journal updates");
    int64_t cursor = 0;
    for (const auto& event : journal) {
        check(event.at("seq").is_string(), "Sequence must remain a string");
        const auto next = std::stoll(event.at("seq").get<std::string>());
        check(next > cursor, "Journal must remain append-only in increasing sequence order");
        check(event.at("live_id") == live, "Journal must not include another room");
        cursor = next;
    }
    check(store.events(live, cursor).empty(), "Replay from final journal cursor must be empty");
}

void gift_progress_persists_once_per_group() {
    TemporaryDatabase database;
    const auto first = gift("a-event-1", "combo-1", 1);
    const auto second = gift("a-event-2", "combo-1", 2);
    const auto third = gift("a-event-3", "combo-1", 3);
    const auto final = gift("a-event-final", "combo-1", 3, true);
    pipeline::RawFrameEnvelope final_frame;
    json saved_a, saved_b;
    {
        Store store;
        store.target(room_a, "douyin", true);
        store.target(room_b, "douyin", true);
        deliver(store, room_a, first);
        deliver(store, room_a, second);
        const auto third_frame = deliver(store, room_a, third);
        auto snapshot = store.snapshot(room_a);
        check(snapshot.at("events").size() == 1, "1 -> 2 -> 3 gift progress must display one row");
        const auto combined = display_event(snapshot, "combo-1");
        check(combined.at("gift_count") == 3 && combined.at("gift_final") == false,
              "Open combo must display its latest cumulative count");
        check(combined.at("user_id") == uid && combined.at("user_id").is_string(),
              "Gift sender UID must preserve all 64-bit digits");
        check_stats(store, room_a, 1, 3);
        const auto first_journal = store.events(room_a, 0);
        check(first_journal.size() == 3, "Distinct gift progress messages must remain in the replay journal");
        check(first_journal[0].at("gift_count") == 1 && first_journal[1].at("gift_count") == 2 &&
              first_journal[2].at("gift_count") == 3, "Later progress must not rewrite earlier journal entries");

        const auto before_duplicate = store.overview();
        deliver(store, third_frame, third);
        check(store.overview() == before_duplicate, "Same frame must not add a receipt or journal event");
        check(store.snapshot(room_a) == snapshot, "Same frame must not mutate the current display state");
        const auto duplicate_event_frame = deliver(store, room_a, third);
        check(store.events(room_a, 0) == first_journal, "Same event in a new frame must not append another event");
        check(store.receipts(json::array({third_frame.frame_id(), duplicate_event_frame.frame_id()})).size() == 2,
              "Both independently delivered frames must have durable receipts");
        check_stats(store, room_a, 1, 3);

        const auto older = gift("a-late-2", "combo-1", 2);
        deliver(store, room_a, older);
        check(display_event(store.snapshot(room_a), "combo-1").at("gift_count") == 3,
              "Late lower count must not reduce the displayed count");
        check(store.events(room_a, 0).back().at("gift_count") == 3,
              "Replay must publish normalized high-water count after a late update");
        check_stats(store, room_a, 1, 3);

        final_frame = deliver(store, room_a, final);
        const auto journal_before_repeat = store.events(room_a, 0);
        deliver(store, final_frame, final);
        deliver(store, room_a, final);
        check(store.events(room_a, 0) == journal_before_repeat, "Repeated final frame/event must remain idempotent");
        deliver(store, room_a, gift("a-second-final", "combo-1", 3, true));
        deliver(store, room_a, gift("a-late-open", "combo-1", 2, false));
        const auto ended = display_event(store.snapshot(room_a), "combo-1");
        check(ended.at("gift_count") == 3 && ended.at("gift_final") == true,
              "Final state must be sticky when lower/open progress arrives later");
        check(store.events(room_a, 0).back().at("gift_final") == true,
              "Journal consumers must also see sticky final state");
        check_stats(store, room_a, 1, 3);

        deliver(store, room_a, gift("a-other-group", "combo-2", 2, true));
        check(store.snapshot(room_a).at("events").size() == 2, "Two group IDs must remain two display rows");
        check(display_event(store.snapshot(room_a), "combo-2").at("gift_count") == 2,
              "Second group must keep its independent quantity");
        check_stats(store, room_a, 2, 5);

        deliver(store, room_b, gift("b-event-1", "combo-1", 7, false));
        check(store.snapshot(room_b).at("events").size() == 1, "Another room must start an independent group");
        check(display_event(store.snapshot(room_b), "combo-1").at("gift_count") == 7,
              "Same display ID in another room must not share a high-water mark");
        check_stats(store, room_a, 2, 5);
        check_stats(store, room_b, 1, 7);
        check_journal_cursor(store, room_a);
        check_journal_cursor(store, room_b);
        saved_a = store.snapshot(room_a);
        saved_b = store.snapshot(room_b);
    }
    {
        Store reopened;
        check(reopened.snapshot(room_a) == saved_a && reopened.snapshot(room_b) == saved_b,
              "Display rows, statistics and cursors must survive database reopen");
        const auto journal_before = reopened.events(room_a, 0);
        deliver(reopened, final_frame, final);
        check(reopened.events(room_a, 0) == journal_before, "Frame receipts must survive database reopen");
        deliver(reopened, room_a, gift("a-after-restart", "combo-1", 4, false));
        const auto continued = display_event(reopened.snapshot(room_a), "combo-1");
        check(continued.at("gift_count") == 4 && continued.at("gift_final") == true,
              "Restart must preserve the group high-water mark and sticky final flag");
        check(reopened.snapshot(room_a).at("events").size() == 2, "Restarted progress must update its original row");
        check_stats(reopened, room_a, 2, 6);
        check_stats(reopened, room_b, 1, 7);
        check(reopened.snapshot(room_b) == saved_b, "Updating one room must leave another room unchanged");
        const auto replay = reopened.events(room_a, std::stoll(saved_a.at("through_seq").get<std::string>()));
        check(replay.size() == 1 && replay[0].at("gift_count") == 4 && replay[0].at("gift_final") == true,
              "Reconnection cursor must replay exactly the new normalized update");
        check_journal_cursor(reopened, room_a);
    }
}

void legacy_database_migrates_once() {
    TemporaryDatabase database;
    auto legacy_chat = gift("legacy-chat", "unused", 0);
    legacy_chat["type"] = "chat";
    legacy_chat["content"] = "升级前的弹幕[色]";
    auto legacy_gift_a = gift("legacy-gift-a", "unused", 2);
    auto legacy_gift_b = gift("legacy-gift-b", "unused", 4);
    json legacy_events = json::array({legacy_chat, legacy_gift_a, legacy_gift_b});
    for (auto& event : legacy_events) {
        // The old schema has no combo projection or new profile fields.
        for (const char* field : {"display_id", "gift_final", "user_level", "fans_club"}) event.erase(field);
        event["live_id"] = room_a;
        event["room_id"] = "internal-" + room_a;
        event["source"] = "douyin";
        event["received_at_ms"] = int64_t(1700000000000);
        event["persisted_at_ms"] = int64_t(1700000000050);
    }
    {
        sqlite3* legacy = nullptr;
        if (sqlite3_open(database.path().string().c_str(), &legacy) != SQLITE_OK)
            throw std::runtime_error("Cannot open owned legacy fixture database");
        try {
            const char* schema = R"SQL(
              CREATE TABLE rooms(live_id TEXT PRIMARY KEY,source TEXT NOT NULL,title TEXT NOT NULL,enabled INTEGER NOT NULL DEFAULT 1,version INTEGER NOT NULL DEFAULT 1,status TEXT NOT NULL DEFAULT 'connecting',detail TEXT NOT NULL DEFAULT '',chat INTEGER DEFAULT 0,gift INTEGER DEFAULT 0,enter_count INTEGER DEFAULT 0,likes INTEGER DEFAULT 0,online INTEGER DEFAULT 0,total INTEGER DEFAULT 0,updated_at INTEGER DEFAULT 0);
              CREATE TABLE events(id INTEGER PRIMARY KEY AUTOINCREMENT,event_id TEXT UNIQUE NOT NULL,live_id TEXT NOT NULL,body TEXT NOT NULL);
            )SQL";
            if (sqlite3_exec(legacy, schema, nullptr, nullptr, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(legacy));
            {
                Statement room(legacy, "INSERT INTO rooms(live_id,source,title,chat,gift,total) VALUES(?,'douyin','升级前直播间',1,2,3)");
                room.text(1, room_a).row();
            }
            const int64_t ids[] = {7, 11, 19};
            for (size_t index = 0; index < legacy_events.size(); ++index) {
                const auto& event = legacy_events[index];
                Statement row(legacy, "INSERT INTO events(id,event_id,live_id,body) VALUES(?,?,?,?)");
                row.num(1, ids[index]).text(2, event.at("event_id").get<std::string>())
                   .text(3, room_a).text(4, event.dump()).row();
                legacy_events[index]["seq"] = std::to_string(ids[index]);
            }
            sqlite3_close(legacy);
        } catch (...) { sqlite3_close(legacy); throw; }
    }

    json saved_snapshot, saved_journal;
    {
        Store migrated;
        const auto snapshot = migrated.snapshot(room_a);
        check(snapshot.at("events") == legacy_events, "Migration must preserve original event bodies and sequence IDs");
        check(migrated.events(room_a, 0) == legacy_events, "Migration must leave the existing journal unchanged");
        check(snapshot.at("through_seq") == "19", "Migrated snapshot cursor must use the highest old sequence");
        const auto stats = migrated.stats(room_a);
        check(stats.at("chat") == 1 && stats.at("gift") == 2 && stats.at("total") == 3,
              "Migration must preserve existing logical message counts");
        check(stats.at("gift_quantity") == 6, "Migration must initialize quantity from old gift counts 2 + 4");
        check(migrated.overview().at("events") == 3 && migrated.overview().at("frames") == 0,
              "Migration must not replay ingestion or invent delivery receipts");

        auto duplicate = legacy_gift_a;
        for (const char* field : {"display_id", "gift_final", "user_level", "fans_club"}) duplicate.erase(field);
        deliver(migrated, room_a, duplicate);
        check(migrated.events(room_a, 0) == legacy_events && migrated.stats(room_a) == stats,
              "Re-delivery of an old event must remain idempotent after migration");
        deliver(migrated, room_a, gift("new-after-migration-1", "new-combo", 2));
        deliver(migrated, room_a, gift("new-after-migration-2", "new-combo", 5, true));
        check(migrated.snapshot(room_a).at("events").size() == 4,
              "New combo must add one display row alongside all old events");
        const auto after = migrated.stats(room_a);
        check(after.at("chat") == 1 && after.at("gift") == 3 && after.at("total") == 4 &&
              after.at("gift_quantity") == 11, "New combo must add only 5 units to legacy quantity 6");
        saved_snapshot = migrated.snapshot(room_a);
        saved_journal = migrated.events(room_a, 0);
    }
    {
        Store reopened;
        check(reopened.snapshot(room_a) == saved_snapshot && reopened.events(room_a, 0) == saved_journal,
              "Reopening a migrated database must not replay migration or duplicate projection rows");
        check(reopened.stats(room_a).at("gift_quantity") == 11,
              "Migration must not rescan progress journal 2 + 5 as extra gift quantity on restart");
    }
}

void sparse_progress_matches_only_an_unambiguous_recipient() {
    TemporaryDatabase database;
    Store store;
    store.target(room_a, "douyin", true);
    const std::string group_key = "shared-sender-gift-group";
    const auto progress = [&](const std::string& event_id, const std::string& display_id,
                              int64_t count, bool combo, const std::string& recipient,
                              bool final, int64_t repeat_end) {
        auto event = gift(event_id, display_id, count, final);
        event["gift_group_key"] = group_key;
        event["gift_combo"] = combo;
        event["recipient_id"] = recipient;
        event["repeat_end"] = repeat_end;
        return event;
    };

    deliver(store, room_a, progress("sparse-first", "anchor-combo", 1, true, "anchor", false, 0));
    deliver(store, room_a, progress("sparse-second", "sparse-second", 2, false, "", true, 0));
    auto snapshot = store.snapshot(room_a);
    check(snapshot.at("events").size() == 1,
          "Sparse progress with the same group key must reuse the unique compatible recipient display");
    auto combined = display_event(snapshot, "anchor-combo");
    check(combined.at("gift_count") == 2 && combined.at("gift_combo") == true,
          "Sparse progress must retain the established combo classification");
    check(combined.at("gift_final") == false,
          "A sparse non-combo fallback final flag must not finalize an established combo when repeat_end is zero");
    check(combined.at("recipient_id") == "anchor",
          "An omitted recipient must preserve the established recipient identity");
    check_stats(store, room_a, 1, 2);

    deliver(store, room_a, progress("sparse-final", "sparse-final", 3, false, "", true, 1));
    snapshot = store.snapshot(room_a);
    check(snapshot.at("events").size() == 1, "Sparse final progress must still update one existing row");
    combined = display_event(snapshot, "anchor-combo");
    check(combined.at("gift_count") == 3 && combined.at("gift_combo") == true &&
          combined.at("gift_final") == true, "Explicit repeat_end must finalize the established combo at quantity 3");
    check_stats(store, room_a, 1, 3);
    const auto journal = store.events(room_a, 0);
    check(journal.size() == 3, "Sparse progress must preserve each unique journal event");
    check(journal[1].at("display_id") == "anchor-combo" && journal[1].at("gift_final") == false &&
          journal[2].at("display_id") == "anchor-combo" && journal[2].at("gift_final") == true,
          "Journal consumers must receive the resolved display identity and corrected final state");

    deliver(store, room_a, progress("cohost-first", "cohost-combo", 4, true, "cohost", false, 0));
    snapshot = store.snapshot(room_a);
    check(snapshot.at("events").size() == 2,
          "Explicitly different recipients must remain independent despite a shared group key");
    check(display_event(snapshot, "anchor-combo").at("gift_count") == 3 &&
          display_event(snapshot, "cohost-combo").at("gift_count") == 4,
          "Adding another recipient must not merge or overwrite existing quantities");
    check_stats(store, room_a, 2, 7);

    deliver(store, room_a, progress("ambiguous-progress", "ambiguous-progress", 5, false, "", true, 0));
    snapshot = store.snapshot(room_a);
    check(snapshot.at("events").size() == 3,
          "Unknown recipient with two compatible groups must remain separate rather than guess a recipient");
    check(display_event(snapshot, "anchor-combo").at("gift_count") == 3 &&
          display_event(snapshot, "cohost-combo").at("gift_count") == 4 &&
          display_event(snapshot, "ambiguous-progress").at("gift_count") == 5,
          "Ambiguous progress must not change either known recipient's group");
    check_stats(store, room_a, 3, 12);
    check_journal_cursor(store, room_a);
}
void message_history_search() {
    TemporaryDatabase database; Store store;
    store.target("111","douyin",true);store.target("222","douyin",true);store.target("demo","demo",true);
    for(int i=0;i<120;i++) {
        auto e=gift("search-"+std::to_string(i),"search-"+std::to_string(i),0);
        e["type"]="chat";e["content"]=i==0?"literal %_ marker":"hello";
        e["user_id"]=i<110?uid:"9007199254740992";
        e["user_level"]=i%3==0?json(nullptr):json(i%60);e["fans_club"]["level"]=i%20;
        deliver(store,i%2==0?"111":"222",e);
    }
    deliver(store,"111",gift("g1","same-combo",1));deliver(store,"111",gift("g2","same-combo",3,true));
    auto demo=gift("demo-entry","demo-entry",0);demo["type"]="chat";deliver(store,"demo",demo);
    store.target("222","douyin",false);
    MessageSearch q;q.limit=17;auto first=store.search_messages(q);
    check(first["total"]==121,"History must include records beyond 100 and stopped rooms, with one gift view");
    std::set<std::string> seen;auto page=first;
    do {
        for(const auto& e:page["events"])check(seen.insert(e["event_id"].get<std::string>()).second,"Pagination duplicated a message");
        if(page["next_before"].is_null())break;
        q.before=std::stoll(page["next_before"].get<std::string>());page=store.search_messages(q);
    }while(true);
    check(seen.size()==121,"Pagination must reach all saved messages");
    q=MessageSearch{};q.user=uid;check(store.search_messages(q)["total"]==111,"User search must match precise UID across rooms");
    q.type="gift";auto gifts=store.search_messages(q);check(gifts["total"]==1 && gifts["events"][0]["gift_count"]==3,"Gift history must show the final combo once");
    q=MessageSearch{};q.keyword="%_";check(store.search_messages(q)["total"]==1,"Search metacharacters must be literal");
    q.keyword="' OR 1=1 --";check(store.search_messages(q)["total"]==0,"Search must bind input");
    q=MessageSearch{};q.wealth_unknown=true;check(store.search_messages(q)["total"]==41,"Unknown wealth must remain distinct from zero");
    q=MessageSearch{};q.room="222";q.type="chat";q.wealth_min=20;q.wealth_max=29;q.fans_min=5;q.fans_max=9;
    int expected=0;for(int i=0;i<120;i++)if(i%2 && i%3!=0 && i%60>=20 && i%60<=29 && i%20>=5 && i%20<=9)expected++;
    check(store.search_messages(q)["total"]==expected,"Room, type and both level ranges must combine");
    httplib::Request req;req.params.emplace("before","1oops");bool rejected=false;
    try{read_message_search(req);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Reject malformed cursors");
    q.wealth_min=40;q.wealth_max=10;rejected=false;try{store.search_messages(q);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Reject reversed ranges");
}

void saved_detail_upgrade() {
    TemporaryDatabase database;Store store;store.target("111","douyin",true);
    GiftSortMessage sort;sort.set_message_type(3);sort.mutable_scene_insert_strategy()->add_gift_ids(4095);
      auto unknown=observed_event("old-unknown","WebcastGiftSortMessage",sort.SerializeAsString(),1234);
      unknown["_details"]["field_analysis"]={{"version",2},{"entries",json::array()}};
    deliver(store,"111",unknown);
    ChatMessage chat;chat.set_content("saved chat");auto known=observed_event("old-chat","WebcastChatMessage",chat.SerializeAsString(),1234);
    known["type"]="chat";known["user_id"]="9007199254740993";known["user_name"]="repaired name";known["user_level"]=42;known["content"]="repaired chat";deliver(store,"111",known);
    const auto before=store.snapshot("111"),stats=store.stats("111"),detail=store.message_detail("old-unknown");
    const auto preview=redecode_details(false);check(preview["updated"]==2 && preview["promoted"]==1,"Dry-run should identify saved schema upgrades");
    check(store.snapshot("111")==before && store.message_detail("old-unknown")==detail,"Dry-run must not modify records");
      const auto applied=redecode_details(true);check(applied["failed"]==0 && applied["updated"]==2,"Apply must enrich both known and unknown saved payloads");
      check(store.message_detail("old-unknown")["details"]["field_analysis"]["version"]==3 && store.message_detail("old-unknown")["event"]["parser_version"]==event_parser_version,"Version 2 details must upgrade alongside records without analysis");
    const auto after=store.snapshot("111");check(after["through_seq"]==before["through_seq"] && store.stats("111")==stats,"Upgrade must not replay events, cursors or business counters");
    check(after["events"].size()==1 && store.message_detail("old-unknown")["event"]["type"]=="gift_notice","Resolved configuration is enriched in storage without becoming an audience action");
    check(after["events"][0]["user_name"]=="repaired name" && after["events"][0]["user_level"]==42 && after["events"][0]["content"]=="repaired chat","Existing user and business repairs must survive detail upgrades");
    check(store.message_detail("old-unknown")["details"]["payload_base64"]==detail["details"]["payload_base64"],"Original payload must remain byte-identical");
    check(redecode_details(true)["updated"]==0,"Repeated detail upgrade must be idempotent");
    RoomMessage roomNotice;roomNotice.set_content(" ");
    auto* display=roomNotice.mutable_common()->mutable_display_text();display->set_default_pattern("大家在说 {0:string}");
    auto* piece=display->add_pieces();piece->set_type(1);piece->set_string_value("好看");
    auto oldNotice=observed_event("old-notice","WebcastRoomMessage",roomNotice.SerializeAsString(),1234);
    decode_observed(oldNotice,roomNotice.SerializeAsString());oldNotice["content"]=" ";oldNotice["parser_version"]=5;
    deliver(store,"111",oldNotice);
    const auto noticeStats=store.stats("111"),noticeBefore=store.message_detail("old-notice"),noticeCursor=store.snapshot("111")["through_seq"];
    check(redecode_details(false)["updated"]==1 && store.message_detail("old-notice")==noticeBefore,"Parser-only upgrades must be discovered without modifying data in dry-run");
    check(redecode_details(true)["updated"]==1 && store.message_detail("old-notice")["event"]["content"]=="大家在说 好看","Saved blank notices must receive their rendered content");
    check(store.snapshot("111")["events"].back()["content"]=="大家在说 好看","Historical notice repair must update the feed view too");
    check(store.message_detail("old-notice")["details"]["payload_base64"]==noticeBefore["details"]["payload_base64"] && store.stats("111")==noticeStats && store.snapshot("111")["through_seq"]==noticeCursor,"Notice repair must preserve raw payload, counters and cursor");
    check(redecode_details(true)["updated"]==0,"Notice repair must be idempotent");
    FansclubMessage fans;fans.set_type(6);fans.mutable_user()->mutable_fansclub()->mutable_data()->set_level(8);
    auto oldFans=observed_event("old-fans","WebcastFansclubMessage",fans.SerializeAsString(),1234);
    decode_observed(oldFans,fans.SerializeAsString());oldFans["content"]="粉丝团事件";oldFans["parser_version"]=6;
    deliver(store,"111",oldFans);
    const auto fansStats=store.stats("111"),fansBefore=store.message_detail("old-fans");
    check(redecode_details(false)["updated"]==1 && store.message_detail("old-fans")==fansBefore,"Fansclub summary repair must be read-only in preview");
    check(redecode_details(true)["updated"]==1 && store.snapshot("111")["events"].back()["content"]=="粉丝团资料 · 当前等级 Lv8（具体动作未确认）","Historical generic fansclub events must gain known context");
    check(store.stats("111")==fansStats && store.message_detail("old-fans")["details"]["payload_base64"]==fansBefore["details"]["payload_base64"],"Fansclub repair must preserve counters and source payload");
    check(redecode_details(true)["updated"]==0,"Fansclub repair must be idempotent");
    auto unrelated=observed_event("unchanged-v6-chat","WebcastChatMessage",chat.SerializeAsString(),1234);
    decode_observed(unrelated,chat.SerializeAsString());unrelated["parser_version"]=6;deliver(store,"111",unrelated);
    check(redecode_details(true)["updated"]==0,"Notification-only parser upgrades must not rewrite unrelated chat history");
    bool rejected=false;try{decode_saved_base64("AA==junk");}catch(...){rejected=true;}check(rejected,"Trailing base64 corruption must be rejected");
}

void room_state_snapshot() {
    TemporaryDatabase database;Store store;store.target("111","douyin",true);store.target("222","douyin",true);
    auto behavior=gift("behavior","behavior",0);behavior["type"]="chat";deliver(store,"111",behavior);
    auto state=behavior;state["type"]="stream";state["method"]="WebcastRoomStreamAdaptationMessage";
    for(int i=0;i<110;i++){state["event_id"]="state-"+std::to_string(i);state["display_id"]=state["event_id"];deliver(store,"111",state);}
    state["event_id"]="foreign";state["display_id"]="foreign";deliver(store,"222",state);
    const auto snapshot=store.snapshot("111");
    check(snapshot["events"].size()==1 && snapshot["events"][0]["event_id"]=="behavior","State floods must not displace snapshot behaviors");
    check(snapshot["state_events"].empty(),"Stream adaptation is backend-only, not a header metric");
    check(snapshot["through_seq"]==store.events("111",0).back()["seq"],"Replay cursor must include suppressed state");
    MessageSearch q;q.room="111";q.type="stream";q.deliveries=true;
    check(store.search_messages(q)["total"]==0 && store.events("111",0).size()==111,"All state updates remain stored but are excluded even from explicit frontend type queries");
    auto social=behavior;social["event_id"]="old-follow";social["display_id"]="old-follow";social["type"]="social";social["action"]=1;deliver(store,"111",social);
    social["event_id"]="share";social["display_id"]="share";social["social_action"]="share";deliver(store,"111",social);
    social["event_id"]="unknown-action";social["display_id"]="unknown-action";social["social_action"]="unknown";deliver(store,"111",social);
    q.type="follow";check(store.search_messages(q)["total"]==1,"Historical follow must be filterable");
    q.keyword="关注";check(store.search_messages(q)["total"]==1,"Historical follow labels must be keyword-searchable");q.keyword="";
    q.type="share";check(store.search_messages(q)["total"]==1,"Share evidence must override action code");
    q.type="social";check(store.search_messages(q)["total"]==1,"Other social actions must remain separately filterable");
}

void full_monitor_replay() {
    TemporaryDatabase database;Store store;store.target("111","douyin",true);
    Response response;
    ChatMessage chat;chat.set_content("old chat");chat.mutable_user()->set_id(123);
    auto* a=response.add_messageslist();a->set_method("WebcastChatMessage");a->set_msgid(101);a->set_payload(chat.SerializeAsString());
    auto* b=response.add_messageslist();b->set_method("WebcastUnmappedFutureMessage");b->set_msgid(102);b->set_payload(std::string("\x08\x01",2));
    PushFrame push;push.set_payload(response.SerializeAsString());auto envelope=frame("111",json::object());
    envelope.set_payload(push.SerializeAsString());envelope.set_payload_sha256(digest(envelope.payload()));const auto raw=envelope.SerializeAsString();
    const auto parsed=parse(envelope);auto legacy=parsed.first[0];legacy.erase("_details");legacy.erase("method");legacy.erase("parse_status");legacy.erase("has_details");
    store.commit(envelope,raw,json::array({legacy}),json::array({"unsupported: WebcastUnmappedFutureMessage"}));
    check(store.overview()["quarantine"]==1,"Old unsupported messages must start quarantined");
    store.commit(envelope,raw,parsed.first,parsed.second,true);
    check(store.stats("111")["chat"]==1 && store.stats("111")["total"]==2,"Replay must add unknown event without recounting chat");
    check(store.overview()["quarantine"]==0,"Recovered quarantine must be resolved without deletion");
    auto snapshot=store.snapshot("111");check(snapshot["events"].size()==1 && store.events("111",0).size()==2,"Unknown events stay persisted without entering the behavior feed");
    for(const auto& event:snapshot["events"])check(!event.contains("_details"),"Large payloads must not be streamed in snapshots");
    auto detail=store.message_detail(parsed.first[1]["event_id"]);
    check(detail["details"]["payload_base64"]=="CAE=","Detail API must preserve original bytes");
    check(store.message_detail(parsed.first[0]["event_id"])["event"]["has_details"]==true,"Replay must hydrate old event details");
    MessageSearch query;query.method="WebcastUnmappedFutureMessage";query.parse_status="unmapped";
    check(store.search_messages(query)["total"]==0,"Ordinary history must not expose unknown protocol messages");
    query.method="' OR 1=1 --";check(store.search_messages(query)["total"]==0,"Method filter must bind input");
    const auto stats=store.stats("111");store.commit(envelope,raw,parsed.first,parsed.second,true);
    check(store.stats("111")==stats && store.snapshot("111")["events"].size()==1,"Repeated replay must be idempotent");
    check(store.message_methods("111").size()==1,"Frontend method inventory must omit backend-only methods");
    check(store.message_detail("missing").is_null(),"Missing details must be explicit");
}
void presentation_delivery_and_paging() {
    TemporaryDatabase database;Store store;store.target("111","douyin",true);store.target("222","douyin",true);
    const std::vector<std::pair<std::string,std::string>> configs={
        {"gift_notice","WebcastGiftSortMessage"},{"notice","WebcastProfileViewMessage"},
        {"room_notice","WebcastRoomCommentTopicMessage"},{"protocol","WebcastCommonDotMessage"},
        {"game","WebcastProfitGameStatusMessage"},{"commerce","WebcastProductChangeMessage"},
        {"room_stats","WebcastRoomStatsMessage"},{"room_rank","WebcastRoomRankMessage"},
        {"banner","WebcastInRoomBannerMessage"},{"unknown","WebcastFutureMessage"},
        {"parse_error","FrameDecodeError"},{"notice","WebcastHotChatMessage"}};
    for(size_t i=0;i<configs.size();++i){
        auto event=gift("config-"+std::to_string(i),"config-"+std::to_string(i),0);
        event["type"]=configs[i].first;event["method"]=configs[i].second;
        event["_details"]={{"payload_base64","CAE="},{"decoded",{{"config","kept"}}}};
        deliver(store,"111",event);
    }
    const auto raw=store.events("111",0),stats=store.stats("111"),batch=store.frontend_batch("111",0,5);
    check(batch["events"].empty() && batch["state_events"].empty() && batch["through_seq"]==raw[4]["seq"],"Hidden-only batches advance the raw cursor without sending protocol payloads");
    auto cursor=std::stoll(batch["through_seq"].get<std::string>());
    while(cursor<std::stoll(raw.back()["seq"].get<std::string>()))cursor=std::stoll(store.frontend_batch("111",cursor,5)["through_seq"].get<std::string>());
    check(store.frontend_batch("111",cursor)["through_seq"]==std::to_string(cursor),"Caught-up subscriptions do not produce new cursors");
    for(int i=0;i<3;++i){auto event=gift("chat-"+std::to_string(i),"chat-"+std::to_string(i),0);event["type"]="chat";deliver(store,"111",event);}
    auto online=gift("online-1","online-1",0);online["type"]="online_count";online["online_count"]=9;deliver(store,"111",online);
    online["event_id"]="online-2";online["display_id"]="online-2";online["online_count"]=0;deliver(store,"111",online);
    online["event_id"]="foreign-online";online["display_id"]="foreign-online";online["online_count"]=999;deliver(store,"222",online);
    auto mixed=store.frontend_batch("111",cursor);
    check(mixed["events"].size()==3 && mixed["state_events"].size()==1 && mixed["state_events"][0]["online_count"]==0,"Resume delivers all behaviors and the latest room-local metric, including zero");
    check(mixed["state_events"][0].size()==6 && !mixed["state_events"][0].contains("content"),"State projection sends metric fields only");
    check(store.snapshot("111")["state_events"]==mixed["state_events"],"Snapshot and streaming expose the same compact room state");
    MessageSearch q;q.room="111";q.limit=2;q.deliveries=true;auto page=store.search_messages(q);
    check(page["total"]==3 && page["events"].size()==2 && !page["next_before"].is_null(),"History count and page limits use visible actions only");
    q.before=std::stoll(page["next_before"].get<std::string>());page=store.search_messages(q);
    check(page["total"]==3 && page["events"].size()==1 && page["next_before"].is_null(),"Hidden rows cannot create empty intermediate pages");
    q=MessageSearch{};q.room="111";q.method="WebcastGiftSortMessage";
    check(store.search_messages(q)["total"]==0,"Explicit method filtering cannot reintroduce configuration rows");
    for(size_t i=0;i<configs.size();++i)check(store.message_detail("config-"+std::to_string(i))["details"]["payload_base64"]=="CAE=","Hidden messages retain complete detail payloads");
    check(store.events("111",0).size()==raw.size()+5 && store.stats("111")["total"]==stats["total"].get<int>()+5,"Presentation never drops stored deliveries or their collection totals");
    for(const auto& method:{"WebcastRoomMessage","WebcastCommonTextMessage","WebcastNotifyMessage"}){
        auto event=gift(method,method,0);event["method"]=method;event["type"]=std::string(method)=="WebcastRoomMessage"?"room_notice":"notice";event["content"]="主播发布的重要通知";deliver(store,"111",event);
        event["content"]=" \t\n";check(!feed_visible(event),"Whitespace-only notices must not enter feed or history");
    }
    auto system=gift("ended","ended",0);system["type"]="system";system["method"]="WebcastControlMessage";system["content"]="直播已结束";deliver(store,"111",system);
    check(store.snapshot("111")["events"].size()==7,"Actual room notices and end-of-live events remain visible");
    q=MessageSearch{};q.room="111";q.type="notice";q.limit=2;
    auto notices=store.search_messages(q);
    check(notices["total"]==4 && notices["events"].size()==2 && !notices["next_before"].is_null(),"User notice category groups room, common and control messages before paging");
    q.before=std::stoll(notices["next_before"].get<std::string>());
    check(store.search_messages(q)["events"].size()==2,"Notice grouping preserves the second history page");
    for(const auto& type:{"emoji","episode_chat","audio_chat","screen_chat"}){
        auto event=gift(type,type,0);event["type"]=type;deliver(store,"111",event);
    }
    q=MessageSearch{};q.room="111";q.type="chat";
    check(store.search_messages(q)["total"]==7,"Chat category includes text, emoji and other chat transports");
    check(store.message_detail("audio_chat")["event"]["type"]=="audio_chat","Grouping never rewrites original event types");
}
} // namespace

void debug_startup_and_details() {
    check(!read_startup_options({}).debug,"Startup without a flag must hide diagnostics");
    check(read_startup_options({"--debug"}).debug,"Explicit startup flag enables diagnostics");
    check(!read_startup_options({"--redecode-details"}).apply,"Re-decode remains read-only by default");
    check(read_startup_options({"--replay-quarantine","--apply"}).apply,"Existing maintenance commands remain available");
    for(const auto& args:std::vector<std::vector<std::string>>{{"--debug=false"},{"--debug","--apply"},{"--redecode-details","--debug"},{"--unknown"}}) {
        bool rejected=false;try{read_startup_options(args);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Invalid flags must fail instead of silently enabling debug");
    }
    TemporaryDatabase database;Store store;store.target("111","douyin",true);
    auto event=gift("debug-event","debug-event",1);event["_details"]={{"payload_base64","CAE="},{"decoded",{{"field","original"}}}};
    deliver(store,"111",event);
    const auto stored=store.message_detail("debug-event");
    const auto normal=message_detail_response(stored,read_startup_options({}).debug);
    check(normal["debug_mode"]==false && normal["details"].is_null() && normal["event"]==stored["event"],"Normal detail responses preserve the business record without raw diagnostics");
    const auto debug=message_detail_response(stored,read_startup_options({"--debug"}).debug);
    check(debug["debug_mode"]==true && debug["details"]==stored["details"],"Debug detail responses include intact diagnostics");
    check(store.message_detail("debug-event")==stored,"Mode selection must not mutate persisted details");
}

void late_connection_status_cannot_override_active_stream() {
    TemporaryDatabase database;Store store;store.target("111","douyin",true);
    auto observed=frame("111",json::object());observed.set_kind("collector_status");observed.set_session_id("connection-session");
    observed.set_session_seq(20);observed.set_received_at_ms(now_ms());
    observed.set_payload(json{{"status","collecting"},{"detail","connected"}}.dump());
    store.commit(observed,observed.SerializeAsString(),json::array(),json::array());
    auto earlier=observed;earlier.set_frame_id("late-connecting");earlier.set_session_seq(10);earlier.set_received_at_ms(observed.received_at_ms()-100);
    earlier.set_payload(json{{"status","connecting"},{"detail","opening"}}.dump());
    store.commit(earlier,earlier.SerializeAsString(),json::array(),json::array());
    auto chat=gift("live-chat","live-chat",0);chat["type"]="chat";
    auto message=frame("111",chat);message.set_session_id("connection-session");message.set_session_seq(21);message.set_received_at_ms(now_ms());
    deliver(store,message,chat);
    check(store.stats("111")["chat"]==1 && store.rooms()[0]["status"]=="collecting","Receiving current messages must not leave the room connecting after a delayed old status");
    auto failed=observed;failed.set_frame_id("new-failure");failed.set_session_seq(30);failed.set_received_at_ms(observed.received_at_ms()+10);
    failed.set_payload(json{{"status","failed"},{"detail","disconnected"}}.dump());store.commit(failed,failed.SerializeAsString(),json::array(),json::array());
    auto lateData=message;lateData.set_frame_id("late-data");lateData.set_session_seq(22);lateData.set_received_at_ms(failed.received_at_ms()+1);
    store.commit(lateData,lateData.SerializeAsString(),json::array(),json::array());
    check(store.rooms()[0]["status"]=="failed","Older data sequence cannot overwrite a newer failure even with a later timestamp");
    Store reopened;
    auto old=earlier;old.set_frame_id("old-after-restart");old.set_session_seq(29);
    reopened.commit(old,old.SerializeAsString(),json::array(),json::array());
    check(reopened.rooms()[0]["status"]=="failed","Status order must survive reopening the database");
    auto next=earlier;next.set_frame_id("next-session");next.set_session_id("next-session");next.set_session_seq(1);next.set_received_at_ms(failed.received_at_ms()+100);
    store.commit(next,next.SerializeAsString(),json::array(),json::array());
    lateData.set_frame_id("previous-session-data");lateData.set_session_seq(31);lateData.set_received_at_ms(failed.received_at_ms()+50);
    store.commit(lateData,lateData.SerializeAsString(),json::array(),json::array());
    check(store.rooms()[0]["status"]=="connecting","Previous session deliveries cannot mark a new connection as collecting");
    auto resumed=next;resumed.set_frame_id("resumed-data");resumed.set_kind("upstream_frame");resumed.set_session_seq(2);
    store.commit(resumed,resumed.SerializeAsString(),json::array(),json::array());
    check(store.rooms()[0]["status"]=="collecting","Fresh data from the new session repairs the connection status");
    store.target("111","douyin",false);
    resumed.set_frame_id("disabled-data");resumed.set_session_seq(3);store.commit(resumed,resumed.SerializeAsString(),json::array(),json::array());
    check(store.rooms()[0]["status"]=="stopped","Data arriving after stop cannot restart a room");
    store.target("111","douyin",true);
    resumed.set_frame_id("obsolete-version-data");resumed.set_session_seq(4);store.commit(resumed,resumed.SerializeAsString(),json::array(),json::array());
    check(store.rooms()[0]["status"]=="connecting","Old target versions cannot update a restarted room");
    auto stale=resumed;stale.set_frame_id("historical-data");stale.set_desired_version(3);stale.set_received_at_ms(now_ms()-60000);
    store.commit(stale,stale.SerializeAsString(),json::array(),json::array());
    check(store.rooms()[0]["status"]=="connecting","Persisting historical buffered data is not proof of a live connection");
    stale.set_frame_id("explicit-replay-data");stale.set_received_at_ms(now_ms());store.commit(stale,stale.SerializeAsString(),json::array(),json::array(),true);
    check(store.rooms()[0]["status"]=="connecting","Explicit replay cannot affect live connection state");
    store.target("222","douyin",true);
    auto untracked=resumed;untracked.set_frame_id("untracked-live");untracked.set_live_id("222");untracked.set_session_id("untracked");untracked.set_received_at_ms(now_ms());
    store.commit(untracked,untracked.SerializeAsString(),json::array(),json::array());
    const auto rooms=store.rooms();check(rooms[0]["status"]=="connecting" && rooms[1]["status"]=="collecting","Fresh data repairs an existing stuck room without affecting other rooms");
    auto deadStatus=untracked;deadStatus.set_frame_id("dead-status");deadStatus.set_session_seq(untracked.session_seq()+1);deadStatus.set_kind("collector_status");
    deadStatus.set_payload(json{{"status","connecting"}}.dump());store.commit(deadStatus,deadStatus.SerializeAsString(),json::array(),json::array({"broker delivery limit reached"}));
    check(store.rooms()[1]["status"]=="collecting","Dead-letter status is archived without changing connection state");
}

int main() {
    try {
        late_connection_status_cannot_override_active_stream();
        message_history_search();
        full_monitor_replay();
        room_state_snapshot();
        saved_detail_upgrade();
        presentation_delivery_and_paging();
        debug_startup_and_details();
        gift_progress_persists_once_per_group();
        legacy_database_migrates_once();
        sparse_progress_matches_only_an_unambiguous_recipient();
        std::cout << "gift_store tests passed: aggregation, idempotency, replay, restart and room isolation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "gift_store test failed: " << error.what() << '\n';
        return 1;
    }
}
