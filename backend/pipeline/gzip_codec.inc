// Shared gzip primitives: RFC 1952 streams detected by their 1F 8B magic.
// Used both for upstream frame inflation and for compressed body columns.
#include <limits>

// SQLite's length arguments are signed integers. Apply the same bound before
// encoding and when reading stored bodies so a successful write can be read.
static constexpr size_t sqlite_body_size_limit=size_t(std::numeric_limits<int>::max());
static bool is_compressed_body(const char* p, size_t n) {
    return n>=2 && (unsigned char)p[0]==0x1f && (unsigned char)p[1]==0x8b;
}
static std::string gzip_encode(const std::string& plain) {
    if(plain.size()>sqlite_body_size_limit)throw std::runtime_error("body exceeds SQLite size limit");
    z_stream stream{};
    if(deflateInit2(&stream,6,Z_DEFLATED,16+MAX_WBITS,8,Z_DEFAULT_STRATEGY)!=Z_OK) throw std::runtime_error("gzip init failed");
    struct Cleanup { z_stream* stream; ~Cleanup(){deflateEnd(stream);} } cleanup{&stream};
    stream.next_in=(Bytef*)plain.data(); stream.avail_in=uInt(plain.size());
    std::string out; char chunk[16384]; int rc;
    do {
        stream.next_out=(Bytef*)chunk; stream.avail_out=sizeof(chunk);
        rc=deflate(&stream,Z_FINISH);
        out.append(chunk,sizeof(chunk)-stream.avail_out);
    } while(rc==Z_OK);
    if(rc!=Z_STREAM_END) throw std::runtime_error("gzip encode failed");
    return out;
}
static std::string gzip_decode(const char* p, size_t n, size_t max_output_bytes) {
    if(n>size_t(std::numeric_limits<uInt>::max()))throw std::runtime_error("gzip input exceeds size limit");
    z_stream stream{}; stream.next_in=(Bytef*)p; stream.avail_in=uInt(n);
    if(inflateInit2(&stream,16+MAX_WBITS)!=Z_OK) throw std::runtime_error("gzip init failed");
    struct Cleanup { z_stream* stream; ~Cleanup(){inflateEnd(stream);} } cleanup{&stream};
    std::string result; char chunk[16384]; int rc=Z_OK;
    while(rc==Z_OK) {
        stream.next_out=(Bytef*)chunk; stream.avail_out=sizeof(chunk); rc=inflate(&stream,Z_NO_FLUSH);
        const auto produced=sizeof(chunk)-stream.avail_out;
        if(produced>max_output_bytes-result.size())throw std::runtime_error("decompressed data exceeds size limit");
        result.append(chunk,produced);
    }
    if(rc!=Z_STREAM_END) throw std::runtime_error("invalid gzip body"); return result;
}
