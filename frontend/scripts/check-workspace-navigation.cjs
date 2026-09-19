// Run after npm run build, with Playwright available on NODE_PATH.
// The browser uses the built UI; every API and WebSocket is isolated with mocks.
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const { once } = require('node:events');
const { chromium } = require('playwright');
const dist = path.resolve(__dirname, '../dist');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.woff2': 'font/woff2' };
const now = Date.now();
const rooms = ['111', '222', '333'].map((id, index) => ({ live_id: id, source: 'douyin', enabled: index !== 2, status: index === 2 ? 'stopped' : 'collecting', detail: '', metadata_stale: false,
  stats: { chat: 2, gift: 0, enter: 0, like: 0, online: 1, total: 2 }, metadata: { room_id: id, title: '测试直播间', live_status: 'live', checked_at_ms: now, anchor: { nickname: '房间' + id, avatar_url: '', signature: '' } } }));
const event = (uid, seq, content) => ({ event_id: 'event-' + seq, seq, live_id: '111', type: 'chat', content, user_id: uid, user_name: '观众' + uid, timestamp: now, received_at_ms: now });
const first = event('9007199254740993', '60', '用户当前记录');
const older = event(first.user_id, '1', '用户第二页记录');
const other = event('42', '59', '其他用户记录');
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
    const context = await browser.newContext({ viewport: { width: 1440, height: 1000 } });
    const queries = [], errors = [];
    await context.route('**/api/**', route => {
      const url = new URL(route.request().url());
      assert.equal(route.request().method(), 'GET');
      let data;
      if (url.pathname === '/api/auth/session') data = { user: { username: 'navigation-test', displayName: '测试', role: '管理员' }, setupRequired: false };
      else if (url.pathname === '/api/rooms') data = rooms;
      else if (url.pathname === '/api/health') data = { database: true, rabbitmq: true, redis: true, collector: { online: true }, frames: 1, events: 1, quarantine: 0 };
      else if (url.pathname.endsWith('/snapshot')) data = { events: [first], state_events: [], through_seq: '60' };
      else if (url.pathname === '/api/messages/search') {
        queries.push(url.searchParams);
        const scoped = url.searchParams.has('user_id'), next = url.searchParams.has('before');
        const selected = url.searchParams.get('user_id') === first.user_id ? first : other;
        data = { events: scoped ? next ? [older] : [selected] : [first, other], total: scoped ? selected === first ? 51 : 1 : 52, next_before: scoped && selected === first && !next ? '10' : null };
      } else throw Error('Unexpected API ' + url.pathname);
      return route.fulfill({ json: data });
    });
    await context.routeWebSocket('**/ws', socket => socket.close());
    const page = await context.newPage(); page.on('pageerror', error => errors.push(error.message));
    await page.goto('http://127.0.0.1:' + server.address().port);
    const order = () => page.locator('[data-room-id]').evaluateAll(nodes => nodes.map(node => node.dataset.roomId));
    await page.getByRole('button', { name: '拖动排序 房间333', exact: true }).waitFor();
    assert.deepEqual(await order(), ['111', '222', '333']);
    await page.getByRole('button', { name: '拖动排序 房间333', exact: true }).press('Home');
    await page.getByText('房间333 已移至当前列表第 1 位', { exact: true }).waitFor();
    assert.deepEqual(await order(), ['333', '111', '222']);
    await page.reload(); await page.getByRole('button', { name: '拖动排序 房间333', exact: true }).waitFor();
    assert.deepEqual(await order(), ['333', '111', '222'], 'Manual order persists across reloads with stopped rooms');
    await page.locator('.room-sort-select').getByRole('button').click(); await page.getByRole('option', { name: '默认排序', exact: true }).click();
    assert.deepEqual(await order(), ['111', '222', '333'], 'Default mode still groups enabled rooms first');
    await page.getByRole('button', { name: '查看 房间111 的消息', exact: true }).click();
    await page.getByRole('button', { name: '查看 观众9007199254740993 在所有直播间的记录', exact: true }).click();
    await page.locator('.archive-user-scope').waitFor(); await page.getByText('用户当前记录', { exact: true }).waitFor();
    assert.equal(queries.at(-1).get('user_id'), first.user_id);
    await page.getByRole('button', { name: '下一页', exact: true }).click(); await page.getByText('用户第二页记录', { exact: true }).waitFor();
    assert.equal(queries.at(-1).get('before'), '10');
    await page.getByRole('button', { name: '信息汇总', exact: true }).click(); await page.getByText('其他用户记录', { exact: true }).waitFor();
    assert.equal(await page.locator('.archive-user-scope').count(), 0);
    assert.equal(queries.at(-1).get('user_id'), null); assert.equal(queries.at(-1).get('before'), null);
    assert(await page.getByText('第 1 页 · 每页 50 条', { exact: true }).isVisible());
    // Selecting a user inside the archive must use the same controlled scope.
    await Promise.all([
      page.waitForResponse(response => new URL(response.url()).pathname === '/api/messages/search' && new URL(response.url()).searchParams.get('user_id') === '42'),
      page.getByRole('button', { name: '查看 观众42 在所有直播间的记录', exact: true }).click(),
    ]);
    await page.locator('.archive-user-scope').waitFor(); await page.getByText('其他用户记录', { exact: true }).waitFor();
    assert.equal(queries.at(-1).get('user_id'), '42');
    await page.getByRole('button', { name: '信息汇总', exact: true }).click(); await page.getByText('用户当前记录', { exact: true }).waitFor();
    assert.equal(await page.locator('.archive-user-scope').count(), 0); assert.equal(queries.at(-1).get('user_id'), null);
    assert.deepEqual(errors, []);
    console.log('PASS: stopped-room manual ordering and persistence, default ordering, global archive navigation clears controlled user scope and page cursor from both entry points');
  } finally {
    if (browser) await browser.close();
    server.closeAllConnections(); await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
