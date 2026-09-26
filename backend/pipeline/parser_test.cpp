// These fixtures encode wire tags directly: the tested schema cannot silently
// make the producer repeat the consumer's field-type mistakes.
#include "douyin.pb.h"
#include "ingest.pb.h"
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;
static int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
#include "sha256.inc"

#ifndef PARSER_BASELINE
#include "event_parser.inc"
#endif

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::string varint(uint64_t value) {
    std::string bytes;
    do {
        auto next = static_cast<unsigned char>(value & 127);
        value >>= 7;
        if (value) next |= 128;
        bytes.push_back(static_cast<char>(next));
    } while (value);
    return bytes;
}
std::string number(unsigned field, uint64_t value) { return varint(uint64_t(field) << 3) + varint(value); }
std::string data(unsigned field, const std::string& bytes) {
    return varint((uint64_t(field) << 3) | 2) + varint(bytes.size()) + bytes;
}
constexpr uint64_t sender_id = 9007199254740993123ULL;
constexpr uint64_t receiver_id = 9007199254740993124ULL;
constexpr uint64_t combo_group = 9007199254740993125ULL;

std::string user_wire(uint64_t id = sender_id,
                      std::optional<int64_t> wealth = std::nullopt,
                      std::optional<int32_t> club_level = std::nullopt,
                      std::optional<int32_t> club_status = std::nullopt,
                      bool empty_children = false) {
    // User.Level=99 is deliberately different from PayGrade.level.
    auto user = number(1, id) + data(3, "测试用户") + number(6, 99);
    if (wealth || empty_children) user += data(23, wealth ? number(6, uint64_t(*wealth)) : "");
    if (club_level || club_status || empty_children) {
        std::string club;
        if (club_level) club += number(2, uint64_t(*club_level));
        if (club_status) club += number(3, uint64_t(*club_status));
        if (club_level || club_status) club += data(1, "测试粉丝团");
        user += data(24, data(1, club));
    }
    return user;
}
struct GiftOptions {
    uint64_t combo_count = 2, repeat_count = 3, group_count = 9;
    uint64_t group = combo_group, sender = sender_id, receiver = receiver_id, gift_id = 112233;
    uint32_t repeat_end = 0;
    bool combo = true, packed = true, name = true, unknown_extensions = false;
};
std::string gift_wire(const GiftOptions& options = {}) {
    auto payload = number(2, options.gift_id) + number(3, 128) + number(4, options.group_count)
        + number(5, options.repeat_count) + number(6, options.combo_count)
        + data(7, user_wire(options.sender, 32, 7, 1))
        + data(8, user_wire(options.receiver)) + number(9, options.repeat_end)
        + number(11, options.group) + number(12, 12) + number(13, 13)
        + number(17, 17) + number(20, 20);
    auto gift = number(5, options.gift_id) + number(10, options.combo) + number(12, 1);
    if (options.name) gift += data(16, "粉丝灯牌");
    payload += data(15, gift);
    if (options.packed) {
        payload += data(28, varint(128) + varint(129));
        // 128 is 0x80 0x01: valid packed varint, invalid UTF-8 string.
        payload += data(32, varint(128) + varint(options.receiver));
    } else {
        payload += number(28, 128) + number(28, 129);
        payload += number(32, 128) + number(32, options.receiver);
    }
    payload += number(29, 29) + number(30, 30) + number(33, 33) + number(34, 34) + number(36, 36);
    if (options.unknown_extensions)
        for (unsigned field : {24, 31, 37, 38, 40, 41, 42, 99}) payload += data(field, std::string(1, char(0xff)));
    return payload;
}

void packed_schema_regression() {
    GiftMessage gift;
    check(gift.ParseFromString(gift_wire()),
          "GiftMessage #32 packed uint64 value 128 must parse; it is not UTF-8 text");
    // Check field 32's wire type through the protobuf descriptor.
    const auto* field = gift.GetDescriptor()->FindFieldByNumber(32);
    check(field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_UINT64,
          "GiftMessage #32 must be repeated uint64");
}

#ifndef PARSER_BASELINE
pipeline::RawFrameEnvelope envelope(const std::string& method, const std::string& payload,
                                    uint64_t message_id = 101, const std::string& room = "test-room") {
    // The outer protocol is also built from independent wire tags.
    const auto message = data(1, method) + data(2, payload) + number(3, message_id);
    const auto response = data(1, message);
    // Gift and user fixtures stay independent; reuse only the outer transport.
    PushFrame outer;
    outer.set_payload(response);
    pipeline::RawFrameEnvelope result;
    result.set_frame_id("offline-parser-" + std::to_string(message_id));
    result.set_room_id(room);
    result.set_live_id("test-live");
    result.set_kind("upstream_frame");
    result.set_payload(outer.SerializeAsString());
    result.set_received_at_ms(now_ms());
    return result;
}
json one(const std::string& method, const std::string& payload,
         uint64_t message_id = 101, const std::string& room = "test-room") {
    const auto result = parse(envelope(method, payload, message_id, room));
    check(result.second.empty(), "Unexpected parser error: " + result.second.dump());
    check(result.first.size() == 1, "Exactly one event must be produced");
    return result.first.front();
}

void gift_fields_and_identity() {
    check(one("WebcastGiftMessage",data(15,number(12,0)))["gift_unit_price"]==0,"Explicit zero gift price remains distinguishable from omission");
    check(one("WebcastGiftMessage",data(15,""))["gift_unit_price"].is_null(),"Omitted gift price must not become zero");
    for (bool packed : {true, false}) {
        GiftOptions options; options.packed = packed;
        const auto payload = gift_wire(options);
        const auto gift = decode<GiftMessage>(payload);
        check(gift.to_user_ids_size() == 2 && gift.to_user_ids(0) == 128 && gift.to_user_ids(1) == receiver_id,
              "Packed and unpacked recipient IDs must preserve uint64 values");
        check(gift.min_asset_set_size() == 2 && gift.min_asset_set(0) == 128 && gift.min_asset_set(1) == 129,
              "Packed and unpacked asset IDs must be accepted");
        const auto event = one("WebcastGiftMessage", payload);
        check(event.at("user_id") == std::to_string(sender_id), "Sender ID must preserve every digit");
        check(event.at("recipient_id") == std::to_string(receiver_id), "Recipient ID must preserve every digit");
        check(event.at("group_id") == std::to_string(combo_group), "Group ID must remain a precise string");
        check(event.at("gift_id") == "112233" && event.at("gift_name") == "粉丝灯牌", "Gift identity must be decoded");
        check(event.at("gift_count") == 3, "Count must be max(repeat=3,combo=2,1), never multiplied by group=9");
        check(event.at("group_count") == 9 && event.at("repeat_count") == 3 && event.at("combo_count") == 2,
              "Raw count candidates must remain available");
        check(event.at("gift_raw_counts").at("group_count") == "9", "Raw counts must also have exact decimal strings");
        check(event.at("gift_combo") == true && event.at("gift_final") == false, "Open combo must remain in progress");
        check(event.at("display_id") != event.at("event_id"), "Identified combo must have a stable display group");
        check(event.at("user_level") == 32 && event.at("fans_club").at("level") == 7,
              "Wealth and club levels must come from independent fields");
    }
    GiftOptions options;
    const auto first = one("WebcastGiftMessage", gift_wire(options), 101);
    options.combo_count = 4; options.repeat_count = 4; options.repeat_end = 1;
    const auto final = one("WebcastGiftMessage", gift_wire(options), 102);
    check(final.at("display_id") == first.at("display_id"), "Message ID/count/end changes must retain combo identity");
    check(final.at("event_id") != first.at("event_id"), "Distinct upstream messages must retain distinct event IDs");
    check(final.at("gift_final") == true && final.at("gift_count") == 4, "Explicit terminal combo must preserve cumulative count");
    check(one("WebcastGiftMessage", gift_wire(options), 102, "other-room").at("display_id") != final.at("display_id"),
          "Room must participate in combo identity");
    for (int component = 0; component < 4; ++component) {
        auto different = options;
        if (component == 0) different.sender++;
        if (component == 1) different.receiver++;
        if (component == 2) different.gift_id++;
        if (component == 3) different.group++;
        check(one("WebcastGiftMessage", gift_wire(different)).at("display_id") != final.at("display_id"),
              "Sender, recipient, gift, and group must each participate in identity");
    }
    for (int missing = 0; missing < 4; ++missing) {
        auto standalone = options;
        if (missing == 0) standalone.group = 0;
        if (missing == 1) standalone.sender = 0;
        if (missing == 2) standalone.receiver = 0;
        if (missing == 3) standalone.gift_id = 0;
        const auto event = one("WebcastGiftMessage", gift_wire(standalone));
        check(event.at("display_id") == event.at("event_id"), "Gifts without a complete group identity must remain separate observations");
    }
    auto non_combo = options;
    non_combo.combo = false;
    const auto independent_flag = one("WebcastGiftMessage", gift_wire(non_combo), 103);
    check(independent_flag.at("display_id") == final.at("display_id"),
          "GiftStruct.combo must not split a gift group with the same complete identity");
    check(independent_flag.at("gift_combo") == false && independent_flag.at("gift_final") == true,
          "Grouping must preserve the observed non-combo flag");
    const auto roomless = one("WebcastGiftMessage", gift_wire(options), 104, "");
    check(roomless.at("display_id") == roomless.at("event_id"), "Missing room identity cannot create a gift display group");
    options = {};
    options.combo_count = 0; options.repeat_count = 0; options.name = false;
    const auto fallback = one("WebcastGiftMessage", gift_wire(options));
    check(fallback.at("gift_count") == 1, "Zero count candidates must fall back to one");
    check(fallback.at("gift_name").get<std::string>().find("112233") != std::string::npos,
          "Missing gift name must display the gift ID");
    options.unknown_extensions = true;
    one("WebcastGiftMessage", gift_wire(options));
    options.unknown_extensions = false;
    options.combo_count = uint64_t(std::numeric_limits<int64_t>::max()) + 1;
    const auto invalid = parse(envelope("WebcastGiftMessage", gift_wire(options)));
    check(invalid.first.size() == 1 && invalid.first[0]["type"] == "parse_error" && invalid.second.size() == 1,
          "Count above signed 64-bit storage capacity must be quarantinable, not wrapped negative");
}

void sparse_gift_group_key() {
    const GiftOptions options;
    const auto first = one("WebcastGiftMessage", gift_wire(options));
    const auto key = first.at("gift_group_key");
    check(key.get<std::string>().rfind("gift-group:", 0) == 0, "Gift lookup key must have its own namespace");
    // Omit to_user, to_user_ids, and the entire gift structure, including combo.
    // No shared schema encoder can accidentally materialize those fields.
    const auto sparse = number(2, options.gift_id) + number(5, 4) + number(11, options.group)
        + data(7, user_wire(options.sender));
    const auto later = one("WebcastGiftMessage", sparse, 102);
    check(later.at("recipient_id") == "" && later.at("gift_combo") == false,
          "Sparse fixture must actually omit the recipient and combo declaration");
    check(later.at("gift_group_key") == key,
          "Omitted recipient/combo must retain the candidate group key");
    check(later.at("display_id") == later.at("event_id"),
          "Candidate key alone must not change parser display grouping");
    auto other_recipient = options;
    other_recipient.receiver++;
    check(one("WebcastGiftMessage", gift_wire(other_recipient)).at("gift_group_key") == key,
          "Recipient compatibility is resolved by Store, outside the candidate key");
    for (int component = 0; component < 3; ++component) {
        auto changed = options;
        if (component == 0) changed.sender++;
        if (component == 1) changed.gift_id++;
        if (component == 2) changed.group++;
        check(one("WebcastGiftMessage", gift_wire(changed)).at("gift_group_key") != key,
              "Sender, gift, and group changes must each select a different candidate key");
    }
    check(one("WebcastGiftMessage", gift_wire(options), 101, "another-room").at("gift_group_key") != key,
          "Candidate key must be scoped to its room");
    for (int missing = 0; missing < 3; ++missing) {
        auto incomplete = options;
        if (missing == 0) incomplete.sender = 0;
        if (missing == 1) incomplete.gift_id = 0;
        if (missing == 2) incomplete.group = 0;
        check(!one("WebcastGiftMessage", gift_wire(incomplete)).contains("gift_group_key"),
              "Incomplete sender/gift/group identity must not create a candidate key");
    }
    check(!one("WebcastGiftMessage", gift_wire(options), 101, "").contains("gift_group_key"),
          "An empty room ID must not create a candidate key");
}

json chat(const std::string& user) { return one("WebcastChatMessage", data(2, user) + data(3, "等级测试")); }
void unknown_field_analysis() {
    check(one("WebcastDecorationModifyMethod",number(7,1))["_details"]["field_analysis"]["entries"].size()==1,"Non-Message method names still need structural analysis");
    const auto image=data(1,"https://one.example/image")+data(1,"https://two.example/image")+data(2,"resource/key")+number(3,20)+number(4,40)+data(5,"#123456")+number(6,1)+data(8,data(4,"房管勋章"));
    const auto decodedImage=one("WebcastChatMessage",data(2,data(9,image))+data(3,"hello"))["_details"]["decoded"]["user"]["AvatarThumb"];
    check(decodedImage["url_list"].size()==2 && decodedImage["height"]=="20" && decodedImage["width"]=="40","Image schema must preserve repeated URLs and correct dimensions");
    check(decodedImage["content"]["alternative_text"]=="房管勋章","Badge alternative text must decode");
    const auto nested=data(1,"readable")+data(2,R"({"enabled":true})");
    const auto event=one("WebcastChatMessage",data(2,user_wire()+data(999,nested))+data(3,"hello"));
    const auto analysis=event["_details"]["field_analysis"];
    check(event["parse_status"]=="partial","Structural inspection must not promote unknown semantics to decoded");
    bool found=false;for(const auto& field:analysis["entries"])if(field.value("path","")=="ChatMessage.user.#999") {
        found=true;check(field["meaning_confirmed"]==false && field["candidate_only"]==true,"Nested wire must be labelled a candidate");
        check(field["protobuf_candidate"][0]["utf8_preview"]=="readable","Nested unknown UTF-8 must be visible");
        check(field["protobuf_candidate"][1]["json_value"]["enabled"]==true,"Nested JSON must expand");
    }
    check(found,"Unknown field path must include its schema parent");
    std::string many;for(int i=0;i<700;i++)many+=number(10,18446744073709551615ULL);
    const auto bounded=one("WebcastFutureOpaqueMessage",many)["_details"]["field_analysis"];
    check(bounded["truncated"]==true && bounded["entries"].size()==512,"Inspection budget must truncate oversized structures");
    check(bounded["entries"][0]["value"]=="18446744073709551615","Unsigned 64-bit unknown values must remain exact strings");
    auto recursive=number(1,1);for(int i=0;i<15;i++)recursive=data(1,recursive);
    check(one("WebcastFutureOpaqueMessage",recursive)["_details"]["field_analysis"]["truncated"]==true,"Recursive candidates must have a depth bound");
    const auto giftSort=one("WebcastGiftSortMessage",number(2,3)+data(4,data(2,varint(4095))));
    check(giftSort["type"]=="gift_notice" && giftSort["parse_status"]=="decoded" && giftSort["_details"]["decoded"]["scene_insert_strategy"]["gift_ids"][0]=="4095","Binary scene config must decode as packed gift IDs, not text or a gift transaction");
    const auto topics=one("WebcastRoomCommentTopicMessage",data(4,data(3,"话题正文")+data(5,"热聊中")));
    check(topics["content"]=="热聊中：话题正文","Verified topic fields must become readable content");
    const auto profile=one("WebcastProfileViewMessage",data(3,data(1,"profile_view_sub")+data(4,data(11,"亲密度"))));
    check(profile["content"]=="资料页提示：亲密度" && profile["user_id"]=="","Profile display hint must not invent a profile visitor identity");
}

void social_actions() {
    SocialMessage m;m.set_action(1);
    check(one("WebcastSocialMessage",m.SerializeAsString())["social_action"]=="follow","Historical action 1 should classify as follow");
    m.mutable_common()->mutable_display_text()->set_key("room_follow_msg");
    check(one("WebcastSocialMessage",m.SerializeAsString())["content"]=="关注了主播","Follow must show a concrete label");
    m.set_action(3);m.mutable_common()->mutable_display_text()->set_key("room_share_msg");
    m.mutable_common()->mutable_display_text()->set_default_pattern("{0:user} 分享了直播间");
    check(one("WebcastSocialMessage",m.SerializeAsString())["social_action"]=="share","Platform share text must classify share without guessing action codes");
    m.clear_common();m.set_action(2);
    const auto unknown=one("WebcastSocialMessage",m.SerializeAsString());
    check(unknown["social_action"]=="unknown" && unknown["content"]=="其他互动（动作 2）","Unknown action must retain code rather than invent semantics");
}

void raw_numeric_analysis() {
    const auto inspect=[](const std::string& bytes){return field_analysis(nullptr,bytes);};
    const auto fixed=[](unsigned field,uint64_t value,int width){
        auto bytes=varint((uint64_t(field)<<3)|(width==4?5:1));
        for(int i=0;i<width;++i)bytes+=char(value>>(8*i));return bytes;
    };
    const auto fields=inspect(number(1,UINT64_MAX)+number(2,3)+fixed(3,0x3fc00000,4)+fixed(4,0xc004000000000000ULL,8))["entries"];
    check(fields[0]["value"]=="18446744073709551615" && fields[0]["numeric_candidates"]["int64"]=="-1","Signed candidates must not replace raw precision");
    check(fields[0]["numeric_candidates"]["sint64"]=="-9223372036854775808" && fields[1]["numeric_candidates"]["sint64"]=="-2","ZigZag must handle negative values and the signed minimum");
    check(fields[2]["numeric_candidates"]["float"]==1.5 && fields[3]["numeric_candidates"]["double"]==-2.5,"Fixed-width bytes must offer IEEE float candidates");
    check(inspect(fixed(1,0x7f800000,4))["entries"][0]["numeric_candidates"]["float"]=="Infinity","Nonfinite floats must remain explicit JSON strings");
    const auto packed=inspect(data(1,varint(1)+varint(300)+varint(UINT64_MAX)))["entries"][0]["packed_candidates"][0];
    check(packed["candidate_only"]==true && packed["count"]==3 && packed["values"][2]["value"]=="18446744073709551615","Packed varints must preserve order and exact values");
    const auto packedFixed=inspect(data(1,std::string("\x00\x00\xc0\x3f\x00\x00\x00\xc0",8)))["entries"][0]["packed_candidates"];
    bool fixedFound=false;for(const auto& candidate:packedFixed)if(candidate["encoding"]=="fixed32") {
        fixedFound=true;check(candidate["values"][0]["interpretations"]["float"]==1.5 && candidate["values"][1]["interpretations"]["float"]==-2.0,"Packed fixed32 floats must use little endian order");
    }
    check(fixedFound,"Packed fixed32 should be offered for aligned binary data");
    for(const auto& bad:std::vector<std::string>{std::string(1,char(0x80)),std::string(9,char(0xff))+char(2),std::string(10,char(0x80))+char(0)}) {
        const auto analysis=inspect(data(1,bad));
        for(const auto& candidate:analysis["entries"][0]["packed_candidates"])
            check(candidate["encoding"]!="varint","Truncated or overflowing packed varints must not produce candidates");
    }
    check(!inspect(data(1,"hello"))["entries"][0].contains("packed_candidates"),"Readable text should not flood diagnostics with integer candidates");
    std::string many;for(int i=0;i<12;++i)many+=data(1,std::string(600,'\0'));
    const auto bounded=inspect(many);size_t values=0;
    for(const auto& entry:bounded["entries"])for(const auto& candidate:entry["packed_candidates"])values+=candidate["values"].size();
    check(bounded["truncated"]==true && values==512,"Packed previews must share a global value budget");
    check(inspect(std::string(1,char(0xff)))["wire_valid"]==false && inspect("")["wire_valid"]==true,"Invalid wire must be distinguishable from a valid empty message");
    const auto failed=parse(envelope("WebcastChatMessage",std::string(1,char(0xff))));
    check(failed.first[0]["_details"]["field_analysis"]["wire_valid"]==false,"Schema failures must retain raw analysis");
}

void additional_observed_schemas() {
    const auto battle=one("WebcastBattleStatusMessage",data(2,"90071992547409931")+data(3,"90071992547409932")+number(4,3)+number(6,300)+number(7,60)+data(8,"1700000000000")+data(9,"1700000300000"));
    check(battle["parse_status"]=="partial" && battle["user_id"]=="" && battle["type"]=="game","Battle status must not invent a viewer action");
    check(battle["_details"]["decoded"]["field_6"]=="300" && battle["_details"]["decoded"]["field_2"]=="90071992547409931","Battle wire fields should be visible with exact values");
    const auto prize=one("WebcastPrizeNoticeMessage",number(2,7424)+number(6,3)+number(8,35));
    check(prize["parse_status"]=="partial" && prize["_details"]["decoded"]["field_8"]=="35" && prize["user_id"]=="","Prize codes do not identify a winner");
    const auto visibility=one("WebcastVisibilityRangeChangeMessage",data(1,data(1,"WebcastVisibilityRangeChangeMessage"))+number(9,17));
    check(visibility["parse_status"]=="partial" && visibility["_details"]["field_analysis"]["entries"][0]["number"]==9,"Header-only schema must preserve future fields as unknown");
    const auto sync=one("WebcastRoomDataSyncMessage",number(2,sender_id)+number(4,UINT64_MAX)+data(3,"InteractEffectSyncData")+data(5,number(1,1)));
    check(sync["_details"]["decoded"]["roomID"]==std::to_string(sender_id) && sync["_details"]["decoded"]["version"]=="18446744073709551615","Room sync ID and version are wire varints, not wire strings");
    check(sync["parse_status"]=="partial" && sync["_details"]["field_analysis"]["entries"].size()==1,"Room sync opaque payload must remain available after correcting known scalar types");
}

void named_semantic_fields() {
    const auto badge=data(1,"https://example.test/badge.webp")+data(8,data(4,"等级勋章"));
    const auto buff=number(1,88)+number(2,2)+number(3,1700000000000ULL)
        +data(4,number(1,9007199254740993ULL)+number(2,128))+data(5,badge);
    const auto user=user_wire(sender_id,32)+data(23,data(26,buff))
        +data(32,number(2,1)+data(4,varint(1)+varint(128)))
        +number(54,3)+data(61,badge)+data(61,badge)+number(66,1)
        +data(68,"脱敏名称")+data(69,number(1,2))+data(73,"webcast-identity")
        +data(78,data(1,number(1,9007199254740993ULL)+data(2,badge)));
    const auto event=one("WebcastChatMessage",data(2,user)+data(3,"hello")
        +data(9,number(2,9007199254740993ULL)+number(3,128)+number(4,10)+number(6,1)+number(12,1)));
    const auto decoded=event["_details"]["decoded"],profile=decoded["user"];
    check(event["parse_status"]=="decoded" && event["_details"]["field_analysis"]["entries"].empty(),"Supported user and public-area fields must be named, not merely candidates");
    check(profile["badge_image_list_v2"].size()==2 && profile["badge_image_list_v2"][0]["content"]["alternative_text"]=="等级勋章","Repeated badge images and their labels must be decoded");
    check(profile["user_attr"]["is_admin"]==true && profile["user_attr"]["admin_privileges"][1]==128,"Packed user privileges must decode through the named schema");
    check(profile["PayGrade"]["buffInfo"]["stats_info"]["9007199254740993"]=="128","Buff map keys and values must retain 64-bit precision");
    check(profile["public_area_badge_info"]["badge_info_map"].contains("9007199254740993"),"Public-area badge map must preserve numeric keys");
    check(decoded["publicAreaCommon"]["user_consume_in_room"]=="9007199254740993" && decoded["publicAreaCommon"]["user_send_gift_cnt_in_room"]=="128","Public-area numeric fields must use varint wire types");
    check(event["user_id"]==std::to_string(sender_id) && event["user_name"]=="测试用户" && event["user_level"]==32,"Webcast identity, masked name, and buff level must not overwrite account identity or wealth level");
    const auto rank=one("WebcastRoomRankMessage",data(1,data(27,"test-room"))+data(3,data(1,user)+number(2,500)+number(3,1)));
    check(rank["_details"]["decoded"]["audience_ranks"][0]["user"]["badge_image_list_v2"].size()==2 && rank["user_id"]=="","Additional ranked users must be decoded without becoming senders");
    check(rank["parse_status"]=="decoded" && rank["_details"]["decoded"]["audience_ranks"][0]["score"]=="500" && rank["_details"]["decoded"]["audience_ranks"][0]["rank"]=="1","Ranking values must use the CDN's named score/rank definitions");
    const auto like=one("WebcastLikeMessage",data(12,number(4,100)));
    check(like["_details"]["decoded"]["public_area_common"]["individual_priority"]=="100","Like public-area information must no longer be dropped");
    const auto gift=one("WebcastGiftMessage",gift_wire()+data(15,data(44,"触发词一")+data(44,"触发词二")
        +data(59,number(1,10)+data(2,"十连送"))+data(74,badge))
        +data(21,number(5,2)+number(12,3000)+number(17,sender_id)+number(18,3))+data(39,number(2,4)));
    check(gift["_details"]["decoded"]["gift"]["trigger_words"].size()==2 && gift["_details"]["decoded"]["gift"]["group_info"][0]["group_text"]=="十连送","Gift triggers and group labels must be named fields");
    check(gift["_details"]["decoded"]["tray_info"]["origin_gift_id"]==std::to_string(sender_id) && gift["_details"]["decoded"]["tray_info"]["duration"]=="3000","Tray IDs and duration must preserve varint precision");
    check(gift["gift_count"]==one("WebcastGiftMessage",gift_wire())["gift_count"],"Display group options and tray fields must not change transaction quantity");
    const auto malformed=parse(envelope("WebcastChatMessage",data(2,data(61,std::string(1,char(0xff))))));
    check(malformed.first[0]["parse_status"]=="failed" && malformed.first[0]["_details"]["field_analysis"]["wire_valid"]==true,"Malformed typed badges must retain valid outer raw wire for diagnosis");
}

void cdn_named_protocol_fields() {
    const auto image=data(1,"https://example.test/entry.webp")+data(8,data(4,"入场勋章"));
    const auto member=one("WebcastMemberMessage",data(15,data(25,image)+data(25,image)));
    check(member["parse_status"]=="decoded" && member["_details"]["decoded"]["enter_effect_config"]["badge_list"].size()==2,"Entry badge field 25 is a repeated Image, including nested Member EffectConfig");
    const auto ranks=one("WebcastRoomRankMessage",data(3,data(1,user_wire())+number(2,100)+number(3,1))+data(3,data(1,user_wire(receiver_id))+number(2,80)+number(3,2)+number(5,20)));
    check(ranks["_details"]["decoded"]["audience_ranks"].size()==2 && ranks["_details"]["decoded"]["audience_ranks"][1]["delta"]=="20","Audience ranks must repeat; a singular proto would silently discard users");
    const auto guide=one("WebcastLowPcuGuideChatMessage",data(2,number(1,1)+data(2,number(1,2)+number(2,1234)+data(4,"挥手"))+data(2,data(4,"比心"))));
    check(guide["parse_status"]=="decoded" && guide["_details"]["decoded"]["guide_chat"]["guide_chat_config"].size()==2 && guide["type"]=="protocol","Guidance options must decode without masquerading as viewer chat");
    const auto sort=one("WebcastGiftSortMessage",number(2,3)+data(4,data(1,"anchor_exhibition")+data(2,varint(128)+varint(129))+data(10,data(1,"slice_id")+data(2,"test"))));
    check(sort["parse_status"]=="decoded" && sort["_details"]["decoded"]["scene_insert_strategy"]["gift_ids"].size()==2 && sort["_details"]["decoded"]["scene_insert_strategy"]["event_track"]["slice_id"]=="test","Sort configuration must decode packed IDs and map metadata");
    const auto dotValue=data(1,"toolbar")+data(2,data(1,"gift")+data(2,data(2,"item")+data(3,number(1,123)+number(3,2))));
    const auto dots=one("WebcastCommonDotMessage",data(2,data(1,"panel-a")+data(2,dotValue))+data(2,data(1,"panel-b")+data(2,dotValue)));
    check(dots["parse_status"]=="decoded" && dots["_details"]["decoded"]["panel_dots"].size()==2 && dots["_details"]["decoded"]["panel_dots"]["panel-a"]["item_dots"]["gift"]["dot"]["id"]=="123","Panel dots must preserve all entries and expand the nested dot structure");
    const auto chatLike=one("WebcastChatLikeMessage",data(2,number(1,9007199254740993ULL)+data(2,number(1,4)+number(2,9007199254740994ULL))));
    check(chatLike["parse_status"]=="decoded" && chatLike["_details"]["decoded"]["total_msg_data"]["9007199254740993"]["version"]=="9007199254740994","Chat like message references and versions must stay exact");
    check(chatLike["type"]=="protocol" && !chatLike.contains("like_count") && chatLike["user_id"]=="","Aggregated chat likes must not become a room-like transaction or invent a sender");
    const auto indicator=one("WebcastRoomIndicatorMessage",number(2,9)+number(3,3)+data(4,data(2,number(1,1)+data(3,"在线观众"))+data(2,number(2,100))));
    check(indicator["parse_status"]=="decoded" && indicator["_details"]["decoded"]["biz_info"]["contents"].size()==2 && !indicator.contains("online_count"),"Indicator contents must decode without guessing a room metric from the business code");
    check(one("WebcastChatLikeMessage",number(999,1))["parse_status"]=="partial","Unknown future fields must still prevent complete schema coverage");
}

void full_monitor_coverage() {
    const auto fansUpdate=one("WebcastFansclubMessage",number(2,6)+data(4,user_wire(sender_id,std::nullopt,8,2)));
    check(fansUpdate["content"]!="粉丝团事件" && fansUpdate["content"].get<std::string>().find("Lv8")!=std::string::npos,"Fans club updates without prose must expose known context instead of a generic event label");
    check(fansUpdate["content"]=="【测试粉丝团】粉丝团资料 · 当前等级 Lv8（具体动作未确认）" && fansUpdate["action"]==6,"Unknown action 6 must not be guessed as a join, upgrade or badge activation");
    check(one("WebcastFansclubMessage",number(2,7)+data(4,user_wire(sender_id,std::nullopt,1,1)))["content"]=="【测试粉丝团】粉丝团资料 · 当前等级 Lv1（具体动作未确认）","Action 7 must not be inferred from current membership");
    check(one("WebcastFansclubMessage",number(2,999))["content"]=="粉丝团资料（具体动作未确认）","Unknown actions must not fabricate a level or membership");
    check(one("WebcastFansclubMessage",number(2,1)+data(4,user_wire(sender_id,std::nullopt,7)))["content"]=="【测试粉丝团】粉丝团升级至 Lv7","Verified level-up action should show the reported level");
    check(one("WebcastFansclubMessage",number(2,2)+data(4,user_wire(sender_id,std::nullopt,1,1)))["content"]=="加入【测试粉丝团】粉丝团 · 当前等级 Lv1","Verified join action should be explained");
    check(one("WebcastFansclubMessage",number(2,1)+data(3,"原始升级文案"))["content"]=="原始升级文案","Explicit fan club prose must be retained");
    const auto fansDisplay=data(2,"恭喜 {0:user} 加入【{1:string}】")+data(4,number(1,11)+data(21,data(1,user_wire())))+data(4,number(1,1)+data(11,"测试团"));
    check(one("WebcastFansclubMessage",data(1,data(8,fansDisplay))+number(2,2)+data(3," "))["content"]=="恭喜 测试用户 加入【测试团】","Fansclub commonInfo must use display templates before action fallback");
    check(one("WebcastFansclubMessage",data(1,data(7,"平台提供的粉丝团提示"))+number(2,6))["content"]=="平台提供的粉丝团提示","Fansclub common description takes precedence over an unknown action");
    const auto display=data(2,"{0:user}等{1:string}人在说 {2:string}")
        +data(4,number(1,11)+data(21,data(1,user_wire())))
        +data(4,number(1,1)+data(11,"8"))
        +data(4,number(1,1)+data(11,"哈哈哈"));
    const auto richNotice=one("WebcastRoomMessage",data(1,data(8,display))+data(2," "));
    check(richNotice["content"]=="测试用户等8人在说 哈哈哈","Whitespace room notice must render its display_text template and pieces");
    check(richNotice["user_id"]=="","Users mentioned in a room notice must not become its sender");
    check(richNotice["_details"]["decoded"]["content"]==" ","Rendered summary must preserve the decoded source verbatim");
    check(one("WebcastCommonTextMessage",data(1,data(8,display)))["content"]==richNotice["content"],"Common text must use the same display template");
    check(one("WebcastNotifyMessage",data(1,data(8,display))+data(4,"\t\n"))["content"]==richNotice["content"],"Notify whitespace must use its display template");
    check(one("WebcastRoomMessage",data(1,data(8,display))+data(2,"已有正文"))["content"]=="已有正文","Explicit notice text must not be overwritten");
    const auto literal=data(2,"{0:string} / {0:string}")+data(4,number(1,1)+data(11,"{9:user}"));
    check(one("WebcastRoomMessage",data(1,data(8,literal)))["content"]=="{9:user} / {9:user}","Viewer text must not be recursively evaluated as a template");
    const auto invalid=data(2,"{999999999999:user}");
    check(one("WebcastRoomMessage",data(1,data(7,"备用通知正文")+data(8,invalid)))["content"]=="备用通知正文","Malformed templates must safely use the common description");
    check(one("WebcastRoomMessage",data(1,data(8,invalid))+data(2," "))["content"]=="房间通知","Unresolvable templates must not expose empty or unresolved text");
    const auto notice=one("WebcastRoomMessage",data(2,"真实房间通知"));
    check(notice["content"]=="真实房间通知" && notice["type"]=="room_notice","Room notices must expose their text");
    check(notice["_details"]["payload_base64"]==payload_base64(data(2,"真实房间通知")),"Full raw payload must round-trip");
    const auto unknown=one("WebcastFutureMessage",number(1,sender_id)+data(2,"unverified bytes"));
    check(unknown["parse_status"]=="unmapped" && unknown["type"]=="unknown","Unknown methods must be visible without invented meaning");
    check(unknown["_details"]["wire"]["fields"][0]["value"]==std::to_string(sender_id),"Wire values must preserve 64-bit precision");
    const auto partial=one("WebcastChatMessage",data(2,user_wire())+data(3,"hello")+number(999,1));
    check(partial["parse_status"]=="partial" && partial["content"]=="hello","Unknown fields must not hide a known message");
    const auto sync=one("WebcastRoomDataSyncMessage",data(5,"opaque"));
    check(sync["has_details"]==true && sync["parse_status"]=="partial","Sync payload must remain inspectable");
    Response mixed;auto* bad=mixed.add_messageslist();bad->set_method("WebcastChatMessage");bad->set_payload(std::string(1,char(0xff)));bad->set_msgid(1);
    auto* good=mixed.add_messageslist();good->set_method("WebcastRoomMessage");good->set_payload(data(2,"after failure"));good->set_msgid(2);
    PushFrame push;push.set_payload(mixed.SerializeAsString());auto env=envelope("unused","");env.set_payload(push.SerializeAsString());
    const auto parsed=parse(env);
    check(parsed.first.size()==2 && parsed.second.size()==1,"One broken message must not hide itself or the following message");
    check(parsed.first[0]["parse_status"]=="failed" && parsed.first[1]["content"]=="after failure","Malformed payload must remain visible");
    // Every top-level *Message definition must be discoverable, including ones
    // added later. Empty proto3 fixtures exercise dispatch, not field semantics.
    const auto* file=ChatMessage::descriptor()->file();int covered=0;
    for(int i=0;i<file->message_type_count();++i) {
        const auto name=file->message_type(i)->name();
        if(name=="Message" || name=="PreMessage" || name.size()<7 || name.substr(name.size()-7)!="Message")continue;
        const auto event=one("Webcast"+name,"");
        check((event["parse_status"]=="decoded" || event["parse_status"]=="partial") && event["_details"]["schema"]==name,"Existing schema must be dispatched: "+name);++covered;
    }
    check(covered>=40,"Coverage should include all current protocol message definitions");
}
void user_presence() {
    for (bool empty_children : {false, true}) {
        const auto event = chat(user_wire(sender_id, std::nullopt, std::nullopt, std::nullopt, empty_children));
        check(event.at("user_id") == std::to_string(sender_id), "64-bit user ID must survive JSON serialization");
        check(event.at("user_level").is_null(), "Missing wealth level must be unknown, not User.Level=99 or zero");
        const auto& fans = event.at("fans_club");
        check(fans.at("member").is_null() && fans.at("level").is_null() && fans.at("status").is_null(),
              "Missing club scalar fields must remain unknown even when nested messages exist");
        check(event.at("display_id") == event.at("event_id"), "Non-gift display identity must be its event identity");
    }
    const auto zero = chat(user_wire(sender_id, 0, 0, 0));
    check(zero.at("user_level") == 0 && zero.at("fans_club").at("level") == 0,
          "Explicit zero levels must be distinguished from omitted values");
    check(zero.at("fans_club").at("member") == false && zero.at("fans_club").at("member_inferred") == true,
          "Explicit club state zero maps to inferred false");
    const auto joined = chat(user_wire(sender_id, 21, 2, 1));
    check(joined.at("fans_club").at("member") == true && joined.at("fans_club").at("status") == 1,
          "Explicit club state one maps to inferred true while retaining its raw status");
    const auto unknown = chat(user_wire(sender_id, 21, 2, 2));
    check(unknown.at("fans_club").at("member").is_null() && unknown.at("fans_club").at("status") == 2,
          "Unknown club state must not be flattened to joined or not joined");
    check(unknown.at("fans_club").at("member_inferred") == false, "Unknown membership has no inferred boolean");
    const auto string_id = chat(data(1028, "18446744073709551615") + data(3, "字符串ID"));
    check(string_id.at("user_id") == "18446744073709551615", "String UID fallback must retain all digits");
    check(chat(data(1028, "not-an-id")).at("user_id") == "", "Invalid UID fallback must not form combo identity");
    const auto member = one("WebcastMemberMessage", number(12, 9007199254740993ULL));
    check(member.at("user_id") == "9007199254740993", "Member outer user_id may supply a missing nested UID");
}
void control_message_status() {
    const auto ended = one("WebcastControlMessage", number(2, 3));
    check(ended.at("type") == "system" && ended.at("control_status") == 3 && ended.at("content") == "直播已结束",
          "End-of-live control messages must expose the numeric status");
    const auto update = one("WebcastControlMessage", number(2, 1));
    check(update.at("control_status") == 1 && update.at("content") == "直播状态更新",
          "Other control statuses must remain visible without the end label");
}
void screen_chat_classified() {
    const auto pinned = one("WebcastScreenChatMessage", data(2, user_wire()) + data(4, "用户的醒目留言"));
    check(pinned.at("type") == "screen_chat" && pinned.at("content") == "用户的醒目留言",
          "Screen chat must decode with its pinned comment content");
    check(pinned.at("user_id") == std::to_string(sender_id) && pinned.at("user_name") == "测试用户",
          "Screen chat must attribute the sender");
}
void audience_ranks_captured() {
    const auto first = number(1, 500) + data(2, user_wire(111111)) + number(3, 1);
    const auto second = number(1, 300) + data(2, user_wire(222222)) + number(3, 2);
    const auto update = one("WebcastRoomUserSeqMessage", data(2, first) + data(2, second) + number(3, 789));
    check(update.at("type") == "online_count" && update.at("online_count") == 789,
          "Room user sequence must keep the online total");
    const auto& ranks = update.at("audience_ranks");
    check(ranks.size() == 2 && ranks[0].at("rank") == 1 && ranks[0].at("user_id") == "111111" &&
          ranks[0].at("user_name") == "测试用户" && ranks[0].at("score") == 500,
          "Audience contribution ranks must persist with the online update");
    check(ranks[1].at("rank") == 2 && ranks[1].at("score") == 300, "All observed ranks must stay in order");
}
#endif
} // namespace

int main() {
    try {
        packed_schema_regression();
#ifndef PARSER_BASELINE
        gift_fields_and_identity();
        sparse_gift_group_key();
        user_presence();
        full_monitor_coverage();
        social_actions();
        unknown_field_analysis();
        raw_numeric_analysis();
        additional_observed_schemas();
        named_semantic_fields();
        cdn_named_protocol_fields();
        control_message_status();
        screen_chat_classified();
        audience_ranks_captured();
#endif
        std::cout << "event parser wire regression tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "event parser test failed: " << error.what() << '\n';
        return 1;
    }
}
