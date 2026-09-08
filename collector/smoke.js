// Runs inside the collector container against the actual broker and C++ process.
const amqp = require('amqplib');
const protobuf = require('protobufjs');
const WebSocket = require('ws');
const crypto = require('node:crypto');
const zlib = require('node:zlib');
const assert = require('node:assert/strict');
const path = require('node:path');
const API = process.env.BACKEND_URL || 'http://backend:8080';
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function main() {
  const root = await protobuf.load([path.join(__dirname, 'proto/douyin.proto'), path.join(__dirname, 'proto/ingest.proto')]);
  const encode = (name, data) => { const t = root.lookupType(name); return Buffer.from(t.encode(t.fromObject(data)).finish()); };
  const run = crypto.randomUUID(), room = 'integration';
  const snapshot = await fetch(API + '/api/rooms/' + room + '/snapshot').then(r => r.json());
  const n = process.argv.includes('--burst') ? 1105 : 3;
  const messagesList = Array.from({ length: n }, (_, index) => ({
    method: 'WebcastChatMessage', msgId: String(8000000000000000000n + BigInt(Date.now()) * 1000n + BigInt(index)),
    payload: encode('ChatMessage', { common: { createTime: String(Math.floor(Date.now()/1000)) }, user: { id: '9007199254740993', nickName: '自动验证' }, content: 'pipeline-smoke:' + run + ':' + index }),
  }));
  const payload = encode('PushFrame', { payloadType: 'msg', payloadEncoding: 'gzip', payload: zlib.gzipSync(encode('Response', { messagesList })) });
  const raw = encode('pipeline.RawFrameEnvelope', { schemaVersion: 1, frameId: run, liveId: room, roomId: room, sessionId: run, receivedAtMs: String(Date.now()), payload, payloadSha256: crypto.createHash('sha256').update(payload).digest('hex'), source: 'demo', kind: 'upstream_frame' });
  const socket = new WebSocket(API.replace('http:', 'ws:').replace(':8080', ':8081') + '/ws');
  const received = new Map(); let cursor = snapshot.through_seq;
  socket.on('message', bytes => { const m = JSON.parse(bytes); if(m.type === 'event_batch') { for(const event of m.events) if(event.content.startsWith('pipeline-smoke:'+run))received.set(event.event_id,event); cursor=m.through_seq; } });
  await new Promise((resolve,reject) => {socket.once('open',resolve);socket.once('error',reject)});
  socket.send(JSON.stringify({ action:'subscribe',live_id:room,after_seq:snapshot.through_seq }));
  const conn = await amqp.connect(process.env.AMQP_URL); const ch = await conn.createConfirmChannel();
  let returned = false; ch.on('return',()=>returned=true);
  // Exact duplicate deliveries must result in one set of events.
  for(let i=0;i<2;i++)ch.publish('dy.ingest.v1','p0',raw,{mandatory:true,persistent:true,messageId:run});
  await ch.waitForConfirms(); assert.equal(returned,false);
  const deadline=Date.now()+20000;
  while(received.size<n && Date.now()<deadline)await delay(100);
  assert.equal(received.size,n,'C++ WS must deliver every decoded business event');
  for(const event of received.values()) {assert.equal(event.user_id,'9007199254740993');assert.equal(event.source,'demo');assert.ok(event.timestamp>1700000000000);}
  const all=[]; let after=snapshot.through_seq;
  for(let i=0;i<20;i++) {const page=await fetch(`${API}/api/rooms/${room}/events?after_seq=${after}`).then(r=>r.json());all.push(...page.events);if(!page.events.length)break;after=page.through_seq;}
  assert.equal(all.filter(e=>e.content.startsWith('pipeline-smoke:'+run)).length,n,'Duplicate frame must not duplicate persisted events');
  const receipts=await fetch(API+'/internal/receipts',{method:'POST',headers:{'Content-Type':'application/json','X-Internal-Token':process.env.INTERNAL_TOKEN},body:JSON.stringify([run])}).then(r=>r.json());
  assert.deepEqual(receipts,[run]);
  const reconnect = new WebSocket(API.replace('http:','ws:').replace(':8080',':8081')+'/ws');
  const replay=[]; reconnect.on('message',bytes=>{const m=JSON.parse(bytes);if(m.type==='event_batch')replay.push(...m.events)});
  await new Promise((resolve,reject)=>{reconnect.once('open',resolve);reconnect.once('error',reject)});
  reconnect.send(JSON.stringify({action:'subscribe',live_id:room,after_seq:snapshot.through_seq}));
  const replayDeadline=Date.now()+10000;while(replay.length<n&&Date.now()<replayDeadline)await delay(100);
  assert.equal(replay.filter(e=>e.content.startsWith('pipeline-smoke:'+run)).length,n,'New socket must replay committed events');
  const health=await fetch(API+'/api/health').then(r=>r.json());
  if(process.argv.includes('--cache-down'))assert.equal(health.redis,false);
  console.log(JSON.stringify({pass:true,events:n,duplicate_frames:2,preserved_64bit_id:true,ws_replay:true,redis_online:health.redis,cursor}));
  socket.close(); reconnect.close(); await ch.close(); await conn.close();
}
main().catch(e=>{console.error(e.stack);process.exit(1)});
