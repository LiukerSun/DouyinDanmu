#include <google/protobuf/util/json_util.h>
#include <google/protobuf/unknown_field_set.h>
#include <memory>
#include <map>
#include <vector>

// All received methods have a persisted event. A schema is a decoding aid, never
// a claim that every field or action has a verified business interpretation.
static std::string payload_base64(const std::string& input) {
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve((input.size()+2)/3*4);
    for(size_t i=0;i<input.size();i+=3) {
        uint32_t n=uint32_t(uint8_t(input[i]))<<16;
        if(i+1<input.size())n|=uint32_t(uint8_t(input[i+1]))<<8;
        if(i+2<input.size())n|=uint8_t(input[i+2]);
        out+=alphabet[(n>>18)&63];out+=alphabet[(n>>12)&63];
        out+=i+1<input.size()?alphabet[(n>>6)&63]:'=';out+=i+2<input.size()?alphabet[n&63]:'=';
    }
    return out;
}
#include "field_analysis.inc"

static json wire_preview(const std::string& payload) {
    google::protobuf::UnknownFieldSet fields;
    if(!fields.ParseFromString(payload))return {{"valid",false},{"fields",json::array()}};
    json list=json::array();
    for(int i=0;i<std::min(fields.field_count(),512);++i) {
        const auto& f=fields.field(i);json item={{"number",f.number()}};
        using F=google::protobuf::UnknownField;
        switch(f.type()) {
            case F::TYPE_VARINT:item["wire_type"]=0;item["value"]=std::to_string(f.varint());break;
            case F::TYPE_FIXED64:item["wire_type"]=1;item["value"]=std::to_string(f.fixed64());break;
            case F::TYPE_LENGTH_DELIMITED:item["wire_type"]=2;item["bytes"]=f.length_delimited().size();item["base64_preview"]=payload_base64(f.length_delimited().substr(0,256));item["truncated"]=f.length_delimited().size()>256;break;
            case F::TYPE_GROUP:item["wire_type"]=3;item["group_fields"]=f.group().field_count();break;
            case F::TYPE_FIXED32:item["wire_type"]=5;item["value"]=std::to_string(f.fixed32());break;
        }
        list.push_back(item);
    }
    return {{"valid",true},{"fields",list},{"truncated",fields.field_count()>512}};
}
static bool incomplete_schema(const google::protobuf::Message& message,int depth=0) {
    if(depth>=64)return true;
    const auto* r=message.GetReflection();
    if(r->GetUnknownFields(message).field_count())return true;
    std::vector<const google::protobuf::FieldDescriptor*> fields;r->ListFields(message,&fields);
    for(const auto* field:fields) {
        if(field->type()==google::protobuf::FieldDescriptor::TYPE_BYTES)return true;
        if(field->cpp_type()!=google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE)continue;
        if(field->is_repeated()) {for(int i=0;i<r->FieldSize(message,field);++i)if(incomplete_schema(r->GetRepeatedMessage(message,field,i),depth+1))return true;}
        else if(incomplete_schema(r->GetMessage(message,field),depth+1))return true;
    }
    return false;
}
static std::pair<std::string,std::string> monitor_category(const std::string& name) {
    static const std::map<std::string,std::pair<std::string,std::string>> categories={
        {"GiftSortMessage",{"gift_notice","礼物排序配置更新"}}, {"ResidentGuestMessage",{"protocol","常驻访客配置更新"}},
        {"LowPcuGuideMessage",{"protocol","低在线引导配置"}}, {"LowPcuGuideChatMessage",{"protocol","低在线聊天引导配置"}},
        {"CommonDotMessage",{"protocol","工具栏提示更新"}}, {"ProfitGameStatusMessage",{"game","游戏状态更新"}},
        {"ProfileViewMessage",{"notice","资料页提示"}}, {"RoomCommentTopicMessage",{"room_notice","直播间话题更新"}},
        {"EmojiChatMessage",{"emoji","表情消息"}}, {"FansclubMessage",{"fansclub","粉丝团事件"}},
        {"RoomStatsMessage",{"room_stats","房间统计"}}, {"RoomMessage",{"room_notice","房间通知"}},
        {"RoomRankMessage",{"room_rank","房间排行榜"}}, {"RanklistHourEntranceMessage",{"hour_rank","小时榜入口"}},
        {"InRoomBannerMessage",{"banner","房间横幅"}}, {"RoomStreamAdaptationMessage",{"stream","直播流适配"}},
        {"RoomDataSyncMessage",{"room_sync","房间数据同步"}}, {"CommonTextMessage",{"notice","公共文本"}},
        {"UpdateFanTicketMessage",{"room_stats","粉丝票更新"}}, {"EpisodeChatMessage",{"episode_chat","节目聊天"}},
        {"MatchAgainstScoreMessage",{"game","比赛比分"}}, {"LiveShoppingMessage",{"commerce","直播电商"}},
        {"ProductChangeMessage",{"commerce","商品变更"}}, {"LiveEcomGeneralMessage",{"commerce","电商事件"}},
        {"LuckyBoxMessage",{"lucky_box","福袋事件"}}, {"LuckyBoxTempStatusMessage",{"lucky_box","福袋状态"}},
        {"DecorationModifyMessage",{"decoration","装饰变更"}}, {"DecorationUpdateMessage",{"decoration","装饰更新"}},
        {"LinkMicAudienceKtvMessage",{"linkmic","连麦 KTV"}}, {"QuizAudienceStatusMessage",{"game","竞猜状态"}},
        {"HotChatMessage",{"notice","热门聊天"}}, {"HotRoomMessage",{"room_stats","直播间热度"}},
        {"AudioChatMessage",{"audio_chat","语音聊天"}}, {"NotifyMessage",{"notice","通知"}},
        {"ScreenChatMessage",{"screen_chat","屏幕聊天"}}, {"NotifyEffectMessage",{"effect","通知特效"}},
        {"BindingGiftMessage",{"gift_notice","礼物关联事件"}}, {"TempStateAreaReachMessage",{"task","区域状态事件"}},
        {"GrowthTaskMessage",{"task","成长任务"}}, {"GameCPBaseMessage",{"game","游戏事件"}},
        {"GameCPShowMessage",{"game","游戏展示"}}, {"SandwichBorderMessage",{"decoration","边框事件"}}
    };
    auto found=categories.find(name);return found==categories.end()?std::make_pair(std::string("protocol"),name):found->second;
}
static std::string monitor_text(const std::string& value) {
    size_t end=std::min(value.size(),size_t(512));
    while(end<value.size() && end>0 && (uint8_t(value[end])&0xc0)==0x80)--end;
    return value.substr(0,end)+(end<value.size()?"…":"");
}
static json observed_event(const std::string& key,const std::string& method,const std::string& payload,int64_t timestamp) {
    return {{"event_id",key},{"display_id",key},{"method",method},{"timestamp",timestamp},
        {"type","unknown"},{"content","待识别消息："+method},{"user_id",""},{"user_name","系统"},
        {"user_level",nullptr},{"fans_club",nullptr},{"gift_count",0},{"parse_status","unmapped"},
        {"payload_bytes",payload.size()},{"has_details",true},{"parser_version",3},
        {"_details",{{"method",method},{"payload_base64",payload_base64(payload)},{"wire",wire_preview(payload)}}}};
}
static void decode_observed(json& event,const std::string& payload) {
    const auto method=event.at("method").get<std::string>();
    if(method.rfind("Webcast",0)!=0 || method.size()<14 || method.substr(method.size()-7)!="Message"){event["_details"]["field_analysis"]=field_analysis(nullptr,payload);return;}
    const auto name=method.substr(7);
    const auto* descriptor=google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(name);
    if(!descriptor || name=="Message" || name=="PreMessage"){event["_details"]["field_analysis"]=field_analysis(nullptr,payload);return;}
    const auto* prototype=google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
    if(!prototype)return;
    std::unique_ptr<google::protobuf::Message> decoded(prototype->New());
    if(!decoded->ParseFromString(payload))throw std::runtime_error("invalid protobuf payload");
    std::string output;google::protobuf::util::JsonPrintOptions options;options.preserve_proto_field_names=true;
    const auto status=google::protobuf::util::MessageToJsonString(*decoded,&output,options);
    if(!status.ok())throw std::runtime_error("protobuf JSON conversion failed");
    auto& details=event["_details"];details["schema"]=name;details["decoded"]=json::parse(output);details["field_analysis"]=field_analysis(decoded.get(),payload);
    event["parse_status"]=incomplete_schema(*decoded)?"partial":"decoded";
    auto category=monitor_category(name);event["type"]=category.first;event["content"]=category.second;
    const auto* reflection=decoded->GetReflection();
    for(int i=0;i<descriptor->field_count();++i) {
        const auto* field=descriptor->field(i);
        if(field->is_repeated() || field->cpp_type()!=google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE || !reflection->HasField(*decoded,field))continue;
        // Only an explicitly named sender user is attributed to an event.
        if(field->name()=="user" && field->message_type()==User::descriptor()) {
            auto user=decode<User>(reflection->GetMessage(*decoded,field).SerializeAsString());
            Common common;common.set_create_time(event.at("timestamp").get<int64_t>());
            user_fields(event,user,common);
        }
        if(field->message_type()==Common::descriptor()) {
            const auto common=decode<Common>(reflection->GetMessage(*decoded,field).SerializeAsString());
            const auto time=common.create_time();if(time>0)event["timestamp"]=time<100000000000LL?time*1000:time;
        }
    }
    for(const char* fieldName:{"content","defaultContent","displayLong","roomFanTicketCountText","actionDescription"}) {
        const auto* field=descriptor->FindFieldByName(fieldName);
        if(field && !field->is_repeated() && field->type()==google::protobuf::FieldDescriptor::TYPE_STRING) {
            const auto text=reflection->GetString(*decoded,field);
            if(!text.empty()){event["content"]=monitor_text(text);break;}
        }
    }
    if(name=="RoomStreamAdaptationMessage")event["content"]="直播流适配类型："+std::to_string(decode<RoomStreamAdaptationMessage>(payload).adaptation_type());
    if(name=="ProfileViewMessage") {
        const auto profile=decode<ProfileViewMessage>(payload);std::string content;
        for(const auto& piece:profile.subtitle_text().pieces())content+=piece.string_value();
        if(!content.empty())event["content"]="资料页提示："+monitor_text(content);
    }
    if(name=="RoomCommentTopicMessage") {
        const auto topic=decode<RoomCommentTopicMessage>(payload);std::string content;
        for(const auto& item:topic.topics()){if(!content.empty())content+="；";content+=item.label()+"："+item.content();}
        if(!content.empty())event["content"]=monitor_text(content);
    }
    if(name=="ProfitGameStatusMessage") {
        const auto game=decode<ProfitGameStatusMessage>(payload);event["content"]="游戏状态码："+std::to_string(game.status());
        auto title=game.game_title().size()<=16384 && shallow_json(game.game_title())?json::parse(game.game_title(),nullptr,false):json(nullptr);
        if(title.is_object() && title.contains("title") && title["title"].is_string())event["content"]=monitor_text(title["title"].get<std::string>());
    }
    if(name=="GiftSortMessage" || name=="ResidentGuestMessage" || name=="LowPcuGuideMessage" || name=="LowPcuGuideChatMessage" || name=="CommonDotMessage" || name=="ProfitGameStatusMessage" || name=="ProfileViewMessage" || name=="RoomCommentTopicMessage") {
        details["schema_evidence"]="参考协议与本机样本核对；仅确认字段结构，未确认的动作码及嵌套数据保留原值。";
        event["parse_status"]="partial";
    }
    if(name=="FansclubMessage")event["action"]=decode<FansclubMessage>(payload).type();
}
