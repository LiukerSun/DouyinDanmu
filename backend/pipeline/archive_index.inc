    // Keep the journal and merged views authoritative. Offline repair tools also
    // write them, so persistent triggers invalidate entries using only built-in
    // SQLite operations. -1 means the reader must check the current body.
    void initialize_archive_index() {
        exec("BEGIN IMMEDIATE");
        try {
            Statement migrated(db, "SELECT 1 FROM schema_migrations WHERE name='archive_visibility_v1'");
            if (!migrated.row()) {
                exec("CREATE TABLE message_archive(merged INTEGER NOT NULL,seq INTEGER NOT NULL,"
                     "live_id TEXT NOT NULL,visible INTEGER NOT NULL,PRIMARY KEY(merged,seq)) WITHOUT ROWID");
                for (bool merged : {false, true}) {
                    const std::string table = merged ? "message_views" : "events";
                    const std::string seq = merged ? "last_seq" : "id";
                    const std::string scope = merged ? "1" : "0";
                    exec(("INSERT INTO message_archive SELECT " + scope + "," + seq + ",live_id,feed_visible(body) FROM " + table).c_str());
                    const auto prefix = "CREATE TRIGGER archive_" + table;
                    const auto remove = "DELETE FROM message_archive WHERE merged=" + scope + " AND seq=OLD." + seq + "; ";
                    const auto invalidate = "INSERT OR REPLACE INTO message_archive VALUES(" + scope + ",NEW." + seq + ",NEW.live_id,-1); ";
                    exec((prefix + "_insert AFTER INSERT ON " + table + " BEGIN " + invalidate + "END").c_str());
                    exec((prefix + "_update AFTER UPDATE OF body,live_id," + seq + " ON " + table + " BEGIN " + remove + invalidate + "END").c_str());
                    exec((prefix + "_delete AFTER DELETE ON " + table + " BEGIN " + remove + "END").c_str());
                }
                exec("CREATE INDEX archive_page ON message_archive(merged,seq,live_id,visible) WHERE visible<>0;"
                     "CREATE INDEX archive_room_page ON message_archive(merged,live_id,seq,visible) WHERE visible<>0;"
                     "CREATE INDEX archive_pending ON message_archive(merged,seq) WHERE visible=-1;"
                     "INSERT INTO schema_migrations VALUES('archive_visibility_v1')");
            }
            exec("COMMIT");
        } catch (...) { exec("ROLLBACK"); throw; }
        // Also resolve edits from offline tools before reopening a large archive.
        while (refresh_archive_index(1000)) {}
    }

    bool refresh_archive_index(int limit) {
        std::lock_guard<std::mutex> guard(mutex);
        {
            Statement pending(db,"SELECT 1 FROM message_archive WHERE visible=-1 LIMIT 1");
            if(!pending.row())return false;
        }
        bool more=false;
        for (bool merged : {false, true}) {
            const char* body = merged
                ? "(SELECT body FROM message_views WHERE live_id=a.live_id AND last_seq=a.seq)"
                : "(SELECT body FROM events WHERE id=a.seq)";
            const auto sql = std::string("UPDATE message_archive AS a SET visible=feed_visible(") + body +
                ") WHERE merged=?1 AND seq IN (SELECT seq FROM message_archive WHERE merged=?1 AND visible=-1 LIMIT ?2)";
            Statement update(db, sql.c_str());
            update.num(1, merged).num(2, limit).row();
            if(sqlite3_changes(db)==limit)more=true;
        }
        return more;
    }
