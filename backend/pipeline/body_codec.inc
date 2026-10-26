// Transparent gzip compression for large JSON body columns. Plain JSON stays
// TEXT; compressed bodies are BLOBs whose gzip magic (1F 8B) can never begin
// valid JSON text, so both formats coexist in one column and readers decode
// transparently. Bodies under 64 bytes stay TEXT: the gzip header would cost
// more than it saves. Requires gzip_codec.inc.
static bool compression_enabled() { return env("COMPRESSION","on")!="off"; }
static bool body_stays_text(const std::string& plain) { return !compression_enabled() || plain.size()<64; }
// Type-agnostic decode: gzip-marked input inflates, anything else passes
// through. Typed callers (Statement::decoded, the UDF) reject non-gzip BLOBs
// before reaching here so corruption fails loudly instead of parsing garbage.
static std::string decode_body(const std::string& stored) {
    if(is_compressed_body(stored.data(),stored.size())) return gzip_decode(stored.data(),stored.size(),sqlite_body_size_limit);
    return stored;
}

static void sqlite_decode_body(sqlite3_context* context,int,sqlite3_value** values) noexcept {
    try {
        const auto type=sqlite3_value_type(values[0]);
        if(type==SQLITE_NULL) { sqlite3_result_null(context); return; }
        const auto* p=static_cast<const char*>(sqlite3_value_blob(values[0]));
        const auto n=size_t(sqlite3_value_bytes(values[0]));
        if(type==SQLITE_BLOB) {
            if(!p || !is_compressed_body(p,n)) { sqlite3_result_error(context,"undecodable body",-1); return; }
            const auto plain=gzip_decode(p,n,sqlite_body_size_limit);
            sqlite3_result_text(context,plain.data(),plain.size(),SQLITE_TRANSIENT);
            return;
        }
        sqlite3_result_value(context,values[0]);
    } catch(...) { sqlite3_result_error(context,"undecodable body",-1); }
}
static void sqlite_encode_body(sqlite3_context* context,int,sqlite3_value** values) noexcept {
    try {
        if(sqlite3_value_type(values[0])==SQLITE_NULL) { sqlite3_result_null(context); return; }
        const auto* p=static_cast<const char*>(sqlite3_value_blob(values[0]));
        const auto n=size_t(sqlite3_value_bytes(values[0]));
        // Accept an already-compressed value so json_patch(decode_body(body))
        // results re-encode exactly once regardless of the stored format.
        const std::string plain=(p && is_compressed_body(p,n))?gzip_decode(p,n,sqlite_body_size_limit):std::string(p?p:"",n);
        if(body_stays_text(plain)) { sqlite3_result_text(context,plain.data(),plain.size(),SQLITE_TRANSIENT); return; }
        const auto encoded=gzip_encode(plain);
        sqlite3_result_blob(context,encoded.data(),encoded.size(),SQLITE_TRANSIENT);
    } catch(...) { sqlite3_result_error(context,"cannot encode body",-1); }
}
// Every connection that reads or writes events/event_details/event_presentations
// bodies needs these, including the offline maintenance tools' own connections.
static void register_body_codecs(sqlite3* db) {
    if(sqlite3_create_function_v2(db,"decode_body",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,sqlite_decode_body,nullptr,nullptr,nullptr)!=SQLITE_OK)
        throw std::runtime_error("Cannot register body decoder");
    if(sqlite3_create_function_v2(db,"encode_body",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,sqlite_encode_body,nullptr,nullptr,nullptr)!=SQLITE_OK)
        throw std::runtime_error("Cannot register body encoder");
}
