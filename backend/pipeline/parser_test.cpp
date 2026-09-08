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
        for (unsigned field : {21, 24, 31, 37, 38, 39, 40, 41, 42}) payload += data(field, std::string(1, char(0xff)));
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
        if (missing == 3) standalone.combo = false;
        const auto event = one("WebcastGiftMessage", gift_wire(standalone));
        check(event.at("display_id") == event.at("event_id"), "Unidentified/non-combo gifts must remain separate observations");
        if (missing == 3) check(event.at("gift_final") == true, "Non-combo gift is immediately final");
    }
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
    const auto event=one("WebcastChatMessage",data(2,user_wire()+data(61,nested))+data(3,"hello"));
    const auto analysis=event["_details"]["field_analysis"];
    check(event["parse_status"]=="partial","Structural inspection must not promote unknown semantics to decoded");
    bool found=false;for(const auto& field:analysis["entries"])if(field.value("path","")=="ChatMessage.user.#61") {
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
    check(giftSort["type"]=="gift_notice" && giftSort["parse_status"]=="partial","Binary scene config must not be decoded as UTF-8 or a gift transaction");
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

void full_monitor_coverage() {
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
#endif
        std::cout << "event parser wire regression tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "event parser test failed: " << error.what() << '\n';
        return 1;
    }
}
