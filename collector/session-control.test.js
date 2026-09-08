const test = require('node:test');
const assert = require('node:assert/strict');
const { reconnectRoom } = require('./session-control');

test('changing one room interrupts only its old session and leaves other rooms and stopped targets alone', () => {
  const closed = [], notices = [];
  const session = id => ({ live_id: id, active: true, source: 'douyin', ws: { terminate: () => closed.push(id) } });
  const first = session('111'), second = session('222');
  const sessions = new Map([['111', first], ['222', second]]);
  assert.equal(reconnectRoom(sessions, '111', current => notices.push(current.live_id)), 1);
  assert.equal(first.active, false, 'in-flight callbacks must stop accepting data');
  assert.equal(second.active, true);
  assert.equal(sessions.get('222'), second, 'the other connection retains its exact session');
  assert.equal(sessions.has('111'), false);
  assert.deepEqual(closed, ['111']);
  assert.deepEqual(notices, ['111']);
  assert.equal(reconnectRoom(sessions, '333', () => assert.fail('stopped room restarted')), 0);
  assert.equal(reconnectRoom(sessions, '111', () => assert.fail('repeat restart')), 0);
  assert.equal(sessions.size, 1);
});
