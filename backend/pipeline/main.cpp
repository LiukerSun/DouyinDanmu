// Local container pipeline. Upstream frames are decoded here, never in the UI.
#include "douyin.pb.h"
#include "ingest.pb.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#ifndef PIPELINE_PORTABLE
#include <hiredis/hiredis.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>
#endif
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <zlib.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#ifdef GetMessage
#undef GetMessage
#endif

using json = nlohmann::json;
namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;
static std::atomic<bool> running{true}, mq_ready{false};
static int64_t now_ms() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
static std::string env(const char* key, const char* fallback) { const char* v = std::getenv(key); return v ? v : fallback; }
#include "sha256.inc"

#ifndef PIPELINE_PORTABLE
static std::string bytes(amqp_bytes_t b) { return std::string(static_cast<char*>(b.bytes), b.len); }
#endif

class Statement {
    sqlite3_stmt* stmt = nullptr;
public:
    Statement(sqlite3* db, const char* sql) { if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db)); }
    ~Statement() { sqlite3_finalize(stmt); }
    Statement& text(int i, const std::string& s) { sqlite3_bind_text(stmt, i, s.data(), s.size(), SQLITE_TRANSIENT); return *this; }
    Statement& blob(int i, const std::string& s) { sqlite3_bind_blob(stmt, i, s.data(), s.size(), SQLITE_TRANSIENT); return *this; }
    Statement& num(int i, int64_t n) { sqlite3_bind_int64(stmt, i, n); return *this; }
    bool row() { int r = sqlite3_step(stmt); if (r != SQLITE_ROW && r != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(stmt))); return r == SQLITE_ROW; }
    int64_t number(int i) { return sqlite3_column_int64(stmt, i); }
    std::string str(int i) { auto p = sqlite3_column_text(stmt, i); return p ? reinterpret_cast<const char*>(p) : ""; }
    std::string binary(int i) { auto p=sqlite3_column_blob(stmt,i);return p?std::string(static_cast<const char*>(p),sqlite3_column_bytes(stmt,i)):""; }
};

#include "message_search.inc"
#include "message_presentation.inc"
#include "startup_options.inc"

class Store {
    sqlite3* db = nullptr;
    std::mutex mutex;
    void exec(const char* sql) { char* err = nullptr; if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) { std::string message = err ? err : "database failure"; sqlite3_free(err); throw std::runtime_error(message); } }
    json stats_unlocked(const std::string& live) {
        Statement q(db, "SELECT chat,gift,enter_count,likes,online,total,COALESCE((SELECT quantity FROM gift_totals WHERE live_id=rooms.live_id),0) FROM rooms WHERE live_id=?"); q.text(1, live);
        if (!q.row()) return json::object();
        return {{"chat",q.number(0)},{"gift",q.number(1)},{"gift_quantity",q.number(6)},{"enter",q.number(2)},{"like",q.number(3)},{"online",q.number(4)},{"total",q.number(5)}};
    }
    json events_unlocked(const std::string& live, int64_t after, int limit) {
        Statement q(db, "SELECT id,body FROM events WHERE live_id=? AND id>? ORDER BY id LIMIT ?"); q.text(1,live).num(2,after).num(3,limit);
        json list = json::array(); while(q.row()) { auto e = json::parse(q.str(1)); e["seq"] = std::to_string(q.number(0)); list.push_back(e); } return list;
    }
    #include "connection_state_store.inc"
public:
    Store() {
        auto path = env("DATABASE_PATH", "/data/pipeline.db"); std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) throw std::runtime_error("Cannot open database");
        sqlite3_busy_timeout(db, 3000);
        if(sqlite3_create_function_v2(db,"feed_visible",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,sqlite_feed_visible,nullptr,nullptr,nullptr)!=SQLITE_OK)
            throw std::runtime_error("Cannot register presentation policy");
        exec("PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA foreign_keys=ON;");
        exec(R"SQL(
          CREATE TABLE IF NOT EXISTS rooms(live_id TEXT PRIMARY KEY,source TEXT NOT NULL,title TEXT NOT NULL,enabled INTEGER NOT NULL DEFAULT 1,version INTEGER NOT NULL DEFAULT 1,status TEXT NOT NULL DEFAULT 'connecting',detail TEXT NOT NULL DEFAULT '',chat INTEGER DEFAULT 0,gift INTEGER DEFAULT 0,enter_count INTEGER DEFAULT 0,likes INTEGER DEFAULT 0,online INTEGER DEFAULT 0,total INTEGER DEFAULT 0,updated_at INTEGER DEFAULT 0);
          CREATE TABLE IF NOT EXISTS receipts(frame_id TEXT PRIMARY KEY,hash TEXT NOT NULL,created_at INTEGER NOT NULL);
          CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY AUTOINCREMENT,event_id TEXT UNIQUE NOT NULL,live_id TEXT NOT NULL,body TEXT NOT NULL);
          CREATE INDEX IF NOT EXISTS events_room_seq ON events(live_id,id);
          CREATE TABLE IF NOT EXISTS quarantine(id TEXT PRIMARY KEY,frame_id TEXT,reason TEXT NOT NULL,raw BLOB NOT NULL,created_at INTEGER NOT NULL);
          CREATE TABLE IF NOT EXISTS collector_state(id INTEGER PRIMARY KEY CHECK(id=1),body TEXT NOT NULL,seen_at INTEGER NOT NULL);
          CREATE TABLE IF NOT EXISTS room_metadata(live_id TEXT PRIMARY KEY,body TEXT NOT NULL,checked_at INTEGER NOT NULL);
          CREATE TABLE IF NOT EXISTS message_views(live_id TEXT NOT NULL,display_id TEXT NOT NULL,last_seq INTEGER NOT NULL,body TEXT NOT NULL,PRIMARY KEY(live_id,display_id));
          CREATE INDEX IF NOT EXISTS message_views_room_seq ON message_views(live_id,last_seq);
          CREATE INDEX IF NOT EXISTS message_views_gift_group ON message_views(live_id,json_extract(body,'$.gift_group_key'));
          CREATE TABLE IF NOT EXISTS gift_totals(live_id TEXT PRIMARY KEY,quantity INTEGER NOT NULL DEFAULT 0);
          CREATE TABLE IF NOT EXISTS schema_migrations(name TEXT PRIMARY KEY);
          CREATE TABLE IF NOT EXISTS event_details(event_id TEXT PRIMARY KEY,body TEXT NOT NULL);
          CREATE TABLE IF NOT EXISTS room_connection_state(live_id TEXT PRIMARY KEY,desired_version INTEGER NOT NULL,session_id TEXT NOT NULL,session_seq TEXT NOT NULL,observed_at INTEGER NOT NULL);
          CREATE TABLE IF NOT EXISTS quarantine_resolutions(id TEXT PRIMARY KEY,resolved_at INTEGER NOT NULL);
        )SQL");
        // Keep the append-only delivery journal; snapshots read one current row
        // per streak. Migrate existing records once without replaying business effects.
        exec("BEGIN IMMEDIATE");
        try {
            Statement migrated(db,"SELECT name FROM schema_migrations WHERE name='message_views_v1'");
            if(!migrated.row()) {
                exec(R"SQL(
                  INSERT INTO message_views(live_id,display_id,last_seq,body)
                    SELECT live_id,event_id,id,body FROM events;
                  INSERT INTO gift_totals(live_id,quantity)
                    SELECT live_id,SUM(COALESCE(json_extract(body,'$.gift_count'),1))
                    FROM message_views WHERE json_extract(body,'$.type')='gift' GROUP BY live_id;
                  INSERT INTO schema_migrations VALUES('message_views_v1');
                )SQL");
            }
            exec("COMMIT");
        } catch(...) { exec("ROLLBACK"); throw; }
    }
    ~Store() { sqlite3_close(db); }
    json rooms() {
        std::lock_guard<std::mutex> guard(mutex); json result=json::array();
        Statement q(db,"SELECT r.live_id,r.source,r.title,r.enabled,r.version,r.status,r.detail,r.updated_at,m.body,m.checked_at FROM rooms r LEFT JOIN room_metadata m ON m.live_id=r.live_id ORDER BY r.source,r.title");
        while(q.row()) {
            auto metadata=q.str(8).empty()?json(nullptr):json::parse(q.str(8));
            result.push_back({{"live_id",q.str(0)},{"source",q.str(1)},{"title",q.str(2)},{"enabled",q.number(3)!=0},{"version",q.number(4)},{"status",q.str(5)},{"detail",q.str(6)},{"updated_at",q.number(7)},{"stats",stats_unlocked(q.str(0))},{"metadata",metadata},{"metadata_stale",q.number(9)==0 || now_ms()-q.number(9)>180000}});
        }
        return result;
    }
    void target(const std::string& live, const std::string& source, bool enabled) {
        std::lock_guard<std::mutex> guard(mutex);
        Statement q(db,"INSERT INTO rooms(live_id,source,title,enabled,status) VALUES(?,?,?,?,?) ON CONFLICT(live_id) DO UPDATE SET enabled=excluded.enabled,version=rooms.version+1,status=excluded.status,detail=''");
        q.text(1,live).text(2,source).text(3,source=="demo" ? "演示直播间 · 消息管线" : "抖音直播间 " + live).num(4,enabled).text(5,enabled?"connecting":"stopped").row();
    }
    json snapshot(const std::string& live) {
        std::lock_guard<std::mutex> guard(mutex);
        Statement cursor(db,"SELECT COALESCE(MAX(id),0) FROM events WHERE live_id=?");cursor.text(1,live);cursor.row();
        const auto high=cursor.number(0);
        Statement q(db,"SELECT last_seq,body FROM message_views WHERE live_id=? AND feed_visible(body) ORDER BY last_seq DESC LIMIT 100");q.text(1,live);
        json list=json::array(),latest=json::array();
        while(q.row()) {auto e=json::parse(q.str(1));e["seq"]=std::to_string(q.number(0));list.push_back(e);}
        Statement state(db,"SELECT last_seq,body FROM message_views WHERE live_id=? AND json_extract(body,'$.type')='online_count' AND json_extract(body,'$.online_count') IS NOT NULL ORDER BY last_seq DESC LIMIT 1");state.text(1,live);
        if(state.row()){auto e=json::parse(state.str(1));e["seq"]=std::to_string(state.number(0));latest.push_back(projected_room_state(e));}
        std::reverse(list.begin(),list.end());return {{"events",list},{"state_events",latest},{"through_seq",std::to_string(high)},{"stats",stats_unlocked(live)}};
    }
    #include "message_search_store.inc"
    json events(const std::string& live, int64_t after, int limit=200) { std::lock_guard<std::mutex> guard(mutex); return events_unlocked(live,after,limit); }
    json frontend_batch(const std::string& live,int64_t after,int limit=200) {
        std::lock_guard<std::mutex> guard(mutex);
        const auto journal=events_unlocked(live,after,limit);
        json visible=json::array(),states=json::array(),latest=nullptr;
        for(const auto& event:journal) {
            if(feed_visible(event))visible.push_back(event);
            auto state=projected_room_state(event);if(!state.is_null())latest=std::move(state);
        }
        if(!latest.is_null())states.push_back(std::move(latest));
        // Advance over hidden-only batches too, including on reconnect. Using
        // visible.back() here would replay configuration forever or skip actions.
        return {{"events",visible},{"state_events",states},{"through_seq",journal.empty()?std::to_string(after):journal.back().at("seq").get<std::string>()}};
    }
    json stats(const std::string& live) { std::lock_guard<std::mutex> guard(mutex); return stats_unlocked(live); }
    json receipts(const json& ids) {
        std::lock_guard<std::mutex> guard(mutex); json found=json::array();
        for(const auto& id:ids) { Statement q(db,"SELECT frame_id FROM receipts WHERE frame_id=?"); q.text(1,id.get<std::string>()); if(q.row()) found.push_back(q.str(0)); } return found;
    }
    void heartbeat(const json& body) {
        std::lock_guard<std::mutex> guard(mutex); Statement q(db,"INSERT INTO collector_state VALUES(1,?,?) ON CONFLICT(id) DO UPDATE SET body=excluded.body,seen_at=excluded.seen_at"); q.text(1,body.dump()).num(2,now_ms()).row();
    }
    json overview() {
        std::lock_guard<std::mutex> guard(mutex); json o;
        Statement q(db,"SELECT (SELECT COUNT(*) FROM receipts),(SELECT COUNT(*) FROM events),(SELECT COUNT(*) FROM quarantine q WHERE NOT EXISTS(SELECT 1 FROM quarantine_resolutions r WHERE r.id=q.id))"); q.row();
        o={{"frames",q.number(0)},{"events",q.number(1)},{"quarantine",q.number(2)},{"rabbitmq",mq_ready.load()},{"database",true},{"pipeline","RabbitMQ → C++ Protobuf → SQLite → C++ WebSocket"}};
        Statement c(db,"SELECT body,seen_at FROM collector_state WHERE id=1");
        if(c.row()) { o["collector"]=json::parse(c.str(0)); o["collector"]["online"]=now_ms()-c.number(1)<15000; } else o["collector"]={{"online",false}};
        return o;
    }
    json message_detail(const std::string& event_id) {
        std::lock_guard<std::mutex> guard(mutex);
        Statement q(db,"SELECT e.body,d.body FROM events e LEFT JOIN event_details d ON d.event_id=e.event_id WHERE e.event_id=?");q.text(1,event_id);
        if(!q.row())return nullptr;
        return {{"event",json::parse(q.str(0))},{"details",q.str(1).empty()?json(nullptr):json::parse(q.str(1))}};
    }
    json message_methods(const std::string& room) {
        std::lock_guard<std::mutex> guard(mutex);
        Statement q(db,R"SQL(SELECT COALESCE(json_extract(v.body,'$.method'),json_extract(v.body,'$.type')),COALESCE(json_extract(v.body,'$.parse_status'),'legacy'),COUNT(*)
            FROM events v JOIN rooms r ON r.live_id=v.live_id WHERE r.source='douyin' AND feed_visible(v.body) AND (?='' OR v.live_id=?) GROUP BY 1,2 ORDER BY 3 DESC)SQL");
        q.text(1,room).text(2,room);json list=json::array();
        while(q.row())list.push_back({{"method",q.str(0)},{"parse_status",q.str(1)},{"count",q.number(2)}});
        return list;
    }
    void commit(const pipeline::RawFrameEnvelope& frame, const std::string& raw, const json& parsed, const json& errors, bool replay=false) {
        std::lock_guard<std::mutex> guard(mutex); auto hash=digest(raw);
        Statement receipt(db,"SELECT hash FROM receipts WHERE frame_id=?"); receipt.text(1,frame.frame_id());
        if(receipt.row()) { if(receipt.str(0)!=hash) throw std::runtime_error("frame_id checksum conflict"); if(!replay)return; }
        exec("BEGIN IMMEDIATE");
        try {
            if(frame.kind()=="collector_status" && !replay && errors.empty()) {
                auto status=json::parse(frame.payload());
                observe_connection_unlocked(frame,status.value("status","unknown"),status.value("detail",""));
            }
            // Fresh upstream data proves this connection is receiving. Recover
            // already-stuck status without reconnecting or counting replay as live.
            const auto age=now_ms()-frame.received_at_ms();
            if(frame.kind()=="upstream_frame" && !replay && errors.empty() && age>=-5000 && age<30000) {
                observe_connection_unlocked(frame,"collecting","已连接，正在接收直播数据");
            }
            if(frame.kind()=="room_metadata") {
                const auto metadata=json::parse(frame.payload());
                // A late delivery from a previous target version cannot overwrite current state.
                Statement target(db,"SELECT version,enabled FROM rooms WHERE live_id=?"); target.text(1,frame.live_id());
                if(target.row() && target.number(1)!=0 && target.number(0)==int64_t(frame.desired_version())) {
                    Statement q(db,"INSERT INTO room_metadata(live_id,body,checked_at) VALUES(?,?,?) ON CONFLICT(live_id) DO UPDATE SET body=excluded.body,checked_at=excluded.checked_at WHERE excluded.checked_at>room_metadata.checked_at");
                    q.text(1,frame.live_id()).text(2,metadata.dump()).num(3,metadata.at("checked_at_ms").get<int64_t>()).row();
                }
            }
            for(auto e:parsed) {
                e["live_id"]=frame.live_id(); e["room_id"]=frame.room_id(); e["source"]=frame.source(); e["received_at_ms"]=frame.received_at_ms();
                e["persisted_at_ms"]=now_ms();
                const std::string type=e.at("type"), event_id=e.at("event_id");
                if(e.contains("_details")) {
                    Statement detail(db,"INSERT OR IGNORE INTO event_details VALUES(?,?)");detail.text(1,event_id).text(2,e["_details"].dump()).row();e.erase("_details");
                }
                Statement seen(db,"SELECT id FROM events WHERE event_id=?"); seen.text(1,event_id);
                if(seen.row()) {
                    if(replay && e.value("has_details",false)) {
                        // Hydrate metadata without replaying counters or overwriting
                        // historical gift merges/user profile repairs.
                        const auto patch=json{{"method",e["method"]},{"parse_status",e["parse_status"]},{"payload_bytes",e["payload_bytes"]},{"has_details",true},{"parser_version",e.value("parser_version",2)}}.dump();
                        Statement old(db,"UPDATE events SET body=json_patch(body,?) WHERE event_id=?");old.text(1,patch).text(2,event_id).row();
                        Statement view(db,"UPDATE message_views SET body=json_patch(body,?) WHERE live_id=? AND json_extract(body,'$.event_id')=?");view.text(1,patch).text(2,frame.live_id()).text(3,event_id).row();
                    }
                    continue;
                }
                auto display_id=e.value("display_id",event_id);
                if(type=="gift" && !e.value("gift_group_key","").empty()) {
                    // Progress/terminal frames can omit the recipient or gift
                    // details. Continue an identified group only when its
                    // recipient is compatible and exactly one candidate exists.
                    const auto recipient=e.value("recipient_id","");
                    Statement candidates(db,R"SQL(SELECT display_id,body FROM message_views
                      WHERE live_id=? AND json_extract(body,'$.gift_group_key')=?
                      AND (json_extract(body,'$.gift_combo')=1 OR ?=1)
                      AND (COALESCE(json_extract(body,'$.recipient_id'),'')='' OR ?='' OR json_extract(body,'$.recipient_id')=?) LIMIT 2)SQL");
                    candidates.text(1,frame.live_id()).text(2,e.at("gift_group_key")).num(3,e.value("gift_combo",false)).text(4,recipient).text(5,recipient);
                    if(candidates.row()) {
                        const auto candidate_id=candidates.str(0); const auto old=json::parse(candidates.str(1));
                        if(!candidates.row()) {
                            display_id=candidate_id;
                            if(recipient.empty()) e["recipient_id"]=old.value("recipient_id","");
                            if(old.value("gift_combo",false) && !e.value("gift_combo",false)) {
                                e["gift_combo"]=true;
                                e["gift_final"]=old.value("gift_final",false)||e.value("repeat_end",uint32_t(0))==1;
                            }
                            if(e.value("gift_name","")=="礼物 "+e.value("gift_id","")) {
                                for(const auto* field:{"gift_name","content"}) if(old.contains(field)) e[field]=old[field];
                            }
                        }
                    }
                }
                e["display_id"]=display_id;
                bool new_display=true; int64_t gift_delta=0;
                if(type=="gift") {
                    const auto observed=e.at("gift_count").get<int64_t>();
                    if(observed<1) throw std::runtime_error("invalid gift count");
                    int64_t previous=0; bool finalized=false;
                    Statement current(db,"SELECT body FROM message_views WHERE live_id=? AND display_id=?"); current.text(1,frame.live_id()).text(2,display_id);
                    if(current.row()) {
                        auto old=json::parse(current.str(0)); new_display=false;
                        previous=old.at("gift_count").get<int64_t>(); finalized=old.value("gift_final",false);
                        // Late progress frames must not roll back richer terminal data.
                        if(observed<previous || (finalized&&!e.value("gift_final",false))) {
                            for(const auto* field:{"user_name","user_level","fans_club","gift_name","content","timestamp"}) {
                                if(old.contains(field)) e[field]=old[field];
                            }
                        }
                    }
                    const auto count=std::max(previous,observed);
                    e["gift_count"]=count; e["gift_final"]=finalized||e.value("gift_final",false);
                    gift_delta=count-previous;
                }
                Statement ins(db,"INSERT OR IGNORE INTO events(event_id,live_id,body) VALUES(?,?,?)"); ins.text(1,e["event_id"]).text(2,frame.live_id()).text(3,e.dump()).row();
                if(sqlite3_changes(db)==0) continue;
                const auto seq=sqlite3_last_insert_rowid(db);
                Statement view(db,"INSERT INTO message_views(live_id,display_id,last_seq,body) VALUES(?,?,?,?) ON CONFLICT(live_id,display_id) DO UPDATE SET last_seq=excluded.last_seq,body=excluded.body");
                view.text(1,frame.live_id()).text(2,display_id).num(3,seq).text(4,e.dump()).row();
                if(gift_delta>0) {
                    Statement quantity(db,"INSERT INTO gift_totals(live_id,quantity) VALUES(?,?) ON CONFLICT(live_id) DO UPDATE SET quantity=gift_totals.quantity+excluded.quantity");
                    quantity.text(1,frame.live_id()).num(2,gift_delta).row();
                }
                Statement stats(db,"UPDATE rooms SET total=total+?,chat=chat+?,gift=gift+?,enter_count=enter_count+?,likes=likes+?,online=CASE WHEN ?>=0 THEN ? ELSE online END,updated_at=? WHERE live_id=?");
                auto online=type=="online_count"&&!replay?e.value("online_count",int64_t(0)):int64_t(-1);
                stats.num(1,new_display).num(2,type=="chat").num(3,type=="gift"&&new_display).num(4,type=="enter").num(5,type=="like"?e.value("like_count",int64_t(1)):0).num(6,online).num(7,online).num(8,now_ms()).text(9,frame.live_id()).row();
            }
            int index=0;
            for(const auto& error:replay?json::array():errors) {
                Statement q(db,"INSERT OR IGNORE INTO quarantine VALUES(?,?,?,?,?)");
                q.text(1,frame.frame_id()+":"+std::to_string(index++)).text(2,frame.frame_id()).text(3,error.get<std::string>()).blob(4,raw).num(5,now_ms()).row();
            }
            if(replay && errors.empty()) {
                Statement resolved(db,"INSERT OR IGNORE INTO quarantine_resolutions SELECT id,? FROM quarantine WHERE frame_id=?");resolved.num(1,now_ms()).text(2,frame.frame_id()).row();
            }
            Statement q(db,"INSERT OR IGNORE INTO receipts VALUES(?,?,?)"); q.text(1,frame.frame_id()).text(2,hash).num(3,now_ms()).row();
            exec("COMMIT");
        } catch(...) { exec("ROLLBACK"); throw; }
    }
};

#include "event_parser.inc"
#include "quarantine_replay.inc"
#include "detail_redecode.inc"

static void ingest_body(Store& store,const std::string& body,bool dead=false) {
pipeline::RawFrameEnvelope frame; json events=json::array(), errors=json::array();
try {
    if(body.size()>8*1024*1024 || !frame.ParseFromString(body)) throw std::runtime_error("invalid or oversized envelope");
    if(frame.schema_version()!=1 || frame.frame_id().empty() || frame.live_id().empty()) throw std::runtime_error("unsupported or incomplete envelope");
    if(digest(frame.payload())!=frame.payload_sha256()) throw std::runtime_error("payload checksum mismatch");
    if(dead) {
        errors.push_back("broker delivery limit reached");
        auto failed=observed_event(frame.frame_id()+":frame-error","FrameDecodeError",frame.payload(),frame.received_at_ms());
        failed["type"]="parse_error";failed["parse_status"]="failed";failed["content"]="消息重试次数超限";failed["_details"]["error"]="broker delivery limit reached";events.push_back(failed);
    }
    else { auto parsed=parse(frame); events=parsed.first; errors=parsed.second; }
} catch(const std::exception& e) {
    errors.push_back(e.what()); frame.set_kind("quarantine");
    if(frame.frame_id().empty()) frame.set_frame_id(digest(body));
    if(!frame.live_id().empty()) {
        auto failed=observed_event(frame.frame_id()+":frame-error","FrameDecodeError",frame.payload(),frame.received_at_ms());
        failed["type"]="parse_error";failed["parse_status"]="failed";failed["content"]="原始帧解析失败";failed["_details"]["error"]=e.what();events.push_back(failed);
    }
}
store.commit(frame,body,events,errors);
}

#ifndef PIPELINE_PORTABLE
class Cache {
    std::mutex mutex; redisContext* connection=nullptr;
    int64_t retry_at=0;
    bool connect() {
        if(connection && !connection->err) return true;
        if(connection) { redisFree(connection); connection=nullptr; }
        if(now_ms()<retry_at) return false;
        timeval timeout{0,100000}; connection=redisConnectWithTimeout(env("REDIS_HOST","redis").c_str(),6379,timeout);
        if(!connection || connection->err) { retry_at=now_ms()+3000; return false; }
        redisSetTimeout(connection,timeout); return true;
    }
public:
    ~Cache() { if(connection) redisFree(connection); }
    bool healthy() { std::lock_guard<std::mutex> g(mutex); if(!connect()) return false; auto reply=(redisReply*)redisCommand(connection,"PING"); bool ok=reply&&reply->type==REDIS_REPLY_STATUS; if(reply)freeReplyObject(reply); return ok; }
    json stats(Store& store,const std::string& live) {
        std::lock_guard<std::mutex> g(mutex); const auto key="dy:cache:stats:"+live;
        if(connect()) {
            auto r=(redisReply*)redisCommand(connection,"GET %b",key.data(),key.size());
            if(r) { if(r->type==REDIS_REPLY_STRING) { auto v=json::parse(std::string(r->str,r->len),nullptr,false); freeReplyObject(r); if(!v.is_discarded())return v; } else freeReplyObject(r); }
        }
        auto result=store.stats(live);
        if(connect()) { auto payload=result.dump(); auto r=(redisReply*)redisCommand(connection,"SET %b %b EX 2",key.data(),key.size(),payload.data(),payload.size()); if(r)freeReplyObject(r); }
        return result;
    }
};

static void rpc(amqp_connection_state_t c) { auto reply=amqp_get_rpc_reply(c); if(reply.reply_type!=AMQP_RESPONSE_NORMAL) throw std::runtime_error("AMQP channel operation failed"); }
static void consume(Store& store,bool dead) {
    while(running) {
        auto conn=amqp_new_connection();
        try {
            auto socket=amqp_tcp_socket_new(conn); timeval connect_timeout{3,0};
            if(amqp_socket_open_noblock(socket,env("RABBITMQ_HOST","rabbitmq").c_str(),5672,&connect_timeout)!=AMQP_STATUS_OK) throw std::runtime_error("AMQP broker unavailable");
            auto login=amqp_login(conn,env("RABBITMQ_VHOST","douyin").c_str(),0,131072,30,AMQP_SASL_METHOD_PLAIN,env("RABBITMQ_USER","douyin").c_str(),env("RABBITMQ_PASSWORD","local-demo-password").c_str());
            if(login.reply_type!=AMQP_RESPONSE_NORMAL) throw std::runtime_error("AMQP login failed");
            amqp_channel_open(conn,1); rpc(conn); amqp_basic_qos(conn,1,0,8,0); rpc(conn);
            amqp_basic_consume(conn,1,amqp_cstring_bytes(dead?"dy.dead.v1.p0":"dy.raw.v1.p0"),amqp_empty_bytes,0,0,0,amqp_empty_table); rpc(conn);
            if(!dead) mq_ready=true;
            while(running) {
                amqp_maybe_release_buffers(conn); amqp_envelope_t delivery; timeval timeout{1,0};
                auto reply=amqp_consume_message(conn,&delivery,&timeout,0);
                if(reply.reply_type==AMQP_RESPONSE_LIBRARY_EXCEPTION && reply.library_error==AMQP_STATUS_TIMEOUT) continue;
                if(reply.reply_type!=AMQP_RESPONSE_NORMAL) throw std::runtime_error("AMQP connection interrupted");
                try {
                    ingest_body(store,bytes(delivery.message.body),dead);
                    if(amqp_basic_ack(conn,1,delivery.delivery_tag,0)!=AMQP_STATUS_OK) throw std::runtime_error("AMQP ack failed");
                } catch(...) { amqp_destroy_envelope(&delivery); throw; }
                amqp_destroy_envelope(&delivery);
            }
        } catch(const std::exception& e) { if(!dead)mq_ready=false; std::cerr << "[consumer] " << e.what() << "; retrying\n"; }
        amqp_destroy_connection(conn);
        for(int i=0;i<20&&running;i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

#else
class Cache {
public:
    bool healthy() { return true; }
    json stats(Store& store,const std::string& live) { return store.stats(live); }
};
#endif

// The committed event journal is also a durable outbox. Each WS session reads
// it by cursor, so no database-to-memory handoff can lose a committed event.
class WsSession: public std::enable_shared_from_this<WsSession> {
    websocket::stream<beast::tcp_stream> ws;
    beast::flat_buffer input; net::steady_timer timer;
    Store& store; std::map<std::string,int64_t> subscriptions;
    std::deque<std::string> outgoing; size_t queued_bytes=0; bool stopped=false;
    void stop() { if(stopped)return; stopped=true; timer.cancel(); beast::error_code ec; beast::get_lowest_layer(ws).socket().close(ec); }
    void enqueue(const json& msg) {
        auto payload=msg.dump(); if(queued_bytes+payload.size()>2*1024*1024){stop();return;}
        bool idle=outgoing.empty(); queued_bytes+=payload.size(); outgoing.push_back(std::move(payload)); if(idle)write();
    }
    void write() {
        if(stopped)return; ws.text(true);
        ws.async_write(net::buffer(outgoing.front()),[self=shared_from_this()](beast::error_code ec,size_t){
            if(ec){self->stop();return;} self->queued_bytes-=self->outgoing.front().size(); self->outgoing.pop_front(); if(!self->outgoing.empty())self->write();
        });
    }
    void read() {
        ws.async_read(input,[self=shared_from_this()](beast::error_code ec,size_t){
            if(ec){self->stop();return;}
            try {
                auto command=json::parse(beast::buffers_to_string(self->input.data()));
                auto room=command.value("live_id",""); auto action=command.value("action","");
                if(action=="subscribe" && !room.empty() && room.size()<80 && self->subscriptions.size()<32) {
                    auto cursor=std::stoll(command.value("after_seq","0")); if(cursor<0)throw std::runtime_error("invalid cursor");
                    self->subscriptions[room]=cursor; self->enqueue({{"type","subscribed"},{"live_id",room}});
                } else if(action=="unsubscribe") self->subscriptions.erase(room);
            } catch(...) { self->enqueue({{"type","error"},{"message","Invalid subscription"}}); }
            self->input.consume(self->input.size()); if(!self->stopped)self->read();
        });
    }
    void poll() {
        timer.expires_after(std::chrono::milliseconds(150));
        timer.async_wait([self=shared_from_this()](beast::error_code ec){
            if(ec||self->stopped)return;
            try { for(auto& [room,cursor]:self->subscriptions) {
                auto batch=self->store.frontend_batch(room,cursor);
                auto next=std::stoll(batch.at("through_seq").get<std::string>());
                if(next==cursor)continue;
                batch["v"]=2;batch["type"]="event_batch";batch["live_id"]=room;
                self->enqueue(batch); cursor=next;
            }} catch(const std::exception&){self->stop();return;}
            if(!self->stopped)self->poll();
        });
    }
public:
    WsSession(tcp::socket socket,Store& s):ws(std::move(socket)),timer(ws.get_executor()),store(s){}
    void start() {
        ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server)); ws.read_message_max(8192);
        ws.async_accept([self=shared_from_this()](beast::error_code ec){if(ec){self->stop();return;} self->read();self->poll();});
    }
};
class WsListener {
    tcp::acceptor acceptor; Store& store;
    void accept() { acceptor.async_accept([this](beast::error_code ec,tcp::socket socket){if(!ec)std::make_shared<WsSession>(std::move(socket),store)->start();if(acceptor.is_open())accept();}); }
public:
    WsListener(net::io_context& io,Store& s):acceptor(io,tcp::endpoint(net::ip::make_address(env("BIND_HOST","0.0.0.0")),std::stoi(env("WS_PORT","8081")))),store(s){accept();}
};

int main(int argc, char** argv) {
    try {
        const auto options=read_startup_options(std::vector<std::string>(argv+1,argv+argc));
        if(!options.command.empty()) {
            if(options.command=="--redecode-details"){const auto report=redecode_details(options.apply);std::cout<<report.dump(2)<<std::endl;return report["failed"].get<int>()?2:0;}
            return replay_quarantine(options.apply);
        }
        Store store; Cache cache; httplib::Server http;
        http.new_task_queue=[] { return new httplib::ThreadPool(4); };
        http.set_payload_max_length(9*1024*1024); http.set_read_timeout(5); http.set_write_timeout(5);
        http.set_exception_handler([](const httplib::Request&,httplib::Response& res,std::exception_ptr){res.status=500;res.set_content("{\"error\":\"Request failed\"}","application/json");});
        auto send=[](httplib::Response& res,const json& data){res.set_content(data.dump(),"application/json");};
        auto internal=[&](const httplib::Request& req,httplib::Response& res){if(req.get_header_value("X-Internal-Token")!=env("INTERNAL_TOKEN","local-collector-token")){res.status=401;return false;}return true;};
        http.Get("/health/live",[&](const httplib::Request&,httplib::Response& res){send(res,{{"status","ok"}});});
        http.Get("/api/health",[&](const httplib::Request&,httplib::Response& res){auto o=store.overview();o["redis"]=cache.healthy();o["debug_mode"]=options.debug;
#ifdef PIPELINE_PORTABLE
            o["transport"]="local";o["cache_backend"]="sqlite";o["pipeline"]="Local spool → C++ Protobuf → SQLite → C++ WebSocket";
#endif
            send(res,o);});
        http.Get("/api/rooms",[&](const httplib::Request&,httplib::Response& res){send(res,store.rooms());});
        http.Get("/api/messages/search",[&](const httplib::Request& req,httplib::Response& res){
            try {send(res,store.search_messages(read_message_search(req)));}
            catch(const std::invalid_argument& e){res.status=400;send(res,{{"error",e.what()}});}
        });
        http.Get("/api/messages/detail",[&](const httplib::Request& req,httplib::Response& res){
            const auto id=req.get_param_value("event_id");if(id.empty()||id.size()>2048){res.status=400;send(res,{{"error","无效的消息 ID"}});return;}
            auto detail=store.message_detail(id);if(detail.is_null()){res.status=404;send(res,{{"error","记录不存在"}});return;}send(res,message_detail_response(std::move(detail),options.debug));
        });
        http.Get("/api/messages/methods",[&](const httplib::Request& req,httplib::Response& res){send(res,store.message_methods(req.get_param_value("room")));});
        http.Post("/api/rooms",[&](const httplib::Request& req,httplib::Response& res){
            auto data=json::parse(req.body,nullptr,false); if(data.is_discarded()){res.status=400;send(res,{{"error","Invalid JSON"}});return;}
            auto live=data.value("live_id",""); auto source=live=="demo"?"demo":"douyin";
            if(live.empty() || live.size()>30 || (live!="demo" && live.find_first_not_of("0123456789")!=std::string::npos)){res.status=400;send(res,{{"error","请输入数字直播间号"}});return;}
            store.target(live,source,true);res.status=202;send(res,{{"live_id",live},{"status","connecting"}});
        });
        http.Delete(R"(/api/rooms/([a-zA-Z0-9]+))",[&](const httplib::Request& req,httplib::Response& res){auto live=req.matches[1].str();store.target(live,live=="demo"?"demo":"douyin",false);send(res,{{"status","stopped"}});});
        http.Get(R"(/api/rooms/([a-zA-Z0-9]+)/snapshot)",[&](const httplib::Request& req,httplib::Response& res){send(res,store.snapshot(req.matches[1].str()));});
        http.Get(R"(/api/rooms/([a-zA-Z0-9]+)/stats)",[&](const httplib::Request& req,httplib::Response& res){send(res,cache.stats(store,req.matches[1].str()));});
        http.Get(R"(/api/rooms/([a-zA-Z0-9]+)/events)",[&](const httplib::Request& req,httplib::Response& res){
            try {auto after=std::stoll(req.has_param("after_seq")?req.get_param_value("after_seq"):"0");if(after<0)throw std::invalid_argument("Invalid cursor");send(res,store.frontend_batch(req.matches[1].str(),after));}catch(...){res.status=400;send(res,{{"error","Invalid cursor"}});}
        });
        http.Get("/internal/targets",[&](const httplib::Request& req,httplib::Response& res){if(internal(req,res))send(res,store.rooms());});
        http.Post("/internal/receipts",[&](const httplib::Request& req,httplib::Response& res){if(!internal(req,res))return;auto ids=json::parse(req.body);if(!ids.is_array()||ids.size()>256){res.status=400;return;}send(res,store.receipts(ids));});
        http.Post("/internal/heartbeat",[&](const httplib::Request& req,httplib::Response& res){if(!internal(req,res))return;store.heartbeat(json::parse(req.body));send(res,{{"ok",true}});});
#ifdef PIPELINE_PORTABLE
        mq_ready=true;
        http.Post("/internal/ingest",[&](const httplib::Request& req,httplib::Response& res){
            if(!internal(req,res))return;
            if(req.body.size()>9*1024*1024){res.status=413;return;}
            ingest_body(store,req.body);send(res,{{"committed",true}});
        });
#endif
        net::io_context io; WsListener listener(io,store); net::signal_set signals(io,SIGINT,SIGTERM);
        signals.async_wait([&](beast::error_code,int){running=false;http.stop();io.stop();});
        if(!http.bind_to_port(env("BIND_HOST","0.0.0.0"),std::stoi(env("HTTP_PORT","8080"))))throw std::runtime_error("HTTP port unavailable");
        std::thread api([&]{http.listen_after_bind();});
#ifndef PIPELINE_PORTABLE
        std::thread worker([&]{consume(store,false);}); std::thread dead([&]{consume(store,true);});
#endif
        std::cout << "C++ pipeline ready: HTTP 8080, WebSocket 8081; debug=" << (options.debug?"on":"off") << std::endl;
        io.run(); running=false;http.stop();api.join();
#ifndef PIPELINE_PORTABLE
        worker.join();dead.join();
#endif
    } catch(const std::exception& e) {std::cerr<<e.what()<<std::endl;return 1;}
    return 0;
}
