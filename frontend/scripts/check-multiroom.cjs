const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const ts = require('typescript');
function load(relative) {
  const filename = path.resolve(__dirname, '../src/pipeline', relative);
  const compiled = ts.transpileModule(fs.readFileSync(filename, 'utf8'), { compilerOptions: { target: ts.ScriptTarget.ES2020, module: ts.ModuleKind.CommonJS } }).outputText;
  const module = { exports: {} };
  new Function('require', 'module', 'exports', compiled)(name => load(name + '.ts'), module, module.exports);
  return module.exports;
}
const { RoomStreams } = load('room-streams.ts');
const { parseRoomInput, isLive, needsAttention } = load('monitor.ts');
const { completeRoomOrder, reorderVisibleRooms } = load('room-order.ts');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
const event = (id, seq, extra = {}) => ({ live_id: id, seq, event_id: 'event-' + seq, source: 'douyin', type: 'chat', user_name: 'test', user_id: '9007199254740993', content: '测试消息', timestamp: 1, gift_count: 0, persisted_at_ms: 2, received_at_ms: 1, ...extra });
class FakeSocket {
  readyState = 0; sent = []; onopen; onmessage; onclose; onerror;
  send(raw) { assert.equal(this.readyState, 1); this.sent.push(JSON.parse(raw)); }
  open() { this.readyState = 1; this.onopen?.(); for (const command of this.sent.filter(item => item.action === 'subscribe')) this.receive({ type: 'subscribed', live_id: command.live_id }); }
  receive(data) { this.onmessage?.({ data: JSON.stringify(data) }); }
  close() { this.readyState = 3; this.onclose?.(); }
}
function setup(snapshot = async id => ({ events: [event(id, '1')], through_seq: '1' })) {
  const sockets = []; let latest = {};
  const manager = new RoomStreams({ snapshot, socket: () => { const socket = new FakeSocket(); sockets.push(socket); return socket; }, change: value => { latest = value; } });
  return { manager, sockets, state: () => latest };
}
async function main() {
  assert.deepEqual(completeRoomOrder(['b', 'gone', 'b'], ['a', 'b', 'c']), ['b', 'a', 'c']);
  assert.deepEqual(reorderVisibleRooms(['a', 'b', 'c'], ['a', 'b', 'c'], 'a', 'c'), ['b', 'c', 'a']);
  assert.deepEqual(reorderVisibleRooms(['a', 'hidden', 'b', 'c'], ['a', 'b', 'c'], 'c', 'a'), ['c', 'hidden', 'a', 'b']);
  assert.deepEqual(reorderVisibleRooms(['a', 'b', 'c'], ['c', 'a', 'b'], 'b', 'c'), ['b', 'c', 'a']);
  assert.deepEqual(reorderVisibleRooms(['a', 'b'], ['a', 'b'], 'missing', 'a'), ['a', 'b']);
  console.log('PASS: manual order survives new/deleted rooms and reordering preserves filtered-out slots');
  assert.deepEqual(parseRoomInput('123\nhttps://live.douyin.com/456?foo=1，123;live.douyin.com/789/'), { ids: ['123', '456', '789'], invalid: [] });
  assert.equal(parseRoomInput('https://evil.example/live.douyin.com/123').invalid.length, 1);
  assert.equal(parseRoomInput('https://live.douyin.com.evil.example/123').invalid.length, 1);
  assert.equal(parseRoomInput('123oops').invalid.length, 1);
  const room = { enabled: true, status: 'collecting', metadata_stale: false, metadata: { checked_at_ms: Date.now() - 181000, live_status: 'live' } };
  assert.equal(isLive(room), false); assert.equal(needsAttention({ ...room, status: 'failed' }), true); assert.equal(needsAttention({ ...room, enabled: false, status: 'failed' }), false);
  console.log('PASS: batch room parsing, deduplication, strict host validation and stale live status');

  const first = setup();
  try {
    first.manager.sync(['a', 'b']); await delay(20);
    assert.equal(first.sockets.length, 1); first.sockets[0].open(); await delay(120);
    assert.equal(first.state().a.connected, true); assert.equal(first.state().b.connected, true);
    const frozenA = first.state().a.messages;
    first.sockets[0].receive({ type: 'event_batch', live_id: 'a', events: [event('a', '9007199254740993'), event('b', '9007199254740994')], through_seq: '9007199254740993' });
    await delay(120);
    assert.equal(first.state().a.messages.length, 2); assert.equal(first.state().b.messages.length, 1); assert.equal(frozenA.length, 1);
    first.manager.sync(['b', 'a']); await delay(20); assert.equal(first.sockets.length, 1);
    assert.equal(first.sockets[0].sent.length, 2, 'unchanged room lists must not duplicate subscriptions');
    first.sockets[0].close(); await delay(550); assert.equal(first.sockets.length, 2);
    first.sockets[1].open();
    assert.deepEqual(first.sockets[1].sent.map(item => [item.live_id, item.after_seq]), [['a', '9007199254740993'], ['b', '1']]);
    first.manager.sync(['b']); first.sockets[1].receive({ type: 'event_batch', live_id: 'a', events: [event('a', '9007199254740995')], through_seq: '9007199254740995' }); await delay(120);
    assert.equal(first.state().a, undefined); assert.ok(first.state().b); assert.equal(first.sockets[1].sent.at(-1).action, 'unsubscribe');
    console.log('PASS: room isolation, immutable pause snapshots, stable subscriptions, independent 64-bit reconnect cursors and removed-room late events');
  } finally { first.manager.dispose(); }

  const states = setup(async id => ({ events: [event(id, '1'), event(id, '2', { type: 'gift_notice', method: 'WebcastGiftSortMessage' })], state_events: [event(id, '0', { type: 'online_count', online_count: 10 }), event(id, '2', { type: 'online_count', online_count: 11, method: 'WebcastRoomUserSeqMessage' }), event(id, '2', { type: 'stream' })], through_seq: '2' }));
  try {
    states.manager.sync(['a', 'b']); await delay(20); states.sockets[0].open(); await delay(120);
    assert.equal(states.state().a.roomState.length, 1, 'Legacy state aliases must merge with their current method');
    assert.equal(states.state().a.roomState[0].seq, '2');
    const updates = Array.from({ length: 600 }, (_, i) => event('a', String(i + 3), { type: 'online_count', online_count: 0, method: 'WebcastRoomUserSeqMessage' }));
    states.sockets[0].receive({ type: 'event_batch', live_id: 'a', events: [], state_events: updates, through_seq: '602' });
    await delay(120);
    assert.equal(states.state().a.messages.length, 1); assert.equal(states.state().a.messages[0].seq, '1');
    assert.equal(states.state().a.roomState.length, 1); assert.equal(states.state().a.roomState[0].seq, '602');
    assert.equal(states.state().a.roomState[0].online_count, 0); assert.equal(states.state().a.roomState[0].content, undefined);
    assert.equal(states.state().b.roomState[0].seq, '2');
    states.sockets[0].receive({ type: 'event_batch', live_id: 'a', events: [updates[0]], through_seq: '3' });
    await delay(120); assert.equal(states.state().a.roomState[0].seq, '602');
    states.sockets[0].receive({ type: 'event_batch', live_id: 'a', events: [], state_events: [], through_seq: '999' });
    await delay(120); assert.equal(states.state().a.messages.length, 1); assert.equal(states.state().a.cursor, '999');
    states.sockets[0].close(); await delay(550); states.sockets[1].open();
    assert.equal(states.sockets[1].sent.find(item => item.live_id === 'a').after_seq, '999');
    console.log('PASS: state/configuration cannot evict behaviors; compact zero metrics, room isolation and hidden-only reconnect cursors survive replay');
  } finally { states.manager.dispose(); }
  const { messageKind, messageContent } = load('messages.ts');
  assert.equal(messageKind(event('a', '1', { type: 'social', action: 1 })), 'follow');
  assert.equal(messageKind(event('a', '1', { type: 'social', action: 3 })), 'social');
  assert.equal(messageContent(event('a', '1', { type: 'social', action: 3, social_action: 'share' })), '分享了直播间');
  assert.equal(messageKind(event('a', '1', { type: 'social', action: 1, social_action: 'unknown' })), 'social');
  for (const type of ['chat', 'emoji', 'episode_chat', 'audio_chat', 'screen_chat']) assert.equal(messageKind(event('a', '1', { type })), 'chat');
  for (const type of ['system', 'room_notice', 'notice']) assert.equal(messageKind(event('a', '1', { type })), 'notice');
  assert.equal(messageContent(event('a', '1', { type: 'social', action: 987 })), '发生了其他互动');
  console.log('PASS: historical follow labels and explicit social evidence agree without guessing unknown codes');

  const many = setup();
  try {
    many.manager.sync(Array.from({ length: 65 }, (_, i) => String(i))); await delay(30);
    assert.equal(many.sockets.length, 3); many.sockets.forEach(socket => socket.open());
    assert.deepEqual(many.sockets.map(socket => socket.sent.length), [32, 32, 1]);
    console.log('PASS: 65 room subscriptions are partitioned within the server 32-room limit');
  } finally { many.manager.dispose(); }

  let resolveLate;
  const delayed = setup(() => new Promise(resolve => { resolveLate = resolve; }));
  delayed.manager.sync(['late']); delayed.manager.dispose(); resolveLate({ events: [], through_seq: '0' }); await delay(120);
  assert.equal(delayed.sockets.length, 0); assert.deepEqual(delayed.state(), {});
  console.log('PASS: unmount cancels pending snapshots and publishes no late state');

  const partial = setup(async id => { if (id === 'bad') throw new Error('offline'); return { events: [], through_seq: '0' }; });
  try {
    partial.manager.sync(['good', 'bad']); await delay(20); partial.sockets[0].open(); await delay(120);
    assert.equal(partial.state().good.connected, true); assert.equal(partial.state().bad.connected, false); assert.ok(partial.state().bad.error);
    console.log('PASS: a failed snapshot does not prevent other rooms from receiving messages');
  } finally { partial.manager.dispose(); }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
