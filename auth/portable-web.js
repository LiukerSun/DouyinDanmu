const http = require('node:http');
const fs = require('node:fs/promises');
const path = require('node:path');
const mime = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.json': 'application/json', '.svg': 'image/svg+xml', '.png': 'image/png', '.ico': 'image/x-icon', '.woff2': 'font/woff2' };

async function portableRequest(req, res, { webRoot, backendPort, settingsPort, authenticated, trustedWrite }) {
  const fail = (status, error) => { res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' }); res.end(JSON.stringify({ error })); };
  const url = new URL(req.url, 'http://localhost');
  if (url.pathname === '/verify' || url.pathname.startsWith('/internal/') || url.pathname === '/_auth') return fail(404, '接口不存在');
  if (url.pathname.startsWith('/api/')) {
    if (!authenticated) return fail(401, '登录已过期，请重新登录');
    if (!['GET', 'HEAD'].includes(req.method) && !trustedWrite) return fail(403, '请从工作台提交请求');
    const headers = { ...req.headers, cookie: '', 'x-internal-token': '', 'x-original-method': '', 'x-real-ip': req.socket.remoteAddress };
    const upstream = http.request({ host: '127.0.0.1', port: url.pathname.startsWith('/api/settings/') ? settingsPort : backendPort, method: req.method, path: req.url, headers }, response => {
      res.writeHead(response.statusCode, { ...response.headers, 'Cache-Control': 'no-store' }); response.pipe(res);
    });
    upstream.on('error', () => { if (!res.headersSent) fail(502, '本地服务暂时不可用'); else res.destroy(); });
    upstream.setTimeout(15000, () => upstream.destroy());
    res.on('close', () => upstream.destroy()); req.pipe(upstream); return;
  }
  if (!['GET', 'HEAD'].includes(req.method)) return fail(405, '不支持此操作');
  const root = path.resolve(webRoot);
  let relative;
  try { relative = decodeURIComponent(url.pathname); } catch { return fail(400, '无效路径'); }
  if (relative.includes('\\') || relative.includes('\0')) return fail(400, '无效路径');
  let file = path.resolve(root, '.' + relative);
  if (file !== root && !file.startsWith(root + path.sep)) return fail(403, '无效路径');
  if (!path.extname(file)) file = path.join(root, 'index.html');
  if (!mime[path.extname(file)]) return fail(404, '文件不存在');
  try {
    const contents = await fs.readFile(file);
    res.writeHead(200, { 'Content-Type': mime[path.extname(file)], 'Content-Length': contents.length, 'Cache-Control': file.endsWith('.html') ? 'no-cache' : 'public, max-age=31536000, immutable', 'X-Content-Type-Options': 'nosniff', 'X-Frame-Options': 'DENY', 'Referrer-Policy': 'same-origin' });
    res.end(req.method === 'HEAD' ? undefined : contents);
  } catch { fail(404, '文件不存在'); }
}
module.exports = { portableRequest };
