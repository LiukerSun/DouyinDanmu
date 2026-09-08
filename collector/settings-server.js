const http = require('node:http');

const ENDPOINT = /^\/api\/settings\/rooms\/(\d{1,30})\/douyin-cookie$/;
const MAX_BODY_BYTES = 48 * 1024;

function createSettingsServer({ roomCookies, isKnownRoom, onChange, allowedOrigins = ['http://localhost:3000', 'http://127.0.0.1:3000', 'http://[::1]:3000'] }) {
  const origins = new Set(allowedOrigins);
  const hosts = new Set(allowedOrigins.map(origin => new URL(origin).host));
  const send = (res, status, value) => {
    res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store', Pragma: 'no-cache', 'X-Content-Type-Options': 'nosniff' });
    res.end(JSON.stringify(value));
  };
  const server = http.createServer(async (req, res) => {
    // The reverse proxy preserves the browser's Host. This prevents a rebinding
    // hostname or another website from silently changing the local credential.
    if (!hosts.has(req.headers.host) || req.headers['sec-fetch-site'] === 'cross-site') return send(res, 403, { error: '仅允许本地工作台访问配置' });
    const liveId = req.url.match(ENDPOINT)?.[1];
    if (!liveId) return send(res, 404, { error: '配置接口不存在' });
    if (!['GET', 'PUT', 'DELETE'].includes(req.method)) return send(res, 405, { error: '不支持此操作' });
    try {
      if (!await isKnownRoom(liveId)) return send(res, 404, { error: '直播间不存在，请先添加直播间' });
    } catch { return send(res, 503, { error: '暂时无法确认直播间，请稍后重试' }); }
    const cookieSource = roomCookies.forRoom(liveId);
    if (req.method === 'GET') return send(res, 200, { live_id: liveId, ...cookieSource.status() });
    const origin = req.headers.origin;
    if (!origins.has(origin) || new URL(origin).host !== req.headers.host || req.headers['x-settings-request'] !== '1') return send(res, 403, { error: '请从本地工作台提交配置' });
    if ((req.headers['content-type'] || '').split(';')[0].trim().toLowerCase() !== 'application/json') return send(res, 415, { error: '请提交 JSON 格式配置' });
    let body = '', size = 0;
    try {
      for await (const chunk of req) {
        size += chunk.length;
        if (size > MAX_BODY_BYTES) { send(res, 413, { error: 'Cookie 内容过长' }); req.resume(); return; }
        body += chunk.toString('utf8');
      }
    } catch { if (!res.destroyed) send(res, 400, { error: '配置请求读取失败' }); return; }
    let value;
    try { value = body ? JSON.parse(body) : {}; }
    catch { return send(res, 400, { error: '配置请求不是有效 JSON' }); }
    if (!value || Array.isArray(value) || typeof value !== 'object') return send(res, 400, { error: '配置格式不正确' });
    if (req.method === 'PUT' && (typeof value.cookie !== 'string' || !value.cookie.trim() || /^cookie\s*:\s*$/i.test(value.cookie.trim()))) return send(res, 400, { error: '请先粘贴完整 Cookie；切回游客请使用清除按钮' });
    let status;
    try { status = cookieSource.write(req.method === 'DELETE' ? '' : value.cookie); }
    catch (error) { return send(res, /格式/.test(error.message) ? 400 : 500, { error: error.message }); }
    // This callback only restarts local sessions; no upstream request is awaited.
    try {
      const reconnectingRooms = onChange(liveId);
      return send(res, 200, { live_id: liveId, ...status, saved: true, reconnecting_rooms: reconnectingRooms });
    } catch {
      return send(res, 200, { live_id: liveId, ...status, saved: true, reconnecting_rooms: 0, reconnect_pending: true });
    }
  });
  server.requestTimeout = 10000;
  server.headersTimeout = 10000;
  return server;
}

module.exports = { createSettingsServer };
