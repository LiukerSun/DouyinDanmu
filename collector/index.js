const amqp = require('amqplib');
const protobuf = require('protobufjs');
const WebSocket = require('ws');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const { fetchRoomInfo } = require('./room-info');
const { roomCookies } = require('./auth-cookie');
const { createSettingsServer } = require('./settings-server');
const { reconnectRoom } = require('./session-control');
const { syncDirectory } = require('./durable-files');

const AMQP_URL = process.env.AMQP_URL;
const LOCAL_TRANSPORT = process.env.INGEST_TRANSPORT === 'local';
const API = process.env.BACKEND_URL || 'http://backend:8080';
const TOKEN = process.env.INTERNAL_TOKEN || 'local-collector-token';
const SPOOL = process.env.SPOOL_DIR || '/data/spool';
const RAW_CAPTURE = process.env.RAW_CAPTURE_DIR;
const MAX_RAW_CAPTURE = 64 * 1024 * 1024;
let rawCaptureBytes = 0, rawCaptureFull = false;
const MAX_SPOOL_BYTES = 64 * 1024 * 1024;
const MAX_FRAME_BYTES = 8 * 1024 * 1024;
const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36';
const sessions = new Map(), published = new Map();
let connection, channel, root, Envelope, PushFrame, Response;
let running = true, spoolBytes = 0, mqConnected = false, totalPublished = 0, totalReceived = 0;
let settingsServer;
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function waitForSession(session, ms) {
  const until = Date.now() + ms;
  while (session.active && running && Date.now() < until) await sleep(Math.min(250, until - Date.now()));
}
const hash = data => crypto.createHash('sha256').update(data).digest('hex');
const encode = (name, data) => Buffer.from(root.lookupType(name).encode(root.lookupType(name).fromObject(data)).finish());
async function api(route, body) {
  const response = await fetch(API + '/internal/' + route, {
    method: body === undefined ? 'GET' : 'POST',
    headers: { 'X-Internal-Token': TOKEN, 'Content-Type': 'application/json' },
    body: body === undefined ? undefined : JSON.stringify(body), signal: AbortSignal.timeout(4000),
  });
  if (!response.ok) throw new Error(`Backend ${route}: ${response.status}`);
  return response.json();
}
function persist(session, payload, kind = 'upstream_frame') {
  if (payload.length > MAX_FRAME_BYTES || spoolBytes + payload.length > MAX_SPOOL_BYTES) throw new Error('本地采集缓冲已满或帧超限');
  const id = crypto.randomUUID();
  const body = Buffer.from(Envelope.encode(Envelope.fromObject({
    schemaVersion: 1, frameId: id, liveId: session.live_id, roomId: session.room_id || session.live_id,
    sessionId: session.id, sessionSeq: String(++session.seq), receivedAtMs: String(Date.now()),
    payload, payloadSha256: hash(payload), source: session.source, kind, desiredVersion: String(session.version),
  })).finish());
  const tmp = path.join(SPOOL, id + '.tmp'), target = path.join(SPOOL, id + '.bin');
  const fd = fs.openSync(tmp, 'wx');
  try { fs.writeFileSync(fd, body); fs.fsyncSync(fd); } finally { fs.closeSync(fd); }
  fs.renameSync(tmp, target);
  syncDirectory(SPOOL);
  spoolBytes += body.length; totalReceived++;
  if (RAW_CAPTURE && kind === 'upstream_frame' && !rawCaptureFull) {
    try {
      if (rawCaptureBytes + body.length > MAX_RAW_CAPTURE) { rawCaptureFull = true; console.error('[capture] diagnostic archive reached 64 MiB'); }
      else { fs.writeFileSync(path.join(RAW_CAPTURE, id + '.bin'), body, {flag:'wx'}); rawCaptureBytes += body.length; }
    } catch(e) { rawCaptureFull = true; console.error('[capture]', e.message); }
  }
  return id;
}
function status(session, state, detail = '') {
  if (!session.active) return;
  try { persist(session, Buffer.from(JSON.stringify({ status: state, detail })), 'collector_status'); }
  catch (e) { console.error('[status]', e.message); }
}
async function connectQueue() {
  const conn = await amqp.connect(AMQP_URL + '?heartbeat=15');
  conn.on('error', () => {});
  conn.on('close', () => { if (connection === conn) { channel = undefined; mqConnected = false; } });
  connection = conn;
  const ch = await conn.createConfirmChannel();
  ch.on('error', () => {});
  ch.on('close', () => { if (channel === ch) { channel = undefined; mqConnected = false; } });
  await ch.assertExchange('dy.ingest.v1', 'direct', { durable: true });
  await ch.assertExchange('dy.dead.v1', 'direct', { durable: true });
  await ch.assertQueue('dy.dead.v1.p0', { durable: true, arguments: { 'x-queue-type': 'quorum', 'x-quorum-initial-group-size': 1, 'x-delivery-limit': -1, 'x-overflow': 'reject-publish', 'x-max-length-bytes': 67108864 } });
  await ch.bindQueue('dy.dead.v1.p0', 'dy.dead.v1', 'p0');
  await ch.assertQueue('dy.raw.v1.p0', { durable: true, arguments: { 'x-queue-type': 'quorum', 'x-quorum-initial-group-size': 1, 'x-single-active-consumer': true } });
  await ch.bindQueue('dy.raw.v1.p0', 'dy.ingest.v1', 'p0');
  // Local deployment owns topology. Confirm the reliability policy before publishing.
  const u = new URL(AMQP_URL), vhost = decodeURIComponent(u.pathname.slice(1));
  const policy = await fetch(`http://${u.hostname}:15672/api/policies/${encodeURIComponent(vhost)}/raw-reliability`, {
    method: 'PUT', headers: { 'Content-Type': 'application/json', Authorization: 'Basic ' + Buffer.from(decodeURIComponent(u.username) + ':' + decodeURIComponent(u.password)).toString('base64') },
    body: JSON.stringify({ pattern: '^dy\\.raw\\.v1\\.p0$', 'apply-to': 'quorum_queues', priority: 10, definition: { 'delivery-limit': 20, 'overflow': 'reject-publish', 'max-length-bytes': 134217728, 'dead-letter-exchange': 'dy.dead.v1', 'dead-letter-routing-key': 'p0', 'dead-letter-strategy': 'at-least-once' } }),
    signal: AbortSignal.timeout(5000),
  });
  if (!policy.ok) { await conn.close(); throw new Error('RabbitMQ reliability policy failed: ' + policy.status); }
  ch.returned = new Set();
  ch.on('return', message => ch.returned.add(message.properties.messageId));
  channel = ch; mqConnected = true;
  console.log('[collector] RabbitMQ confirm channel ready');
}
async function publishFile(filename) {
  const id = filename.slice(0, -4), ch = channel;
  const body = fs.readFileSync(path.join(SPOOL, filename));
  if (LOCAL_TRANSPORT) {
    const response = await fetch(API + '/internal/ingest', {
      method: 'POST', headers: { 'X-Internal-Token': TOKEN, 'Content-Type': 'application/x-protobuf' },
      body, signal: AbortSignal.timeout(15000),
    });
    if (!response.ok || !(await response.json()).committed) throw new Error('Local commit not confirmed');
    published.set(id, Date.now()); totalPublished++; return;
  }
  await new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(new Error('Publisher confirm timeout')), 5000);
    ch.publish('dy.ingest.v1', 'p0', body, { persistent: true, mandatory: true, contentType: 'application/x-protobuf', messageId: id }, err => {
      clearTimeout(timeout);
      if (err || ch.returned.delete(id)) reject(err || new Error('Message could not be routed'));
      else resolve();
    });
  });
  published.set(id, Date.now()); totalPublished++;
}
async function deliveryLoop() {
  while (running) {
    try {
      if (!LOCAL_TRANSPORT && !channel) await connectQueue();
      const files = fs.readdirSync(SPOOL).filter(f => f.endsWith('.bin'));
      for (let i = 0; i < files.length && running; i += 128) {
        const batch = files.slice(i, i + 128);
        const done = new Set(await api('receipts', batch.map(f => f.slice(0, -4))));
        if (LOCAL_TRANSPORT) mqConnected = true;
        for (const file of batch) {
          const id = file.slice(0, -4), full = path.join(SPOOL, file);
          if (done.has(id)) { spoolBytes -= fs.statSync(full).size; fs.unlinkSync(full); published.delete(id); }
          else if (!published.has(id) || Date.now() - published.get(id) > 300000) await publishFile(file);
        }
      }
    } catch (e) {
      console.error('[delivery]', e.message);
      if (connection) { try { await connection.close(); } catch {} }
      channel = undefined; mqConnected = false; await sleep(2000);
    }
    await sleep(200);
  }
}
const names = ['小林', '山间晚风', '橘子汽水', '阿远', '追光的人', '小满', '蓝色星球', '清晨咖啡'];
const chats = ['晚上好，今天的分享很有收获', '这个思路讲得很清楚，收藏了', '刚进直播间，赶上啦', '画面和声音都很流畅', '支持主播，继续加油！', '原来是这样，学到了', '期待下一期的内容', '一边喝咖啡一边听，舒服'];
function demoFrame(session) {
  const n = session.demoTick++, millis = Date.now(), id = BigInt(millis) * 1000n + BigInt(n % 1000);
  const common = { msgId: String(id), roomId: '9000000001', createTime: String(Math.floor(millis / 1000)) };
  const user = { id: String(9007199254740993n + BigInt(n % names.length)), nickName: names[n % names.length] };
  let type = 'ChatMessage', data = { common, user, content: chats[n % chats.length] };
  if (n % 7 === 0) { type = 'GiftMessage'; data = { common, user, gift: { name: n % 2 ? '小心心' : '玫瑰', id: '1' }, comboCount: String(n % 3 + 1), repeatEnd: 1 }; }
  else if (n % 5 === 0) { type = 'MemberMessage'; data = { common, user }; }
  else if (n % 4 === 0) { type = 'LikeMessage'; data = { common, user, count: String(n % 9 + 1) }; }
  const messagesList = [{ method: 'Webcast' + type, msgId: String(id), payload: encode(type, data) }, {
    method: 'WebcastRoomUserSeqMessage', msgId: String(id + 1n), payload: encode('RoomUserSeqMessage', { common, total: String(1200 + n % 90) }),
  }];
  const response = encode('Response', { messagesList, needAck: false });
  return encode('PushFrame', { logId: String(id), payloadType: 'msg', payloadEncoding: 'gzip', payload: zlib.gzipSync(response) });
}
async function demo(session) {
  session.room_id = '9000000001'; status(session, 'collecting', '模拟直播数据，经过完整 RabbitMQ / C++ / 数据库链路');
  while (session.active && running) {
    try { persist(session, demoFrame(session)); }
    catch(e) { status(session, 'backpressured', e.message); await sleep(2000); }
    await sleep(Number(process.env.DEMO_INTERVAL_MS || 1000));
  }
}
const getRoom = fetchRoomInfo;
function saveRoomInfo(session, metadata) {
  if (!session.active || !running) return;
  persist(session, Buffer.from(JSON.stringify(metadata)), 'room_metadata');
  session.metadata = metadata;
}
function unknownRoomInfo(session) {
  // Keep the last profile while making an unsuccessful status observation explicit.
  saveRoomInfo(session, {
    room_id: session.metadata?.room_id || session.room_id || '', title: session.metadata?.title || '',
    anchor: session.metadata?.anchor || {id:'',sec_uid:'',nickname:'',avatar_url:'',signature:'',display_id:''},
    live_started_at_ms: session.metadata?.live_started_at_ms || null,
    live_status: 'unknown', checked_at_ms: Date.now(),
  });
}
let getSign;
function liveUrl(roomId) {
  if (!getSign) getSign = new Function('require', fs.readFileSync(fs.existsSync(path.join(__dirname, 'sign.js')) ? path.join(__dirname, 'sign.js') : path.join(__dirname, '../backend/scripts/sign.js'), 'utf8') + '\nreturn get_sign;')(require);
  const uid = String(7000000000000000000n + BigInt('0x' + crypto.randomBytes(7).toString('hex')));
  const signed = ['live_id=1','aid=6383','version_code=180800','webcast_sdk_version=1.0.14-beta.0','room_id='+roomId,'sub_room_id=','sub_channel_id=','did_rule=3','user_unique_id='+uid,'device_platform=web','device_type=','ac=','identity=audience'].join(',');
  const signature = getSign(crypto.createHash('md5').update(signed).digest('hex'));
  const params = new URLSearchParams({ aid:'6383',app_name:'douyin_web',browser_language:'zh-CN',browser_name:'Mozilla',browser_online:'true',browser_platform:'Win32',browser_version:UA.slice(8),compress:'gzip',cookie_enabled:'true',cursor:`d-1_u-1_fh-${uid}_t-${Date.now()}_r-1`,device_platform:'web',device_type:'',did_rule:'3',endpoint:'live_pc',heartbeatDuration:'0',host:'https://live.douyin.com',identity:'audience',im_path:'/webcast/im/fetch/',internal_ext:`internal_src:dim|wss_push_room_id:${roomId}|wss_push_did:${uid}|dim_log_id:${crypto.randomBytes(4).toString('hex')}`,live_id:'1',need_persist_msg_count:'15',room_id:roomId,screen_height:'864',screen_width:'1536',sub_channel_id:'',sub_room_id:'',support_wrds:'1',tz_name:'Asia/Shanghai',update_version_code:'1.0.14-beta.0',user_unique_id:uid,version_code:'180800',webcast_sdk_version:'1.0.14-beta.0',signature });
  return 'wss://webcast100-ws-web-lq.douyin.com/webcast/im/push/v2/?' + params;
}
async function live(session) {
  let attempt = 0;
  while (session.active && running) {
    let roomObserved = false;
    try {
      status(session, 'connecting', '正在连接抖音直播');
      const room = await getRoom(session.live_id); if (!session.active) return;
      session.room_id = room.roomId;
      saveRoomInfo(session, room.metadata);
      roomObserved = true;
      if (room.metadata.live_status !== 'live') {
        const offline = room.metadata.live_status === 'offline';
        status(session, offline ? 'waiting_live' : 'failed', offline ? '主播未开播，每分钟自动检查' : '暂时无法确认开播状态，每分钟重新检查');
        attempt = 0; await waitForSession(session, 60000); continue;
      }
      await new Promise((resolve, reject) => {
        const ws = new WebSocket(liveUrl(room.roomId), { headers: { Cookie: room.cookie, 'User-Agent': UA }, handshakeTimeout: 10000, maxPayload: MAX_FRAME_BYTES });
        session.ws = ws; let heartbeat, observation, observing = false, closed = false;
        const cleanup = () => { closed = true; clearInterval(heartbeat); clearInterval(observation); };
        const refresh = async () => {
          if (observing || closed || !session.active) return;
          observing = true;
          try {
            const current = await getRoom(session.live_id);
            if (closed || !session.active) return;
            saveRoomInfo(session, current.metadata);
            if (current.metadata.live_status === 'offline' || (current.roomId && current.roomId !== room.roomId)) ws.close();
          } catch (e) {
            if (!closed && session.active) {
              try { unknownRoomInfo(session); } catch (error) { console.error('[room-info]', error.message); }
              console.error('[room-info]', session.live_id, e.message);
            }
          } finally { observing = false; }
        };
        ws.on('open', () => {
          if (!session.active || !running || closed) { ws.close(); return; }
          attempt = 0; status(session, 'collecting', '已连接抖音，等待直播消息');
          heartbeat = setInterval(() => { if (ws.readyState === WebSocket.OPEN) ws.send(encode('PushFrame', { payloadType: 'hb' })); }, 5000);
          observation = setInterval(refresh, 60000);
        });
        ws.on('message', raw => {
          if (!session.active || !running || closed) return;
          try {
            // Durable receipt precedes the upstream ACK. No business payload is decoded here.
            persist(session, Buffer.from(raw));
            const frame = PushFrame.decode(raw);
            if (frame.payloadType === 'hb' || !frame.payload.length) return;
            const payload = frame.payload[0] === 0x1f && frame.payload[1] === 0x8b ? zlib.gunzipSync(frame.payload, { maxOutputLength: 16 * 1024 * 1024 }) : frame.payload;
            const response = Response.decode(payload);
            if (response.needAck) ws.send(encode('PushFrame', { logId: String(frame.logId), payloadType: 'ack', payload: Buffer.from(response.internalExt) }));
          } catch (e) { status(session, 'failed', e.message); ws.close(); }
        });
        ws.on('error', err => { cleanup(); reject(err); });
        ws.on('close', () => { cleanup(); resolve(); });
      });
    } catch (e) {
      if (!roomObserved) { try { unknownRoomInfo(session); } catch (error) { console.error('[room-info]', error.message); } }
      status(session, 'failed', e.message); console.error('[live]', session.live_id, e.message);
    }
    if (session.active) { attempt++; await waitForSession(session, Math.min(30000, 2000 * attempt)); }
  }
}
async function reconcileLoop() {
  while (running) {
    try {
      const targets = await api('targets');
      for (const [id, session] of sessions) {
        const target = targets.find(t => t.live_id === id);
        if (!target?.enabled || target.version !== session.version) { session.active = false; session.ws?.close(); sessions.delete(id); getRoom.clearCookieCache(id); }
      }
      for (const target of targets) {
        if (!target.enabled || sessions.has(target.live_id)) continue;
        const session = { ...target, id: crypto.randomUUID(), seq: 0, demoTick: 0, active: true };
        sessions.set(target.live_id, session);
        (target.source === 'demo' ? demo(session) : live(session)).catch(e => console.error('[session]', e.message));
      }
      const roomAuth = Object.fromEntries(targets.filter(t => t.source === 'douyin').map(t => [t.live_id, roomCookies.forRoom(t.live_id).status()]));
      await api('heartbeat', { rabbitmq: mqConnected, spool_bytes: spoolBytes, published: totalPublished, received: totalReceived, sessions: sessions.size, raw_capture_bytes: rawCaptureBytes, raw_capture_full: rawCaptureFull, room_auth: roomAuth });
    } catch(e) { console.error('[control]', e.message); }
    await sleep(2000);
  }
}
function reconnectDouyinSession(liveId) {
  getRoom.clearCookieCache(liveId);
  return reconnectRoom(sessions, liveId, session => status(session, 'reconnecting', '本直播间采集身份已更新，正在重新连接'));
}
async function main() {
  fs.mkdirSync(SPOOL, { recursive: true });
  if (RAW_CAPTURE) {
    fs.mkdirSync(RAW_CAPTURE, {recursive:true});
    rawCaptureBytes = fs.readdirSync(RAW_CAPTURE).reduce((n, file) => n + fs.statSync(path.join(RAW_CAPTURE,file)).size, 0);
    rawCaptureFull = rawCaptureBytes >= MAX_RAW_CAPTURE;
  }
  // Recover complete fsynced records left before rename after an interrupted write.
  root = await protobuf.load([path.join(__dirname,'proto/douyin.proto'),path.join(__dirname,'proto/ingest.proto')]);
  Envelope = root.lookupType('pipeline.RawFrameEnvelope'); PushFrame = root.lookupType('PushFrame'); Response = root.lookupType('Response');
  for (const f of fs.readdirSync(SPOOL)) {
    if (f.endsWith('.tmp')) {
      const full = path.join(SPOOL,f); try { Envelope.decode(fs.readFileSync(full)); fs.renameSync(full,full.replace(/\.tmp$/,'.bin')); } catch { console.error('[spool] incomplete record retained:', f); }
    }
  }
  spoolBytes = fs.readdirSync(SPOOL).reduce((sum,f) => sum + fs.statSync(path.join(SPOOL,f)).size,0);
  settingsServer = createSettingsServer({ roomCookies,
    isKnownRoom: async liveId => (await api('targets')).some(t => t.live_id === liveId && t.source === 'douyin'),
    onChange: reconnectDouyinSession,
    ...(process.env.AUTH_ORIGINS ? { allowedOrigins: process.env.AUTH_ORIGINS.split(',') } : {}),
  });
  await new Promise((resolve, reject) => {
    settingsServer.once('error', reject);
    settingsServer.listen(Number(process.env.SETTINGS_PORT || 8090), process.env.BIND_HOST || '0.0.0.0', resolve);
  });
  console.log('[collector] Started; waiting for configured live rooms');
  await Promise.all([deliveryLoop(), reconcileLoop()]);
}
for (const signal of ['SIGINT','SIGTERM']) process.on(signal, () => { running = false; settingsServer?.close(); for (const s of sessions.values()) {s.active=false;s.ws?.close();} connection?.close().catch(()=>{}); });
if (require.main === module) main().catch(e => { console.error(e.message); process.exitCode=1; });
module.exports = { getRoom, liveUrl };
