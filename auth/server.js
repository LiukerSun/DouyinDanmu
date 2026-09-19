const http = require('node:http');
const fs = require('node:fs/promises');
const path = require('node:path');
const crypto = require('node:crypto');
const { promisify } = require('node:util');
const { portableRequest } = require('./portable-web');
const scrypt = promisify(crypto.scrypt);
const SESSION_MS = 12 * 60 * 60 * 1000;
const SCRYPT = { N: 32768, r: 8, p: 3, maxmem: 64 * 1024 * 1024 };
const publicUser = user => ({ username: user.username, displayName: user.displayName, role: '管理员' });
const digest = token => crypto.createHash('sha256').update(token).digest('hex');

async function createAuthServer({ dataDir, origins = ['http://localhost:3000', 'http://127.0.0.1:3000'], now = Date.now, sessionMs = SESSION_MS, wsHost = 'backend', wsPort = 8081, webRoot, backendPort, settingsPort } = {}) {
  await fs.mkdir(dataDir, { recursive: true, mode: 0o700 });
  const accountFile = path.join(dataDir, 'account.json');
  let user = null;
  try { user = JSON.parse(await fs.readFile(accountFile, 'utf8')); } catch (error) { if (error.code !== 'ENOENT') throw error; }
  const sessions = new Map(), attempts = new Map();
  const allowed = new Set(origins), hosts = new Set(origins.map(origin => new URL(origin).host));
  // Browsers share cookies across ports. Use the configured public origins so
  // independent workbenches cannot overwrite each other, including behind Nginx
  // where every auth service listens on 8091. All aliases of one server agree.
  const cookieName = 'studio_session_' + digest(JSON.stringify([...allowed].sort())).slice(0, 16);
  let settingUp = false, changingPassword = false, hashing = 0;
  const liveConnections = new Set();
  const revoke = key => {
    if (key) sessions.delete(key); else sessions.clear();
    for (const connection of liveConnections) if (!key || connection.key === key) { connection.client.destroy(); connection.upstream?.destroy(); }
  };
  const dummySalt = crypto.randomBytes(16).toString('hex');
  const dummyHash = await scrypt('unknown-account-dummy', dummySalt, 64, SCRYPT);
  const send = (res, status, body, headers = {}) => {
    res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff', ...headers });
    res.end(JSON.stringify(body));
  };
  const tokenFrom = req => (req.headers.cookie || '').split(';').map(part => part.trim()).find(part => part.startsWith(cookieName + '='))?.slice(cookieName.length + 1) || '';
  const sessionFrom = req => {
    const token = tokenFrom(req);
    if (!/^[a-f0-9]{64}$/.test(token)) return null;
    const key = digest(token), session = sessions.get(key);
    if (!session || session.expires <= now()) { revoke(key); return null; }
    return session;
  };
  const cookie = (token, req, maxAge) => `${cookieName}=${token}; Path=/; HttpOnly; SameSite=Strict; Max-Age=${maxAge}${allowed.has('https://' + req.headers.host) ? '; Secure' : ''}`;
  const startSession = (req, res) => {
    revoke(digest(tokenFrom(req)));
    const token = crypto.randomBytes(32).toString('hex');
    if (sessions.size >= 100) revoke(sessions.keys().next().value);
    sessions.set(digest(token), { expires: now() + sessionMs });
    send(res, 200, { user: publicUser(user) }, { 'Set-Cookie': cookie(token, req, Math.floor(sessionMs / 1000)) });
  };
  const clean = setInterval(() => {
    for (const [key, session] of sessions) if (session.expires <= now()) revoke(key);
    for (const [key, entry] of attempts) if (entry.until <= now()) attempts.delete(key);
  }, 1000);
  clean.unref();
  const server = http.createServer(async (req, res) => {
    try {
      if (req.url === '/health/live') return send(res, 200, { ok: true });
      if (!hosts.has(req.headers.host) || req.headers['sec-fetch-site'] === 'cross-site') return send(res, 403, { error: '请从工作台地址访问' });
      if (webRoot && !req.url.startsWith('/api/auth/')) {
        if (req.headers.origin && !allowed.has(req.headers.origin)) return send(res, 403, { error: '请求来源不受信任' });
        return await portableRequest(req, res, { webRoot, backendPort, settingsPort, authenticated: !!sessionFrom(req), trustedWrite: allowed.has(req.headers.origin) && new URL(req.headers.origin).host === req.headers.host });
      }
      if (req.url === '/verify') {
        // Only Nginx can access this port; it overwrites the original-method header.
        const method = req.headers['x-original-method'] || 'GET';
        if (req.headers.origin && !allowed.has(req.headers.origin)) return send(res, 403, { error: '请求来源不受信任' });
        if (!['GET', 'HEAD'].includes(method) && (!allowed.has(req.headers.origin) || new URL(req.headers.origin).host !== req.headers.host)) return send(res, 403, { error: '请求来源不受信任' });
        return send(res, sessionFrom(req) ? 200 : 401, {});
      }
      if (req.method === 'GET' && req.url === '/api/auth/session') return send(res, 200, { setupRequired: !user, user: sessionFrom(req) && user ? publicUser(user) : null });
      if (req.method !== 'POST' || !['/api/auth/login', '/api/auth/setup', '/api/auth/logout', '/api/auth/password'].includes(req.url)) return send(res, 404, { error: '接口不存在' });
      if (!allowed.has(req.headers.origin) || new URL(req.headers.origin).host !== req.headers.host) return send(res, 403, { error: '请从工作台提交请求' });
      if ((req.headers['content-type'] || '').split(';')[0] !== 'application/json') return send(res, 415, { error: '请求格式不正确' });
      if (req.url === '/api/auth/logout') {
        revoke(digest(tokenFrom(req)));
        return send(res, 200, { ok: true }, { 'Set-Cookie': cookie('', req, 0) });
      }
      if (req.url === '/api/auth/password' && !sessionFrom(req)) return send(res, 401, { error: '登录已过期，请重新登录' });
      if (req.url === '/api/auth/setup' && (user || settingUp)) return send(res, 409, { error: '管理员已创建，请使用账号密码登录' });
      const ip = req.headers['x-real-ip'] || req.socket.remoteAddress;
      const key = String(ip), entry = attempts.get(key);
      if (entry && entry.until > now() && entry.count >= 8) return send(res, 429, { error: '尝试次数过多，请 15 分钟后重试' }, { 'Retry-After': '900' });
      if (attempts.size >= 10000 && !entry) return send(res, 429, { error: '请求较多，请稍后重试' });
      if (hashing >= 2) return send(res, 429, { error: '正在处理登录请求，请稍后重试' }, { 'Retry-After': '2' });
      attempts.set(key, { count: entry && entry.until > now() ? entry.count + 1 : 1, until: entry && entry.until > now() ? entry.until : now() + 900000 });
      const chunks = []; let size = 0;
      for await (const chunk of req) { size += chunk.length; if (size > 4096) { send(res, 413, { error: '请求内容过长' }); req.resume(); return; } chunks.push(chunk); }
      let body;
      try { body = JSON.parse(Buffer.concat(chunks).toString('utf8')); } catch { return send(res, 400, { error: '请求格式不正确' }); }
      if (!body || typeof body !== 'object' || Array.isArray(body)) return send(res, 400, { error: '请求格式不正确' });
      const username = typeof body.username === 'string' ? body.username.trim() : '';
      const password = typeof body.password === 'string' ? body.password : '';
      if (!password || password.length > 128 || (req.url !== '/api/auth/password' && !/^[a-zA-Z0-9_.-]{3,32}$/.test(username))) return send(res, 400, { error: '请输入有效账号和密码' });
      // Reading the request body yielded to other requests. Recheck and reserve
      // together before the next await so streamed bodies cannot bypass the cap.
      if (hashing >= 2) return send(res, 429, { error: '正在处理登录请求，请稍后重试' }, { 'Retry-After': '2' });
      hashing++;
      try {
        if (req.url === '/api/auth/setup') {
          if (user || settingUp) return send(res, 409, { error: '管理员已创建，请登录' });
          if (password.length < 12) return send(res, 400, { error: '密码至少需要 12 个字符' });
          settingUp = true;
          try {
            const salt = crypto.randomBytes(16).toString('hex');
            const hash = (await scrypt(password, salt, 64, SCRYPT)).toString('hex');
            const account = { username, displayName: typeof body.displayName === 'string' && body.displayName.trim() ? body.displayName.trim().slice(0, 32) : username, salt, hash };
            // Exclusive creation prevents concurrent requests from replacing the owner.
            await fs.writeFile(accountFile, JSON.stringify(account), { flag: 'wx', mode: 0o600 });
            user = account;
            attempts.delete(key);
            return startSession(req, res);
          } finally { settingUp = false; }
        }
        const derived = await scrypt(password, user?.salt || dummySalt, 64, SCRYPT);
        const matches = crypto.timingSafeEqual(derived, user ? Buffer.from(user.hash, 'hex') : dummyHash);
        if (!user || !matches || (req.url === '/api/auth/login' && username !== user.username)) return send(res, 401, { error: '账号或密码错误' });
        if (req.url === '/api/auth/password') {
          if (changingPassword) return send(res, 409, { error: '密码正在更新，请稍后重试' });
          if (typeof body.newPassword !== 'string' || body.newPassword.length < 12 || body.newPassword.length > 128) return send(res, 400, { error: '新密码需要 12–128 个字符' });
          changingPassword = true;
          try {
          const salt = crypto.randomBytes(16).toString('hex');
          const updated = { ...user, salt, hash: (await scrypt(body.newPassword, salt, 64, SCRYPT)).toString('hex') };
          const temporary = accountFile + '.tmp';
          await fs.writeFile(temporary, JSON.stringify(updated), { mode: 0o600 });
          await fs.rename(temporary, accountFile); user = updated; revoke();
          } finally { changingPassword = false; }
        }
        attempts.delete(key);
        return startSession(req, res);
      } finally { hashing--; }
    } catch (error) {
      if (!res.headersSent && !res.destroyed) send(res, 500, { error: '账号服务暂时不可用，请稍后重试' });
      // Do not log request bodies, credentials, or session tokens.
      console.error('Auth request failed:', error.code || error.name);
    }
  });
  server.requestTimeout = 10000; server.headersTimeout = 10000;
  server.on('upgrade', (req, client, head) => {
    if (!/^\/ws(?:\?|$)/.test(req.url) || !hosts.has(req.headers.host) || (req.headers.origin && (!allowed.has(req.headers.origin) || new URL(req.headers.origin).host !== req.headers.host)) || !sessionFrom(req)) {
      client.end('HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n'); return;
    }
    const connection = { key: digest(tokenFrom(req)), client, upstream: null };
    liveConnections.add(connection);
    const proxy = http.request({ host: wsHost, port: wsPort, path: req.url, headers: { ...req.headers, host: wsHost + ':' + wsPort, cookie: '' } });
    client.on('error', () => client.destroy());
    client.on('close', () => { liveConnections.delete(connection); connection.upstream?.destroy(); proxy.destroy(); });
    proxy.on('upgrade', (response, upstream, upstreamHead) => {
      connection.upstream = upstream;
      if (!sessions.has(connection.key) || client.destroyed) { upstream.destroy(); client.destroy(); return; }
      client.write('HTTP/1.1 101 Switching Protocols\r\n' + Object.entries(response.headers).map(([name, value]) => `${name}: ${value}`).join('\r\n') + '\r\n\r\n');
      if (head.length) upstream.write(head);
      if (upstreamHead.length) client.write(upstreamHead);
      upstream.on('error', () => { upstream.destroy(); client.destroy(); });
      upstream.on('close', () => client.destroy());
      client.pipe(upstream).pipe(client);
    });
    proxy.on('response', response => { response.resume(); client.end(`HTTP/1.1 ${response.statusCode} Upstream rejected\r\nConnection: close\r\n\r\n`); });
    proxy.on('error', () => client.destroy());
    proxy.setTimeout(10000, () => { if (!connection.upstream) { proxy.destroy(); client.destroy(); } });
    proxy.end();
  });
  server.on('close', () => clearInterval(clean));
  return server;
}
module.exports = { createAuthServer };
if (require.main === module) createAuthServer({ dataDir: process.env.AUTH_DATA_DIR || '/data', origins: (process.env.AUTH_ORIGINS || 'http://localhost:3000,http://127.0.0.1:3000').split(',') }).then(server => server.listen(8091, '0.0.0.0')).catch(() => { console.error('Cannot initialize account storage'); process.exit(1); });
