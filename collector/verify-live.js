// Read-only check against a configured real room; never generates live messages.
const assert = require('node:assert/strict');
const WebSocket = require('ws');
const API = process.env.BACKEND_URL || 'http://backend:8080';
const liveId = process.argv[2];
assert.match(liveId || '', /^\d+$/);
(async () => {
  const rooms = await (await fetch(API + '/api/rooms')).json();
  const room = rooms.find(r => r.live_id === liveId);
  assert.equal(room?.source, 'douyin');
  assert.equal(room.enabled, true);
  assert.ok(room.metadata && !room.metadata_stale, 'No fresh room metadata yet');
  assert.ok(['live','offline'].includes(room.metadata.live_status), 'Live status has not been confirmed');
  assert.equal(room.status, room.metadata.live_status === 'live' ? 'collecting' : 'waiting_live');
  const snapshot = await (await fetch(`${API}/api/rooms/${liveId}/snapshot`)).json();
  assert.ok(snapshot.events.length, 'No persisted real upstream events yet');
  assert.ok(snapshot.events.every(e => e.source === 'douyin' && e.live_id === liveId));
  const first = snapshot.events[0];
  const wsUrl = new URL(API); wsUrl.protocol = 'ws:'; wsUrl.port = '8081'; wsUrl.pathname = '/ws';
  await new Promise((resolve, reject) => {
    const ws = new WebSocket(wsUrl);
    const timer = setTimeout(() => { ws.terminate(); reject(new Error('C++ WebSocket replay timeout')); }, 10000);
    ws.on('open', () => ws.send(JSON.stringify({action:'subscribe', live_id:liveId, after_seq:String(BigInt(first.seq) - 1n)})));
    ws.on('error', err => {clearTimeout(timer); reject(err)});
    ws.on('message', raw => {
      try {
        const batch = JSON.parse(raw);
        if (batch.type !== 'event_batch') return;
        const replay = batch.events.find(e => e.event_id === first.event_id);
        assert.deepEqual(replay, first);
        clearTimeout(timer); ws.close(); resolve();
      } catch (e) { clearTimeout(timer); ws.terminate(); reject(e); }
    });
  });
  console.log(JSON.stringify({passed:true,live_id:liveId,source:room.source,status:room.status,live_status:room.metadata.live_status,anchor:room.metadata.anchor.nickname,persisted_events:snapshot.events.length,types:[...new Set(snapshot.events.map(e=>e.type))],stats:snapshot.stats,cpp_websocket_replay:true}));
})().catch(e => {console.error(e.message);process.exitCode=1});
