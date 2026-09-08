    // Delivery can be reordered by spool recovery or broker redelivery. Within
    // one session, sequence is authoritative; timestamps order different sessions.
    // This watermark is separate from rooms.updated_at (which also tracks stats).
    void observe_connection_unlocked(const pipeline::RawFrameEnvelope& frame,const std::string& status,const std::string& detail) {
        if(frame.session_id().empty() || !frame.session_seq() || frame.received_at_ms()<=0)return;
        Statement target(db,"SELECT version,enabled FROM rooms WHERE live_id=?");target.text(1,frame.live_id());
        if(!target.row() || !target.number(1) || uint64_t(target.number(0))!=frame.desired_version())return;
        int64_t observed=frame.received_at_ms();
        Statement prior(db,"SELECT desired_version,session_id,session_seq,observed_at FROM room_connection_state WHERE live_id=?");prior.text(1,frame.live_id());
        if(prior.row() && uint64_t(prior.number(0))==frame.desired_version()) {
            if(prior.str(1)==frame.session_id()) {
                if(frame.session_seq()<=std::stoull(prior.str(2)))return;
                observed=std::max(observed,prior.number(3));
            } else if(observed<=prior.number(3))return;
        }
        Statement watermark(db,"INSERT INTO room_connection_state VALUES(?,?,?,?,?) ON CONFLICT(live_id) DO UPDATE SET desired_version=excluded.desired_version,session_id=excluded.session_id,session_seq=excluded.session_seq,observed_at=excluded.observed_at");
        watermark.text(1,frame.live_id()).num(2,frame.desired_version()).text(3,frame.session_id()).text(4,std::to_string(frame.session_seq())).num(5,observed).row();
        Statement update(db,"UPDATE rooms SET status=?,detail=?,updated_at=? WHERE live_id=?");
        update.text(1,status).text(2,detail).num(3,now_ms()).text(4,frame.live_id()).row();
    }
