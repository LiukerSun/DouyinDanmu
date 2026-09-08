// Structural inspection only: a valid wire shape does not establish semantics.
// Shared budgets bound nested candidates and schema traversal independently.
struct FieldAnalysisBudget { int entries=512, nodes=8192; bool truncated=false; };
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
        case F::TYPE_VARINT:out["wire_type"]=0;out["value"]=std::to_string(field.varint());break;
        case F::TYPE_FIXED64:out["wire_type"]=1;out["value"]=std::to_string(field.fixed64());break;
        case F::TYPE_LENGTH_DELIMITED:out["wire_type"]=2;out.update(analyze_bytes(field.length_delimited(),depth,budget));break;
        case F::TYPE_GROUP:out["wire_type"]=3;if(depth<5)out["group"]=analyze_wire_fields(field.group(),depth+1,budget);else{out["inspection_truncated"]=true;budget.truncated=true;}break;
        case F::TYPE_FIXED32:out["wire_type"]=5;out["value"]=std::to_string(field.fixed32());break;
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
    FieldAnalysisBudget budget;json entries=json::array();
    if(message)analyze_schema_fields(*message,message->GetDescriptor()->name(),entries,budget);
    else {
        google::protobuf::UnknownFieldSet fields;
        if(fields.ParseFromString(payload))entries=analyze_wire_fields(fields,0,budget);
    }
    return {{"version",1},{"scope",message?"unknown_and_opaque_fields":"unmapped_payload"},{"entries",entries},{"truncated",budget.truncated},
        {"note","字段编号及 UTF-8/JSON 可直接读取；protobuf_candidate 仅为可能的嵌套结构，业务含义未确认。完整字节见原始载荷。"}};
}
