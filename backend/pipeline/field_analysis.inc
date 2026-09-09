#include <cmath>
#include <cstring>

static constexpr int field_analysis_version=3;
static constexpr int event_parser_version=7;
// Structural inspection only: a valid wire shape does not establish semantics.
// Shared budgets bound nested candidates and schema traversal independently.
struct FieldAnalysisBudget { int entries=512, nodes=8192, packed_values=512; bool truncated=false; };
static std::string signed_bits(uint64_t value,int width) {
    const uint64_t sign=uint64_t(1)<<(width-1);
    return value&sign?"-"+std::to_string(width==64?~value+1:(uint64_t(1)<<width)-value):std::to_string(value);
}
static json numeric_candidates(uint64_t value,int wire) {
    json out={{"candidate_only",true}};
    if(wire==0) {
        out["int64"]=signed_bits(value,64);
        out["sint64"]=(value&1?"-":"")+std::to_string((value>>1)+(value&1));
    } else if(wire==5) {
        const uint32_t bits=uint32_t(value);float f;std::memcpy(&f,&bits,sizeof(f));
        out["sfixed32"]=signed_bits(value,32);
        out["float"]=std::isfinite(f)?json(f):json(std::isnan(f)?"NaN":f<0?"-Infinity":"Infinity");
    } else {
        double d;std::memcpy(&d,&value,sizeof(d));
        out["sfixed64"]=signed_bits(value,64);
        out["double"]=std::isfinite(d)?json(d):json(std::isnan(d)?"NaN":d<0?"-Infinity":"Infinity");
    }
    return out;
}
// Packed values have no tags. Require a complete parse (including overflow
// checks) before offering a candidate, and bound output across the whole tree.
static json packed_candidates(const std::string& value,FieldAnalysisBudget& budget) {
    json out=json::array();
    for(int width:{0,4,8}) {
        if(width && value.size()%width)continue;
        std::vector<uint64_t> preview;size_t pos=0,count=0;bool valid=true;
        while(pos<value.size() && valid) {
            uint64_t n=0;
            if(width)for(int i=0;i<width;++i)n|=uint64_t(uint8_t(value[pos++]))<<(8*i);
            else {
                bool ended=false;
                for(int i=0;i<10 && pos<value.size();++i) {
                    const auto byte=uint8_t(value[pos++]);
                    if(i==9 && byte>1)break;
                    n|=uint64_t(byte&127)<<(7*i);
                    if(!(byte&128)){ended=true;break;}
                }
                if(!ended){valid=false;break;}
            }
            ++count;if(preview.size()<128)preview.push_back(n);
        }
        if(!valid || count==0)continue;
        json values=json::array();
        for(const auto n:preview) {
            if(budget.packed_values<=0)break;
            --budget.packed_values;
            values.push_back({{"value",std::to_string(n)},{"interpretations",numeric_candidates(n,width==0?0:width==4?5:1)}});
        }
        const bool truncated=values.size()<count;
        if(truncated)budget.truncated=true;
        out.push_back({{"encoding",width==0?"varint":width==4?"fixed32":"fixed64"},{"candidate_only",true},
            {"count",count},{"values",std::move(values)},{"truncated",truncated}});
    }
    return out;
}
static bool readable_utf8(const std::string& value) {
    for(unsigned char c:value)if(c<32 && c!='\n' && c!='\r' && c!='\t')return false;
    try { (void)json(value).dump();return true; } catch(...) { return false; }
}
static std::string analysis_text(const std::string& value) {
    size_t end=std::min(value.size(),size_t(2048));
    while(end<value.size() && end && (uint8_t(value[end])&0xc0)==0x80)--end;
    return value.substr(0,end);
}
static bool shallow_json(const std::string& value) {
    int depth=0;bool quoted=false,escape=false;
    for(char c:value) {
        if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;continue;}
        if(c=='"')quoted=true;
        else if(c=='{' || c=='['){if(++depth>8)return false;}
        else if(c=='}' || c==']')--depth;
    }
    return true;
}
static json analyze_wire_fields(const google::protobuf::UnknownFieldSet&,int,FieldAnalysisBudget&);
static json analyze_bytes(const std::string& value,int depth,FieldAnalysisBudget& budget) {
    json out={{"bytes",value.size()},{"base64_preview",payload_base64(value.substr(0,256))},{"preview_truncated",value.size()>256}};
    if(value.size()>16384){out["inspection_truncated"]=true;budget.truncated=true;return out;}
    if(readable_utf8(value)) {
        out["utf8_preview"]=analysis_text(value);out["text_truncated"]=value.size()>2048;
        if(value.size()>2048)budget.truncated=true;
        if(!value.empty() && shallow_json(value)) {
            auto parsed=json::parse(value,nullptr,false);
            if(parsed.is_object() || parsed.is_array())out["json_value"]=std::move(parsed);
        }
    }
    if(value.empty())return out;
    // Text is already the useful interpretation; avoid treating every letter
    // as a packed integer. Non-text bytes may have several valid wire shapes.
    if(!out.contains("utf8_preview"))out["packed_candidates"]=packed_candidates(value,budget);
    if(depth>=5){out["inspection_truncated"]=true;budget.truncated=true;return out;}
    google::protobuf::UnknownFieldSet nested;
    if(nested.ParseFromString(value) && nested.field_count()>0) {
        out["protobuf_candidate"]=analyze_wire_fields(nested,depth+1,budget);
        out["candidate_only"]=true;
    }
    return out;
}
static json analyze_wire_field(const google::protobuf::UnknownField& field,int depth,FieldAnalysisBudget& budget) {
    using F=google::protobuf::UnknownField;
    json out={{"number",field.number()},{"meaning_confirmed",false}};
    switch(field.type()) {
        case F::TYPE_VARINT:out["wire_type"]=0;out["value"]=std::to_string(field.varint());out["numeric_candidates"]=numeric_candidates(field.varint(),0);break;
        case F::TYPE_FIXED64:out["wire_type"]=1;out["value"]=std::to_string(field.fixed64());out["numeric_candidates"]=numeric_candidates(field.fixed64(),1);break;
        case F::TYPE_LENGTH_DELIMITED:out["wire_type"]=2;out.update(analyze_bytes(field.length_delimited(),depth,budget));break;
        case F::TYPE_GROUP:out["wire_type"]=3;if(depth<5)out["group"]=analyze_wire_fields(field.group(),depth+1,budget);else{out["inspection_truncated"]=true;budget.truncated=true;}break;
        case F::TYPE_FIXED32:out["wire_type"]=5;out["value"]=std::to_string(field.fixed32());out["numeric_candidates"]=numeric_candidates(field.fixed32(),5);break;
    }
    return out;
}
static json analyze_wire_fields(const google::protobuf::UnknownFieldSet& fields,int depth,FieldAnalysisBudget& budget) {
    json out=json::array();
    for(int i=0;i<fields.field_count();++i) {
        if(budget.entries<=0){budget.truncated=true;break;}--budget.entries;
        out.push_back(analyze_wire_field(fields.field(i),depth,budget));
    }
    return out;
}
static void analyze_schema_fields(const google::protobuf::Message& message,const std::string& path,json& entries,FieldAnalysisBudget& budget,int depth=0) {
    if(depth>=32 || budget.nodes<=0 || budget.entries<=0){budget.truncated=true;return;}
    --budget.nodes;
    const auto* reflection=message.GetReflection();const auto& unknown=reflection->GetUnknownFields(message);
    for(int i=0;i<unknown.field_count();++i) {
        if(budget.entries<=0){budget.truncated=true;return;}--budget.entries;
        const auto& field=unknown.field(i);auto item=analyze_wire_field(field,0,budget);
        item["path"]=path+".#"+std::to_string(field.number());item["kind"]="unknown_field";
        item["message_type"]=message.GetDescriptor()->full_name();entries.push_back(std::move(item));
    }
    std::vector<const google::protobuf::FieldDescriptor*> fields;reflection->ListFields(message,&fields);
    for(const auto* field:fields) {
        if(budget.entries<=0 || budget.nodes<=0){budget.truncated=true;return;}
        const int count=field->is_repeated()?reflection->FieldSize(message,field):1;
        for(int i=0;i<count;++i) {
            if(budget.entries<=0 || budget.nodes<=0){budget.truncated=true;return;}--budget.nodes;
            const auto next=path+"."+field->name()+(field->is_repeated()?"["+std::to_string(i)+"]":"");
            if(field->type()==google::protobuf::FieldDescriptor::TYPE_BYTES) {
                --budget.entries;auto item=analyze_bytes(field->is_repeated()?reflection->GetRepeatedString(message,field,i):reflection->GetString(message,field),0,budget);
                item["path"]=next;item["kind"]="opaque_bytes";item["number"]=field->number();item["meaning_confirmed"]=false;entries.push_back(std::move(item));
            } else if(field->cpp_type()==google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
                analyze_schema_fields(field->is_repeated()?reflection->GetRepeatedMessage(message,field,i):reflection->GetMessage(message,field),next,entries,budget,depth+1);
            }
        }
    }
}
static json field_analysis(const google::protobuf::Message* message,const std::string& payload) {
    FieldAnalysisBudget budget;json entries=json::array();bool valid=true;
    if(message)analyze_schema_fields(*message,message->GetDescriptor()->name(),entries,budget);
    else {
        google::protobuf::UnknownFieldSet fields;
        valid=fields.ParseFromString(payload);
        if(valid)entries=analyze_wire_fields(fields,0,budget);
    }
    return {{"version",field_analysis_version},{"wire_valid",valid},{"scope",message?"unknown_and_opaque_fields":"unmapped_payload"},{"entries",entries},{"truncated",budget.truncated},
        {"note","字段编号及 UTF-8/JSON 可直接读取；protobuf_candidate、numeric_candidates、packed_candidates 均为候选解释，不能据此确认类型或业务含义。可读文本不尝试 packed 解释。完整字节见原始载荷。"}};
}
