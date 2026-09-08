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
