    // Plain writes invalidate only the affected cursor, including writes by
    // offline tools. Compression itself writes BLOBs and cannot dirty the pass.
    void compression_initialize() {
        exec("BEGIN IMMEDIATE");
        try {
            bool has_rewritten=false;
            { Statement columns(db,"PRAGMA table_info(compression_state)");while(columns.row())if(columns.str(1)=="rewritten_rows")has_rewritten=true; }
            if(!has_rewritten) {
                exec("ALTER TABLE compression_state ADD COLUMN rewritten_rows INTEGER NOT NULL DEFAULT 0");
                // Recheck databases migrated by the first compression implementation.
                exec("UPDATE compression_state SET last_rowid=0,done=0 WHERE scope<>'vacuum'");
            }
            for(const char* scope:{"event_details","events","event_presentations"}) {
                Statement state(db,"INSERT OR IGNORE INTO compression_state(scope) VALUES(?)");state.text(1,scope).row();
                const auto table=std::string(scope);
                const std::string column=table=="event_presentations"?"fields":"body";
                for(const char* operation:{"INSERT","UPDATE"}) {
                    const auto sql="CREATE TRIGGER IF NOT EXISTS compression_"+table+"_"+operation+" AFTER "+operation+
                        (std::string(operation)=="UPDATE"?" OF "+column:"")+" ON "+table+
                        " WHEN typeof(NEW."+column+")='text' AND length(CAST(NEW."+column+" AS BLOB))>=64 BEGIN "
                        "INSERT INTO compression_state(scope,last_rowid,done,updated_at,rewritten_rows) VALUES('"+table+"',MAX(0,NEW.rowid-1),0,0,0) "
                        "ON CONFLICT(scope) DO UPDATE SET last_rowid=MIN(last_rowid,MAX(0,NEW.rowid-1)),done=0,"
                        "rewritten_rows=CASE WHEN done=1 THEN 0 ELSE rewritten_rows END; END";
                    exec(sql.c_str());
                }
            }
            exec("COMMIT");
        } catch(...) {exec("ROLLBACK");throw;}
        refresh_maintenance_unlocked();
    }

    // Bound rows examined as well as rows rewritten. A database containing
    // mostly BLOBs or small TEXT values must not cause one unbounded scan.
    bool compress_batch(int max_rows) {
        if(!compression_enabled())return false;
        if(max_rows<1)throw std::invalid_argument("Compression batch size must be positive");
        std::lock_guard<std::mutex> guard(mutex);
        std::string table;int64_t last=0;
        {
            Statement state(db,"SELECT scope,last_rowid FROM compression_state WHERE scope IN ('event_details','events','event_presentations') AND done=0 ORDER BY CASE scope WHEN 'event_details' THEN 0 WHEN 'events' THEN 1 ELSE 2 END LIMIT 1");
            if(!state.row())return false;
            table=state.str(0);last=state.number(1);
        }
        const std::string column=table=="event_presentations"?"fields":"body";
        bool complete=false,checkpoint=false;
        exec("BEGIN IMMEDIATE");
        try {
            // An offline writer may have dirtied an earlier row before this
            // transaction acquired its lock. Read the cursor again under it.
            { Statement state(db,"SELECT last_rowid FROM compression_state WHERE scope=?");state.text(1,table);state.row();last=state.number(0); }
            struct Rewrite { int64_t rowid;std::string encoded; };
            std::vector<Rewrite> batch;int scanned=0;
            {
                Statement rows(db,("SELECT rowid,CASE WHEN typeof("+column+")='text' AND length(CAST("+column+" AS BLOB))>=64 THEN "+column+" END FROM "+table+" WHERE rowid>? ORDER BY rowid LIMIT ?").c_str());
                rows.num(1,last).num(2,max_rows);
                while(rows.row()) {
                    last=rows.number(0);++scanned;
                    if(rows.stored_type(1)!=SQLITE_NULL)batch.push_back({last,gzip_encode(rows.decoded(1))});
                }
            }
            for(const auto& rewrite:batch) {
                Statement update(db,("UPDATE "+table+" SET "+column+"=? WHERE rowid=?").c_str());
                update.blob(1,rewrite.encoded).num(2,rewrite.rowid).row();
            }
            {
                Statement mark(db,"UPDATE compression_state SET last_rowid=?,done=?,updated_at=?,rewritten_rows=rewritten_rows+? WHERE scope=?");
                mark.num(1,last).num(2,scanned==0).num(3,now_ms()).num(4,batch.size()).text(5,table).row();
            }
            if(scanned==0) {
                Statement remaining(db,"SELECT SUM(done=0),SUM(rewritten_rows) FROM compression_state WHERE scope IN ('event_details','events','event_presentations')");remaining.row();
                complete=remaining.number(0)==0;checkpoint=complete && remaining.number(1)>0;
            }
            if(checkpoint) {
                Statement pending(db,"INSERT INTO compression_state(scope,done,updated_at) VALUES('vacuum',0,?) ON CONFLICT(scope) DO UPDATE SET done=0,updated_at=excluded.updated_at");pending.num(1,now_ms()).row();
                exec("UPDATE compression_state SET rewritten_rows=0 WHERE scope<>'vacuum'");
            }
            exec("COMMIT");
        } catch(...) {if(!sqlite3_get_autocommit(db))exec("ROLLBACK");throw;}
        refresh_maintenance_unlocked();
        // Only the transition to complete checkpoints; idle calls above are read-only.
        if(checkpoint)exec("PRAGMA wal_checkpoint(TRUNCATE)");
        return !complete;
    }

    // Publish a small snapshot after state changes. Health polling never scans
    // body tables, and the separate status endpoint remains usable during VACUUM.
    void refresh_maintenance_unlocked() {
        json scopes=json::array();bool pending=false;
        Statement state(db,"SELECT scope,done FROM compression_state ORDER BY rowid");
        while(state.row()) {
            const auto scope=state.str(0);const bool done=state.number(1)!=0;
            if(scope=="vacuum")pending=!done;
            else scopes.push_back({{"scope",scope},{"done",done}});
        }
        std::lock_guard<std::mutex> guard(maintenance_mutex);
        maintenance_snapshot={{"reencode",scopes},{"vacuum_pending",pending}};
    }
    json maintenance_status() {
        std::lock_guard<std::mutex> guard(maintenance_mutex);
        auto status=maintenance_snapshot;
        status["compression_enabled"]=compression_enabled();
        status["vacuum_queued"]=vacuum_requested.load();
        status["vacuum_running"]=vacuum_running.load();
        return status;
    }
    bool vacuum_pending() {
        std::lock_guard<std::mutex> guard(mutex);
        Statement state(db,"SELECT done FROM compression_state WHERE scope='vacuum'");
        return state.row() && state.number(0)==0;
    }
    // 0=queued, 1=already queued/running, 2=insufficient free disk space.
    int request_compact() {
        if(vacuum_requested.load() || vacuum_running.load())return 1;
        std::lock_guard<std::mutex> guard(mutex);
        if(vacuum_requested.load() || vacuum_running.load())return 1;
        Statement pages(db,"PRAGMA page_count"),size(db,"PRAGMA page_size");pages.row();size.row();
        std::error_code ec;const auto space=std::filesystem::space(std::filesystem::path(database_path).parent_path(),ec);
        const auto database_bytes=uint64_t(pages.number(0))*uint64_t(size.number(0));
        // VACUUM can need both a temporary database and a rollback journal/WAL.
        // SQLite documents up to twice the original database size in free space.
        if(!ec && space.available/2<database_bytes)return 2;
        vacuum_requested=true;
        return 0;
    }
    void vacuum() {
        vacuum_running=true;
        try {
            std::lock_guard<std::mutex> guard(mutex);
            // VACUUM may renumber rowids of tables without INTEGER PRIMARY KEY.
            // Persist the rewind first so interruption cannot strand old cursors.
            exec("UPDATE compression_state SET last_rowid=0 WHERE done=0 AND scope<>'vacuum'");
            exec("VACUUM");
            exec("PRAGMA wal_checkpoint(TRUNCATE)");
            Statement done(db,"UPDATE compression_state SET done=1,updated_at=? WHERE scope='vacuum'");done.num(1,now_ms()).row();
            refresh_maintenance_unlocked();
        } catch(...) {vacuum_requested=false;vacuum_running=false;throw;}
        vacuum_requested=false;vacuum_running=false;
    }
    void startup_vacuum_if_pending() {
        if(env("VACUUM_ON_STARTUP","on")=="off" || !vacuum_pending())return;
        if(request_compact()==2) {
            std::cerr<<"[maintenance] Database compaction postponed: insufficient free disk space"<<std::endl;
            return;
        }
        std::cerr<<"[maintenance] Compacting database before serving; startup may take several minutes"<<std::endl;
        vacuum();
        std::cerr<<"[maintenance] Database compaction finished"<<std::endl;
    }
