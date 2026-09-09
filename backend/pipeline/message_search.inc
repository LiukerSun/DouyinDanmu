struct MessageSearch {
    std::string room, type, keyword, user, method, parse_status;
    int64_t wealth_min=-1, wealth_max=-1, fans_min=-1, fans_max=-1, before=0;
    int64_t from_ms=-1, to_ms=-1, as_of_seq=-1;
    bool wealth_unknown=false, fans_unknown=false, deliveries=false;
    int limit=50;
    void validate() const {
        auto digits=[](const std::string& s){return s.empty() || s.find_first_not_of("0123456789")==std::string::npos;};
        if(!digits(room) || room.size()>32 || !digits(user) || user.size()>32 || keyword.size()>512 || before<0 || limit<1 || limit>100)
            throw std::invalid_argument("无效的查询条件");
        if(type.size()>64 || type.find_first_not_of("abcdefghijklmnopqrstuvwxyz_")!=std::string::npos || method.size()>256 || parse_status.size()>32)
            throw std::invalid_argument("无效的消息类型");
        for(auto level:{wealth_min,wealth_max,fans_min,fans_max}) if(level < -1 || level>10000) throw std::invalid_argument("无效的等级范围");
        if((wealth_min>=0 && wealth_max>=0 && wealth_min>wealth_max) || (fans_min>=0 && fans_max>=0 && fans_min>fans_max)) throw std::invalid_argument("最低等级不能高于最高等级");
        if(from_ms < -1 || to_ms < -1 || as_of_seq < -1 || (from_ms>=0 && to_ms>=0 && from_ms>=to_ms))
            throw std::invalid_argument("无效的时间范围或统计快照");
    }
};
static MessageSearch read_message_search(const httplib::Request& req) {
    auto number=[&](const char* key,int64_t fallback) {
        if(!req.has_param(key))return fallback;
        auto value=req.get_param_value(key);
        if(value.empty() || value.size()>19 || value.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("无效的数字查询参数");
        try { return int64_t(std::stoll(value)); } catch(...) { throw std::invalid_argument("查询参数超出范围"); }
    };
    MessageSearch q; q.room=req.get_param_value("room");q.type=req.get_param_value("type");q.keyword=req.get_param_value("q");q.user=req.get_param_value("user_id");
    q.deliveries=req.get_param_value("view")=="events";
    q.method=req.get_param_value("method");q.parse_status=req.get_param_value("parse_status");
    q.wealth_min=number("wealth_min",-1);q.wealth_max=number("wealth_max",-1);q.fans_min=number("fans_min",-1);q.fans_max=number("fans_max",-1);
    q.before=number("before",0);auto limit=number("limit",50); if(limit<1 || limit>100)throw std::invalid_argument("分页大小必须为 1–100");q.limit=int(limit);
    q.from_ms=number("from_ms",-1);q.to_ms=number("to_ms",-1);q.as_of_seq=number("as_of_seq",-1);
    for(auto key:{"wealth_unknown","fans_unknown"})if(req.has_param(key)&&req.get_param_value(key)!="1")throw std::invalid_argument("无效的等级查询参数");
    q.wealth_unknown=req.has_param("wealth_unknown");q.fans_unknown=req.has_param("fans_unknown");q.validate();return q;
}
