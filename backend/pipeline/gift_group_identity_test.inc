// Issue #9 fixtures go through the production wire parser and Store transaction.
struct GroupGiftFixture {
    uint64_t sender=123, recipient=456, gift_id=4095, group=42;
    uint64_t count=1;
    bool combo=false, final=false, omit_gift=false;
    std::string live="111", internal="internal-111";
};

static pipeline::RawFrameEnvelope group_gift_frame(const GroupGiftFixture& options) {
    static uint64_t sequence=100000;
    ++sequence;
    GiftMessage message;
    message.set_gift_id(options.gift_id);message.set_group_id(options.group);
    message.set_repeat_count(options.count);message.set_repeat_end(options.final?1:0);
    message.mutable_common()->set_create_time(1700000000000LL+sequence);
    message.mutable_user()->set_id(options.sender);message.mutable_user()->set_nickname("Issue 9 fixture");
    if(options.recipient)message.mutable_to_user()->set_id(options.recipient);
    if(!options.omit_gift) {
        message.mutable_gift()->set_id(options.gift_id);message.mutable_gift()->set_name("嘉年华");
        message.mutable_gift()->set_combo(options.combo);message.mutable_gift()->set_diamondcount(30000);
    }
    Response response;auto* item=response.add_messageslist();item->set_method("WebcastGiftMessage");
    item->set_msgid(sequence);item->set_payload(message.SerializeAsString());
    PushFrame push;push.set_payloadtype("msg");push.set_payload(response.SerializeAsString());
    auto envelope=frame(options.live,json::object());envelope.set_room_id(options.internal);
    envelope.set_payload(push.SerializeAsString());envelope.set_payload_sha256(digest(envelope.payload()));
    return envelope;
}

static json ingest_group_gift(Store& store,const pipeline::RawFrameEnvelope& envelope) {
    auto decoded=parse(envelope);
    check(decoded.second.empty() && decoded.first.size()==1,"Gift group fixture must decode without errors");
    store.commit(envelope,envelope.SerializeAsString(),decoded.first,decoded.second);
    return decoded.first[0];
}

static json ingest_group_gift(Store& store,const GroupGiftFixture& options) {
    return ingest_group_gift(store,group_gift_frame(options));
}

static void check_group_value(Store& store,const std::string& live,int64_t quantity,int64_t value) {
    AnalyticsQuery query;query.scope.room=live;
    const auto ranking=store.analytics_ranking(query,true).at("items");
    check(ranking.size()==1 && ranking[0]["gift_quantity"]==std::to_string(quantity) &&
          ranking[0]["known_gift_value"]==std::to_string(value),
          "Gift rankings must count cumulative increments once and preserve their exact prices");
}

void non_combo_group_counts_progress_once() {
    TemporaryDatabase database;GroupGiftFixture options;
    pipeline::RawFrameEnvelope final_frame;json saved;
    {
        Store store;store.target(options.live,"douyin",true);
        for(int count=1;count<=10;++count) {
            options.count=count;options.final=count==10;
            final_frame=group_gift_frame(options);ingest_group_gift(store,final_frame);
        }
        check_stats(store,options.live,1,10);
        check_group_value(store,options.live,10,300000);
        check(store.snapshot(options.live)["events"].size()==1,
              "Non-combo cumulative 1..10 must be one display group, not ten deliveries added together");
        const auto history=store.events(options.live,0);
        check(history.size()==10,"All distinct cumulative gift deliveries must stay in the journal");
        for(size_t index=0;index<history.size();++index)
            check(history[index]["gift_count"]==index+1 && history[index]["gift_combo"]==false,
                  "Grouped delivery facts retain their own cumulative count and original false combo flag");
        MessageSearch grouped;grouped.room=options.live;grouped.type="gift";
        check(store.search_messages(grouped)["total"]==1,"Grouped archive must agree with the one-row snapshot");
        grouped.deliveries=true;
        check(store.search_messages(grouped)["total"]==10,"Raw archive must retain all ten upstream observations");
        ingest_group_gift(store,final_frame);
        ingest_group_gift(store,options); // A new message ID reporting the same high-water mark.
        options.count=2;options.final=false;ingest_group_gift(store,options);
        check_stats(store,options.live,1,10);check_group_value(store,options.live,10,300000);
        check(store.snapshot(options.live)["events"][0]["gift_count"]==10,
              "Repeated and out-of-order non-combo progress must not inflate or roll back the group");
        saved=store.snapshot(options.live);
    }
    {
        Store reopened;check(reopened.snapshot(options.live)==saved,"Non-combo grouping must survive restart");
        ingest_group_gift(reopened,final_frame);
        options.count=11;ingest_group_gift(reopened,options);
        check_stats(reopened,options.live,1,11);check_group_value(reopened,options.live,11,330000);
        check(reopened.events(options.live,0).size()==13,"Restart must preserve deduplication and original deliveries");
    }
}

void gift_group_identity_keeps_independent_deliveries_separate() {
    for(int component=0;component<6;++component) {
        TemporaryDatabase database;Store store;GroupGiftFixture first,other;
        store.target(first.live,"douyin",true);
        if(component==0)other.group++;
        if(component==1)other.recipient++;
        if(component==2)other.sender++;
        if(component==3)other.gift_id++;
        if(component==4){other.live="222";other.internal="internal-222";store.target(other.live,"douyin",true);}
        if(component==5)other.internal="another-internal-room";
        ingest_group_gift(store,first);ingest_group_gift(store,other);
        if(component==4){check_stats(store,first.live,1,1);check_stats(store,other.live,1,1);}
        else check_stats(store,first.live,2,2);
    }
    for(int missing=0;missing<4;++missing) {
        TemporaryDatabase database;Store store;GroupGiftFixture options;
        store.target(options.live,"douyin",true);
        if(missing==0)options.group=0;
        if(missing==1)options.sender=0;
        if(missing==2)options.gift_id=0;
        if(missing==3)options.internal="";
        ingest_group_gift(store,options);options.count=2;ingest_group_gift(store,options);
        check_stats(store,options.live,2,3);
        check(store.snapshot(options.live)["events"].size()==2,
              "Zero or missing identity fields cannot authorize merging different observations");
    }
}

void non_combo_sparse_recipients_require_one_candidate() {
    for(bool initially_unknown:{false,true}) {
        TemporaryDatabase database;Store store;GroupGiftFixture options;
        store.target(options.live,"douyin",true);
        if(initially_unknown)options.recipient=0;
        ingest_group_gift(store,options);options.count=2;options.recipient=0;ingest_group_gift(store,options);
        options.count=3;options.recipient=456;ingest_group_gift(store,options);
        check_stats(store,options.live,1,3);check_group_value(store,options.live,3,90000);
        check(store.snapshot(options.live)["events"][0]["gift_combo"]==false,
              "Resolving one compatible recipient must not fabricate a true combo flag");
        options.recipient=789;options.count=1;ingest_group_gift(store,options);
        check_stats(store,options.live,2,4);
        options.recipient=0;options.count=4;ingest_group_gift(store,options);
        check_stats(store,options.live,3,8);
        check(store.snapshot(options.live)["events"].size()==3,
              "Missing recipient with two conflicting recipients must stay a separate observation");
        options.recipient=456;options.count=4;ingest_group_gift(store,options);
        check_stats(store,options.live,3,9);
    }
    // Gift detail omission must not overwrite the original metadata flag in facts.
    TemporaryDatabase database;Store store;GroupGiftFixture options;options.combo=true;
    store.target(options.live,"douyin",true);ingest_group_gift(store,options);
    options.count=2;options.omit_gift=true;ingest_group_gift(store,options);
    check_stats(store,options.live,1,2);
    check(store.events(options.live,0).back()["gift_combo"]==false,
          "An omitted combo flag must stay false in the immutable delivery fact");
}
