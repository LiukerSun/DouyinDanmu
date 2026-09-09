#include <boost/multiprecision/cpp_int.hpp>

// SQLite SUM/multiplication may overflow int64 or silently become floating point.
// Monetary totals are exact decimal strings all the way to the API consumer.
static void analytics_sum_step(sqlite3_context* context,int,sqlite3_value** values) {
    auto state=static_cast<boost::multiprecision::cpp_int**>(sqlite3_aggregate_context(context,sizeof(boost::multiprecision::cpp_int*)));
    if(!state){sqlite3_result_error_nomem(context);return;}
    try {
        if(!*state)*state=new boost::multiprecision::cpp_int(0);
        if(sqlite3_value_type(values[0])!=SQLITE_NULL && sqlite3_value_type(values[1])!=SQLITE_NULL)
            **state+=boost::multiprecision::cpp_int(sqlite3_value_int64(values[0]))*sqlite3_value_int64(values[1]);
    } catch(...){sqlite3_result_error(context,"Cannot aggregate statistics",-1);}
}
static void analytics_sum_final(sqlite3_context* context) {
    auto state=static_cast<boost::multiprecision::cpp_int**>(sqlite3_aggregate_context(context,0));
    try {
        const auto value=state&&*state?(*state)->str():"0";
        sqlite3_result_text(context,value.c_str(),int(value.size()),SQLITE_TRANSIENT);
    } catch(...){sqlite3_result_error(context,"Cannot aggregate statistics",-1);}
    if(state){delete *state;*state=nullptr;}
}

struct AnalyticsQuery {
    MessageSearch scope;
    int offset=0;
    std::string sort="value";
    void validate() const {
        scope.validate();
        if(offset<0 || offset>1000000 || (sort!="value" && sort!="quantity"))throw std::invalid_argument("无效的榜单分页或排序");
        // Analytics intentionally has one fixed counting policy, independent of feed filters.
        if(!scope.type.empty() || !scope.user.empty() || !scope.keyword.empty() || !scope.method.empty() || !scope.parse_status.empty() || scope.before || scope.deliveries || scope.wealth_unknown || scope.fans_unknown || scope.wealth_min!=-1 || scope.wealth_max!=-1 || scope.fans_min!=-1 || scope.fans_max!=-1)
            throw std::invalid_argument("统计接口不支持此消息筛选条件");
    }
};
static AnalyticsQuery read_analytics_query(const httplib::Request& req) {
    for(const auto& parameter:req.params)
        if(parameter.first!="room" && parameter.first!="from_ms" && parameter.first!="to_ms" && parameter.first!="as_of_seq" && parameter.first!="limit" && parameter.first!="offset" && parameter.first!="sort")
            throw std::invalid_argument("不支持的统计查询参数");
    AnalyticsQuery q;q.scope=read_message_search(req);
    if(req.has_param("offset")) {
        const auto value=req.get_param_value("offset");
        if(value.empty() || value.size()>7 || value.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("无效的分页偏移");
        q.offset=std::stoi(value);
    }
    if(req.has_param("sort"))q.sort=req.get_param_value("sort");
    q.validate();return q;
}

static int64_t analytics_price(const json& event,const json& legacy_price=nullptr) {
    const auto price=event.contains("gift_unit_price")?event.at("gift_unit_price"):legacy_price;
    if(price.is_number_integer() && price>=0 && price<=uint64_t(UINT32_MAX))return price.get<int64_t>();
    return -1;
}
