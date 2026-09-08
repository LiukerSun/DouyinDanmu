const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { createRoomCookieStore } = require('./auth-cookie');
const { createSettingsServer } = require('./settings-server');

async function fixture(t) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'douyin-cookie-settings-'));
  const options = { filePath: path.join(directory, 'legacy.txt'), storePath: path.join(directory, 'config', '111.cookie') };
  fs.writeFileSync(options.filePath, 'ttwid=legacy-fixture; sessionid=legacy-fixture');
  const storeOptions = { directory: path.join(directory, 'config') };
  const roomCookies = createRoomCookieStore(storeOptions), source = roomCookies.forRoom('111'), changes = [];
  source.write('ttwid=initial-fixture; sessionid=initial-fixture');
  const server = createSettingsServer({ roomCookies, isKnownRoom: async id => ['111', '222'].includes(id),
    onChange: id => { changes.push({ id, cookie: roomCookies.forRoom(id).read() }); return 1; } });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(async () => {
    server.closeAllConnections();
    await new Promise(resolve => server.close(resolve));
    fs.rmSync(directory, { recursive: true, force: true });
  });
  const request = async (method = 'GET', payload, headers = {}, endpoint = '/api/settings/rooms/111/douyin-cookie') => {
    const body = method === 'GET' ? undefined : typeof payload === 'string' ? payload : JSON.stringify(payload || {});
    return new Promise((resolve, reject) => {
      const req = http.request({ hostname: '127.0.0.1', port: server.address().port, path: endpoint,
        method, headers: { Host: 'localhost:3000', Origin: 'http://localhost:3000', 'Content-Type': 'application/json', 'X-Settings-Request': '1', ...(body === undefined ? {} : { 'Content-Length': Buffer.byteLength(body) }), ...headers },
      }, response => {
        let text = '';
        response.on('data', chunk => { text += chunk; });
        response.on('end', () => {
          try { resolve({ status: response.statusCode, headers: new Headers(response.headers), text, data: JSON.parse(text) }); }
          catch (error) { reject(error); }
        });
      });
      req.on('error', reject);
      req.end(body);
    });
  };
  return { source, changes, options, request, roomCookies, storeOptions };
}

test('web save persists atomically, hides values and reconnects only after storage', async t => {
  const { source, changes, options, request, storeOptions } = await fixture(t);
  const initial = await request();
  assert.equal(initial.data.auth_status, 'configured');
  assert.equal(initial.headers.get('cache-control'), 'no-store');
  assert.ok(!initial.text.includes('legacy-fixture'));
  const cookie = 'ttwid=web-fixture; sessionid=private-test-token==';
  const saved = await request('PUT', { cookie: 'Cookie: ' + cookie });
  assert.equal(saved.status, 200);
  assert.equal(saved.data.saved, true);
  assert.equal(saved.data.reconnecting_rooms, 1);
  assert.equal(saved.data.live_id, '111');
  assert.equal(source.read(), cookie);
  assert.deepEqual(changes, [{ id: '111', cookie }]);
  assert.equal(createRoomCookieStore(storeOptions).forRoom('111').read(), cookie, 'new process reads the saved setting');
  for (const response of [saved, await request()]) assert.ok(!response.text.includes('private-test-token') && !response.text.includes('sessionid'));
  if (process.platform !== 'win32') assert.equal(fs.statSync(options.storePath).mode & 0o777, 0o600);
  assert.deepEqual(fs.readdirSync(path.dirname(options.storePath)), ['111.cookie'], 'no intermediate file remains');
});

test('clear persists anonymous override instead of reactivating a legacy Cookie', async t => {
  const { source, changes, options, request, storeOptions } = await fixture(t);
  const result = await request('DELETE');
  assert.equal(result.status, 200);
  assert.equal(result.data.auth_status, 'anonymous');
  assert.equal(source.read(), '');
  assert.equal(createRoomCookieStore(storeOptions).forRoom('111').read(), '');
  assert.deepEqual(changes, [{ id: '111', cookie: '' }]);
  assert.ok(fs.readFileSync(options.filePath, 'utf8').includes('legacy-fixture'), 'legacy file remains untouched');
});

test('foreign origins, cross-site requests and rebinding hosts cannot change credentials', async t => {
  const { source, changes, request } = await fixture(t);
  const initial = source.read();
  for (const headers of [
    { Origin: 'https://untrusted.invalid' }, { Origin: '' },
    { Host: 'untrusted.invalid:3000' }, { 'X-Settings-Request': '' },
    { Origin: 'http://127.0.0.1:3000' }, { 'Sec-Fetch-Site': 'cross-site' },
  ]) assert.equal((await request('PUT', { cookie: 'sessionid=rejected-fixture' }, headers)).status, 403);
  assert.equal((await request('GET', undefined, { Host: 'untrusted.invalid:3000' })).status, 403);
  assert.equal((await request('OPTIONS')).status, 405);
  assert.equal(changes.length, 0);
  assert.equal(source.read(), initial);
});

test('invalid, injected, empty and oversized credentials do not replace the saved account', async t => {
  const { source, changes, request } = await fixture(t);
  const initial = source.read();
  for (const cookie of ['', 'Cookie:', ';', 'not-a-cookie', 'sessionid=private-fixture\r\nX-Injected: secret', 'sessionid=' + 'x'.repeat(33000)]) {
    const result = await request('PUT', { cookie });
    assert.equal(result.status, 400);
    assert.ok(!result.text.includes('private-fixture'));
  }
  for (const payload of ['{broken-json', 'null', '[]', '{"cookie":123}']) assert.equal((await request('PUT', payload)).status, 400);
  assert.equal((await request('PUT', { cookie: 'sessionid=test' }, { 'Content-Type': 'text/plain' })).status, 415);
  assert.equal((await request('PUT', { cookie: 'x'.repeat(60000) })).status, 413);
  assert.equal(source.read(), initial);
  assert.equal(changes.length, 0);
});

test('storage errors are sanitized and do not request a reconnect', async t => {
  const { source, options, changes, request } = await fixture(t);
  fs.unlinkSync(options.storePath);
  fs.mkdirSync(options.storePath, { recursive: true });
  const result = await request('PUT', { cookie: 'sessionid=private-storage-fixture' });
  assert.equal(result.status, 500);
  assert.equal(result.data.error, '无法保存 Cookie，请检查采集服务存储');
  assert.ok(!result.text.includes('private-storage-fixture') && !result.text.includes(options.storePath));
  assert.equal(changes.length, 0);
  assert.equal(source.status().auth_status, 'configuration_error');
});

test('two rooms persist separate accounts; clearing one never changes the other or restores global credentials', async t => {
  const { roomCookies, changes, request, storeOptions } = await fixture(t);
  const endpoint = '/api/settings/rooms/222/douyin-cookie';
  assert.equal((await request('GET', undefined, {}, endpoint)).data.auth_status, 'anonymous');
  const cookies = { '111': 'sessionid=account-a; ttwid=visitor-a', '222': 'sessionid=account-b; ttwid=visitor-b' };
  await Promise.all(Object.entries(cookies).map(async ([id, cookie]) => {
    const result = await request('PUT', { cookie }, {}, `/api/settings/rooms/${id}/douyin-cookie`);
    assert.equal(result.status, 200);
    assert.equal(result.data.live_id, id);
  }));
  const restarted = createRoomCookieStore(storeOptions);
  for (const [id, cookie] of Object.entries(cookies)) assert.equal(restarted.forRoom(id).read(), cookie);
  assert.equal((await request('DELETE')).data.auth_status, 'anonymous');
  assert.equal(roomCookies.forRoom('111').read(), '');
  assert.equal(restarted.forRoom('222').read(), cookies['222']);
  assert.equal((await request('GET', undefined, {}, endpoint)).data.auth_status, 'configured');
  assert.equal(roomCookies.forRoom('333').read(), '', 'a new room must not inherit any account');
  assert.deepEqual(changes.map(change => change.id).sort(), ['111', '111', '222']);
});

test('global, unknown, malformed and traversal room paths cannot read or write configuration', async t => {
  const { roomCookies, changes, request } = await fixture(t);
  for (const endpoint of ['/api/settings/douyin-cookie', '/api/settings/rooms/333/douyin-cookie',
    '/api/settings/rooms/../douyin-cookie', '/api/settings/rooms/%31%31%31/douyin-cookie',
    '/api/settings/rooms/' + '1'.repeat(31) + '/douyin-cookie']) {
    for (const method of ['GET', 'PUT', 'DELETE']) assert.equal((await request(method, { cookie: 'sessionid=rejected' }, {}, endpoint)).status, 404);
  }
  for (const id of ['../111', '111/222', '', 111]) assert.throws(() => roomCookies.forRoom(id), /数字字符串/);
  assert.equal(changes.length, 0);
});
