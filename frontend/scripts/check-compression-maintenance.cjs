// Run after npm run build, with Playwright available on NODE_PATH.
// All service responses are fixtures; this never connects to a real database.
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const { once } = require('node:events');
const { chromium } = require('playwright');
const dist = path.resolve(__dirname, '../dist');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.woff2': 'font/woff2' };
const now = Date.now();
const rooms = [{ live_id: '123', source: 'douyin', enabled: true, status: 'collecting', detail: '', metadata_stale: false,
  stats: { chat: 7, gift: 0, enter: 0, like: 0, online: 1, total: 7 },
  metadata: { title: '压缩维护回归', live_status: 'live', checked_at_ms: now, anchor: { nickname: '维护测试主播', avatar_url: '' } } }];

(async () => {
  const server = http.createServer(async (req, res) => {
    const pathname = new URL(req.url, 'http://localhost').pathname;
    const file = path.resolve(dist, '.' + (pathname === '/' ? '/index.html' : pathname));
    if (!file.startsWith(dist + path.sep) || !mime[path.extname(file)]) { res.writeHead(404); res.end(); return; }
    try { const body = await fs.readFile(file); res.writeHead(200, { 'Content-Type': mime[path.extname(file)] }); res.end(body); }
    catch { res.writeHead(404); res.end(); }
  }).listen(0, '127.0.0.1');
  await once(server, 'listening');
  let browser;
  try {
    browser = await chromium.launch({ headless: true });
    const context = await browser.newContext({ viewport: { width: 1260, height: 800 } });
    const errors = [], operations = [];
    let phase = 'pending', statusRequests = 0, unavailableHealth = 0, unavailableRooms = 0;
    const maintenance = () => ({
      compression_enabled: true,
      reencode: ['event_details', 'events', 'event_presentations'].map(scope => ({ scope, done: true })),
      vacuum_pending: phase === 'pending', vacuum_queued: phase === 'queued', vacuum_running: phase === 'running',
    });
    await context.route('**/api/**', async route => {
      const pathname = new URL(route.request().url()).pathname, method = route.request().method();
      if (method !== 'GET') {
        operations.push([method, pathname]);
        assert.equal(method, 'POST'); assert.equal(pathname, '/api/maintenance/compact');
        assert.equal(phase, 'pending', 'Only the initial manual request is expected');
        phase = 'queued';
        return route.fulfill({ status: 202, json: { queued: true } });
      }
      if (phase === 'running' && ['/api/health', '/api/rooms'].includes(pathname)) {
        if (pathname === '/api/health') unavailableHealth++; else unavailableRooms++;
        return route.fulfill({ status: 503, json: { error: 'Database maintenance in progress' } });
      }
      let data;
      if (pathname === '/api/auth/session') data = { user: { username: 'maintenance-test', displayName: '维护测试', role: '管理员' }, setupRequired: false };
      else if (pathname === '/api/rooms') data = rooms;
      else if (pathname === '/api/health') data = {
        database: true, rabbitmq: true, redis: true, collector: { online: true }, frames: phase === 'done' ? 12346 : 12345,
        events: 201, quarantine: 0, database_bytes: 1024 * 1024, wal_bytes: 0, maintenance: maintenance(),
      };
      else if (pathname === '/api/maintenance/status') { statusRequests++; data = maintenance(); }
      else if (pathname.endsWith('/snapshot')) data = { events: [], state_events: [], through_seq: '0' };
      else if (pathname.endsWith('/douyin-cookie')) data = { live_id: '123', auth_status: 'anonymous' };
      else throw Error('Unexpected API: ' + pathname);
      return route.fulfill({ json: data });
    });
    await context.routeWebSocket('**/ws', socket => socket.close());
    const page = await context.newPage();
    page.setDefaultTimeout(5000); page.on('pageerror', error => errors.push(error.message));
    await page.goto('http://127.0.0.1:' + server.address().port);
    await page.locator('[data-room-id="123"]').waitFor();
    await page.getByRole('button', { name: '服务管理', exact: true }).click();
    const dialog = page.getByRole('dialog', { name: '服务运行状态', exact: true });
    await dialog.waitFor();
    const stat = label => dialog.getByText(label, { exact: true }).locator('..').locator('dd');
    const compact = dialog.getByRole('button', { name: /立即整理数据库$/ });
    await compact.waitFor();
    assert(await compact.isEnabled(), 'A deferred startup vacuum must not disable manual compaction');
    assert.equal(await stat('已处理原始帧').textContent(), '12,345');
    assert.equal(await stat('数据库大小').textContent(), '1.0 MB');
    assert.equal(await stat('数据压缩').textContent(), '已完成');

    await compact.click();
    const queued = dialog.getByRole('button', { name: /等待整理…$/ });
    await queued.waitFor();
    assert(await queued.isDisabled(), 'An accepted compaction request must not be submitted twice');
    assert.deepEqual(operations, [['POST', '/api/maintenance/compact']]);

    phase = 'running';
    await dialog.getByRole('button', { name: /刷新状态$/ }).click();
    const running = dialog.getByRole('button', { name: /正在整理数据库…$/ });
    await running.waitFor();
    assert(await running.isDisabled());
    await dialog.getByText('整理期间采集与查询会暂停，视数据库大小可能需要几分钟，完成后自动恢复。', { exact: true }).waitFor();
    assert(statusRequests > 0 && unavailableHealth > 0 && unavailableRooms > 0, 'Maintenance status must remain available when database-backed endpoints return 503');
    assert.equal(await stat('已处理原始帧').textContent(), '12,345', 'Maintenance must retain the last health snapshot');
    assert.equal(await stat('数据库大小').textContent(), '1.0 MB');
    assert.equal(await page.locator('[data-room-id="123"]').count(), 1, 'Maintenance must retain the room list');

    phase = 'done';
    await dialog.getByRole('button', { name: /刷新状态$/ }).click();
    await compact.waitFor();
    assert(await compact.isEnabled(), 'Compaction becomes available again after completion');
    assert.equal(await stat('已处理原始帧').textContent(), '12,346', 'Normal polling resumes after compaction');
    assert.equal(await dialog.getByText('整理期间采集与查询会暂停，视数据库大小可能需要几分钟，完成后自动恢复。', { exact: true }).count(), 0);
    assert.deepEqual(operations, [['POST', '/api/maintenance/compact']]);
    assert.deepEqual(errors, []);
    console.log('PASS: pending vacuum can be queued manually; queued/running requests stay disabled; independent maintenance status preserves health and rooms during 503; completion restores polling and compaction');
  } finally {
    if (browser) await browser.close();
    server.closeAllConnections(); await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
