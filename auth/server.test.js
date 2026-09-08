const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const crypto = require('node:crypto');
const { createAuthServer } = require('./server');

test('persistent account, real password verification, access control and session lifecycle', async t => {
  const dataDir = await fs.mkdtemp(path.join(os.tmpdir(), 'studio-auth-test-'));
  let clock = Date.now();
  let server = await createAuthServer({ dataDir, now: () => clock, sessionMs: 60000 });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(async () => { await new Promise(resolve => server.close(resolve)); await fs.rm(dataDir, { recursive: true, force: true }); });
  let base = `http://127.0.0.1:${server.address().port}`;
  const request = async (url, body, cookie = '', headers = {}) => {
    return new Promise((resolve, reject) => {
      const req = http.request(base + url, { method: body === undefined ? 'GET' : 'POST', headers: { Host: 'localhost:3000', Origin: 'http://localhost:3000', 'Content-Type': 'application/json', Cookie: cookie, ...headers } }, response => {
        let data = ''; response.on('data', chunk => { data += chunk; }); response.on('end', () => resolve({ status: response.statusCode, data: JSON.parse(data), cookie: response.headers['set-cookie']?.[0] }));
      });
      req.on('error', reject); req.end(body === undefined ? undefined : JSON.stringify(body));
    });
  };
  assert.equal((await request('/api/auth/session')).data.setupRequired, true);
  assert.equal((await request('/verify')).status, 401);
  assert.equal((await request('/api/auth/setup', { username: 'admin', password: '123' })).status, 400);
  assert.equal((await request('/api/auth/setup', { username: 'admin', password: 'long-test-password' }, '', { Origin: 'https://evil.example' })).status, 403);
  const setup = await request('/api/auth/setup', { username: 'admin', displayName: '测试管理员', password: 'long-test-password' });
  assert.equal(setup.status, 200);
  assert.match(setup.cookie, /HttpOnly; SameSite=Strict/);
  assert.equal(setup.data.user.displayName, '测试管理员');
  const firstCookie = setup.cookie.split(';')[0];
  assert.equal((await request('/api/auth/setup', { username: 'other', password: 'long-test-password' })).status, 409);
  assert.equal((await request('/verify', undefined, firstCookie)).status, 200);
  assert.equal((await request('/verify', undefined, firstCookie, { 'X-Original-Method': 'DELETE', Origin: 'https://evil.example' })).status, 403);
  assert.equal((await request('/api/auth/login', { username: 'admin', password: 'wrong-password' })).status, 401);
  assert.equal((await request('/api/auth/login', { username: 'unknown', password: 'long-test-password' })).status, 401);
  assert.equal((await request('/api/auth/password', { password: 'long-test-password', newPassword: 'new-long-test-password' })).status, 401);
  const login = await request('/api/auth/login', { username: 'admin', password: 'long-test-password' });
  assert.equal(login.status, 200);
  const secondCookie = login.cookie.split(';')[0];
  assert.notEqual(firstCookie, secondCookie);
  const changed = await request('/api/auth/password', { password: 'long-test-password', newPassword: 'new-long-test-password' }, firstCookie);
  assert.equal(changed.status, 200);
  assert.equal((await request('/verify', undefined, secondCookie)).status, 401);
  assert.equal((await request('/verify', undefined, firstCookie)).status, 401);
  const changedCookie = changed.cookie.split(';')[0];
  assert.equal((await request('/verify', undefined, changedCookie)).status, 200);
  assert.equal((await request('/api/auth/login', { username: 'admin', password: 'long-test-password' })).status, 401);
  await request('/api/auth/logout', {}, changedCookie);
  assert.equal((await request('/verify', undefined, changedCookie)).status, 401);
  const again = await request('/api/auth/login', { username: 'admin', password: 'new-long-test-password' });
  clock += 60001;
  assert.equal((await request('/verify', undefined, again.cookie.split(';')[0])).status, 401);
  const disk = await fs.readFile(path.join(dataDir, 'account.json'), 'utf8');
  assert.ok(!disk.includes('password'));
  assert.match(JSON.parse(disk).hash, /^[a-f0-9]{128}$/);
  await new Promise(resolve => server.close(resolve));
  server = await createAuthServer({ dataDir, now: () => clock });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  base = `http://127.0.0.1:${server.address().port}`;
  assert.equal((await request('/api/auth/session')).data.setupRequired, false);
  assert.equal((await request('/api/auth/login', { username: 'admin', password: 'new-long-test-password' })).status, 200);
  for (let index = 0; index < 8; index++) assert.equal((await request('/api/auth/login', { username: 'admin', password: 'incorrect-password' })).status, 401);
  assert.equal((await request('/api/auth/login', { username: 'admin', password: 'new-long-test-password' })).status, 429);
  clock += 900001;
  assert.equal((await request('/api/auth/login', { username: 'admin', password: 'new-long-test-password' })).status, 200);
});

test('authenticated WebSocket is closed when its session is revoked', async t => {
  const dataDir = await fs.mkdtemp(path.join(os.tmpdir(), 'studio-stream-auth-test-'));
  const upstream = http.createServer();
  upstream.on('upgrade', (req, socket) => {
    const accept = crypto.createHash('sha1').update(req.headers['sec-websocket-key'] + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
    socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`);
    socket.on('error', () => socket.destroy()); socket.on('end', () => socket.destroy());
  });
  await new Promise(resolve => upstream.listen(0, '127.0.0.1', resolve));
  const server = await createAuthServer({ dataDir, wsHost: '127.0.0.1', wsPort: upstream.address().port });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(async () => { await new Promise(resolve => server.close(resolve)); await new Promise(resolve => upstream.close(resolve)); await fs.rm(dataDir, { recursive: true, force: true }); });
  const options = { host: '127.0.0.1', port: server.address().port, headers: { Host: 'localhost:3000', Origin: 'http://localhost:3000', 'Content-Type': 'application/json' } };
  const post = (url, body, cookie = '') => new Promise((resolve, reject) => {
    const req = http.request({ ...options, path: url, method: 'POST', headers: { ...options.headers, Cookie: cookie } }, res => { res.resume(); res.on('end', () => resolve(res)); }); req.on('error', reject); req.end(JSON.stringify(body));
  });
  const setup = await post('/api/auth/setup', { username: 'tester', password: 'isolated-stream-test' });
  assert.equal(setup.statusCode, 200);
  const cookie = setup.headers['set-cookie'][0].split(';')[0];
  const socket = await new Promise((resolve, reject) => {
    const req = http.request({ ...options, path: '/ws', headers: { ...options.headers, Cookie: cookie, Upgrade: 'websocket', Connection: 'Upgrade', 'Sec-WebSocket-Version': '13', 'Sec-WebSocket-Key': crypto.randomBytes(16).toString('base64') } });
    req.on('upgrade', (res, socket) => resolve(socket)); req.on('response', res => reject(new Error('Rejected: ' + res.statusCode))); req.on('error', reject); req.end();
  });
  const closed = new Promise((resolve, reject) => { const timer = setTimeout(() => { socket.destroy(); reject(new Error('Revoked stream stayed open')); }, 2000); socket.on('close', () => { clearTimeout(timer); resolve(); }); socket.on('end', () => socket.destroy()); });
  await post('/api/auth/logout', {}, cookie);
  await closed;
});
