'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs'), os = require('node:os'), path = require('node:path');
const { DatabaseSync } = require('node:sqlite');
const protobuf = require('protobufjs');
const { repair } = require('./repair-fans-profiles');

test('exact archived fans profile repairs old events and views, preserves data and is idempotent', async () => {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'fans-repair-'));
  const proto = process.env.PROTO_DIR || path.resolve(__dirname, '../backend/proto');
  const database = path.join(temp, 'test.db'), capture = path.join(temp, 'frames');
  fs.mkdirSync(capture);
  const root = await protobuf.load([path.join(proto, 'douyin.proto'), path.join(proto, 'ingest.proto')]);
  const db = new DatabaseSync(database);
  try {
    db.exec('CREATE TABLE events(id INTEGER PRIMARY KEY,event_id TEXT,live_id TEXT,body TEXT); CREATE TABLE message_views(live_id TEXT,display_id TEXT,last_seq INTEGER,body TEXT);');
    const User = root.lookupType('User');
    const uid = '9007199254740993';
    const frameFor = n => {
      const user = { [User.fieldsById[1].name]: uid, [User.fieldsById[24].name]: {data: {clubName: '原来阿',level:10,userFansClubStatus:1,anchorId:'123'}} };
      const Chat = root.lookupType('ChatMessage');
      const Response = root.lookupType('Response'), Push = root.lookupType('PushFrame'), Envelope = root.lookupType('pipeline.RawFrameEnvelope');
      const payload = Chat.encode(Chat.fromObject({user,content:'你好'})).finish();
      const response = Response.encode(Response.fromObject({messagesList:[{method:'WebcastChatMessage',msgId:String(n),wrdsVersion:'0',payload}]})).finish();
      const push = Push.encode(Push.fromObject({payload:response})).finish();
      return Envelope.encode(Envelope.fromObject({kind:'upstream_frame',frameId:`frame-${n}`,roomId:'room',liveId:'live',payload:push})).finish();
    };
    const initial = [];
    for (let n=1;n<=4;n++) {
      const body = {user_id:n===3?'another-user':uid,user_name:'观众',type:'chat',content:'你好',timestamp:1234,gift_count:0,event_id:`room:WebcastChatMessage:${n}:0`};
      if (n===2) body.fans_club = {name:'已确认',level:12,status:2,member:null};
      const live = n===4?'different-live':'live';
      db.prepare('INSERT INTO events VALUES(?,?,?,?)').run(n,body.event_id,live,JSON.stringify(body));
      db.prepare('INSERT INTO message_views VALUES(?,?,?,?)').run(live,body.event_id,n,JSON.stringify(body));
      fs.writeFileSync(path.join(capture,`${n}.bin`),frameFor(n));
      initial.push(body);
    }
    const dry = await repair({database,capture,proto});
    assert.equal(dry.events,1); assert.equal(dry.views,1);
    assert.deepEqual(JSON.parse(db.prepare('SELECT body FROM events WHERE id=1').get().body),initial[0]);
    const applied = await repair({database,capture,proto,apply:true});
    assert.ok(fs.existsSync(applied.backup));
    const body = JSON.parse(db.prepare('SELECT body FROM events WHERE id=1').get().body);
    assert.equal(body.fans_club.name,'原来阿'); assert.equal(body.fans_club.level,10);
    const {fans_club,...rest}=body; assert.deepEqual(rest,initial[0]);
    assert.deepEqual(JSON.parse(db.prepare('SELECT body FROM message_views WHERE last_seq=1').get().body),body);
    const known=JSON.parse(db.prepare('SELECT body FROM events WHERE id=2').get().body).fans_club;
    assert.equal(known.name,'已确认'); assert.equal(known.level,12); assert.equal(known.status,2);
    // Explicit unknown status must not become a contradictory inferred member.
    assert.equal(known.member,null);
    for(const n of [3,4]) assert.deepEqual(JSON.parse(db.prepare('SELECT body FROM events WHERE id=?').get(n).body),initial[n-1]);
    assert.equal(db.prepare('SELECT count(*) n FROM events').get().n,4);
    assert.equal((await repair({database,capture,proto})).events,0);
  } finally { db.close(); fs.rmSync(temp,{recursive:true,force:true}); }
});

test('fans repair synchronizes only matching analytics profiles and commits all projections atomically', async () => {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'fans-analytics-repair-'));
  const proto = process.env.PROTO_DIR || path.resolve(__dirname, '../backend/proto');
  const database = path.join(temp, 'test.db'), capture = path.join(temp, 'frames');
  fs.mkdirSync(capture);
  const root = await protobuf.load([path.join(proto, 'douyin.proto'), path.join(proto, 'ingest.proto')]);
  const encode = (name, value) => { const type = root.lookupType(name); return type.encode(type.fromObject(value)).finish(); };
  const db = new DatabaseSync(database);
  const uid = '9007199254740993';
  const run = (sql, ...values) => {
    const statement = db.prepare(sql);
    statement.setReadBigInts(true);
    return statement.run(...values);
  };
  try {
    db.exec(`CREATE TABLE events(id INTEGER PRIMARY KEY,event_id TEXT,live_id TEXT,body TEXT);
      CREATE TABLE message_views(live_id TEXT,display_id TEXT,last_seq INTEGER,body TEXT);
      CREATE TABLE analytics_events(seq INTEGER PRIMARY KEY,event_id TEXT,live_id TEXT,user_id TEXT,
        fans_club TEXT,kind TEXT,gift_quantity INTEGER,gift_unit_price INTEGER,occurred_at_ms INTEGER);`);
    const supplied = { name: 'archived club', level: 10, status: 1, member: true, member_inferred: true, anchor_id: '123' };
    for (let n = 1; n <= 9; n++) {
      const eventId = `room:WebcastChatMessage:${n}:0`;
      const seq = n === 9 ? 9007199254740993n : n;
      const body = { event_id: eventId, user_id: uid, user_name: 'viewer', type: 'chat', content: 'hello', timestamp: 1234 };
      if (n === 2) body.fans_club = supplied; // Earlier runs already repaired the journal but not analytics.
      if (n === 7) body.fans_club = { ...supplied, level: 99 }; // Archive conflicts with a confirmed journal profile.
      run('INSERT INTO events VALUES(?,?,?,?)', seq, eventId, 'live', JSON.stringify(body));
      run('INSERT INTO message_views VALUES(?,?,?,?)', 'live', eventId, seq, JSON.stringify(body));
      const fans = n === 3 ? { ...supplied, level: 12 } : null;
      run('INSERT INTO analytics_events VALUES(?,?,?,?,?,?,?,?,?)', n === 8 ? 80 : seq,
        n === 4 ? 'other-event' : eventId, n === 5 ? 'other-live' : 'live', n === 6 ? 'other-user' : uid,
        JSON.stringify(fans), 'chat', 0, null, 1234);
      const user = { id: uid, [root.lookupType('User').fieldsById[24].name]: { data: { clubName: 'archived club', level: 10, userFansClubStatus: 1, anchorId: '123' } } };
      const payload = encode('PushFrame', { payload: encode('Response', { messagesList: [
        { method: 'WebcastChatMessage', msgId: String(n), payload: encode('ChatMessage', { user, content: 'hello' }) },
      ] }) });
      fs.writeFileSync(path.join(capture, `${n}.bin`), encode('pipeline.RawFrameEnvelope', { kind: 'upstream_frame', liveId: 'live', roomId: 'room', payload }));
    }
    const rows = table => {
      const statement = db.prepare(`SELECT * FROM ${table} ORDER BY 1`);
      statement.setReadBigInts(true);
      return statement.all();
    };
    const originalAnalytics = rows('analytics_events');
    const originalEvents = rows('events'), originalViews = rows('message_views');
    const dry = await repair({ database, capture, proto });
    assert.deepEqual(rows('analytics_events'), originalAnalytics);
    assert.deepEqual(rows('events'), originalEvents);
    const applied = await repair({ database, capture, proto, apply: true });
    const repairedAnalytics = rows('analytics_events');
    assert.deepEqual(JSON.parse(repairedAnalytics[0].fans_club), supplied);
    assert.deepEqual(JSON.parse(repairedAnalytics[1].fans_club), supplied);
    for (let n = 2; n < 8; n++) assert.deepEqual(repairedAnalytics[n], originalAnalytics[n]);
    assert.deepEqual(JSON.parse(repairedAnalytics[8].fans_club), supplied);
    assert.deepEqual(repairedAnalytics.map(({ fans_club, ...rest }) => rest), originalAnalytics.map(({ fans_club, ...rest }) => rest));
    assert.equal(dry.analytics, 3); assert.equal(applied.analytics, 3);
    assert.deepEqual(JSON.parse(rows('events')[6].body), JSON.parse(originalEvents[6].body));
    const again = await repair({ database, capture, proto });
    assert.equal(again.analytics, 0); assert.equal(again.events, 0); assert.equal(again.views, 0);

    // An analytics write failure must roll back the event/view patches too.
    for (const row of originalEvents) run('UPDATE events SET body=? WHERE id=?', row.body, row.id);
    for (const row of originalViews) run('UPDATE message_views SET body=? WHERE display_id=?', row.body, row.display_id);
    for (const row of originalAnalytics) run('UPDATE analytics_events SET fans_club=? WHERE seq=?', row.fans_club, row.seq);
    db.exec("CREATE TRIGGER reject_profile BEFORE UPDATE ON analytics_events BEGIN SELECT RAISE(ABORT,'test analytics write failure'); END;");
    await assert.rejects(repair({ database, capture, proto, apply: true }), /test analytics write failure/);
    assert.deepEqual(rows('events'), originalEvents);
    assert.deepEqual(rows('message_views'), originalViews);
    assert.deepEqual(rows('analytics_events'), originalAnalytics);
  } finally { db.close(); fs.rmSync(temp, { recursive: true, force: true }); }
});
