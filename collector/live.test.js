const assert = require('node:assert/strict');
const { liveUrl } = require('./index');
const url = new URL(liveUrl('9000000001'));
assert.equal(url.searchParams.get('room_id'), '9000000001');
assert.ok(url.searchParams.get('signature')?.length > 10);
console.log('PASS: collector module generates a signed live WebSocket URL');
