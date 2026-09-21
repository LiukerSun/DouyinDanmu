// Run after npm run build, with Playwright available on NODE_PATH.
// Drive the production UI using isolated room data; never contact a collector.
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const { once } = require('node:events');
const { chromium } = require('playwright');
const dist = path.resolve(__dirname, '../dist');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.woff2': 'font/woff2' };
const now = Date.now();
const fixtures = ['123', '456'].map((id, index) => ({ live_id: id, source: 'douyin', enabled: index === 0, status: index === 0 ? 'collecting' : 'stopped', detail: '', metadata_stale: false,
  stats: { chat: 1, gift: 0, enter: 0, like: 0, online: 1, total: 1 }, metadata: { title: '删除回归', live_status: 'live', checked_at_ms: now, anchor: { nickname: '主播' + id, avatar_url: '' } } }));
let rooms = structuredClone(fixtures);
const history = { live_id: '123', event_id: 'preserved-history', seq: '1', type: 'chat', method: 'WebcastChatMessage', user_id: '42', user_name: '测试观众', content: '删除后保留的历史消息', timestamp: now, received_at_ms: now };

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
    const context = await browser.newContext({ viewport: { width: 1260, height: 740 } });
    const errors = [], operations = [], searches = [];
    const failRemoval = new Set();
    let holdNextPoll = false, onPollHeld;
    await context.addInitScript(() => {
      if (!sessionStorage.getItem('removal-fixture')) {
        localStorage.setItem('monitor:pinned', '["123"]');
        localStorage.setItem('monitor:room-order:removal-test', '["123","456"]');
        sessionStorage.setItem('removal-fixture', '1');
      }
    });
    await context.route('**/api/**', async route => {
      const url = new URL(route.request().url());
      const method = route.request().method();
      if (method !== 'GET') {
        operations.push([method, url.pathname]);
        const removal = url.pathname.match(/^\/api\/rooms\/(\d+)\/remove$/);
        if (method === 'POST' && removal) {
          if (failRemoval.has(removal[1])) return route.fulfill({ status: 503, json: { error: '删除暂时失败，请重试' } });
          rooms = rooms.filter(room => room.live_id !== removal[1]);
          return route.fulfill({ json: { live_id: removal[1], status: 'removed' } });
        }
        if (method === 'POST' && url.pathname === '/api/rooms') {
          const id = route.request().postDataJSON().live_id;
          const target = rooms.find(room => room.live_id === id);
          if (target) { target.enabled = true; target.status = 'collecting'; }
          else rooms.push({ ...structuredClone(fixtures.find(room => room.live_id === id)), enabled: true, status: 'collecting' });
          return route.fulfill({ json: { live_id: id, status: 'started' } });
        }
        if (method === 'DELETE' && /^\/api\/rooms\/\d+$/.test(url.pathname)) {
          const target = rooms.find(room => room.live_id === url.pathname.split('/').at(-1));
          assert(target); target.enabled = false; target.status = 'stopped';
          return route.fulfill({ json: { status: 'stopped' } });
        }
        throw Error('Unexpected mutation: ' + method + ' ' + url.pathname);
      }
      let data;
      if (url.pathname === '/api/auth/session') data = { user: { username: 'removal-test', displayName: '删除测试', role: '管理员' }, setupRequired: false };
      else if (url.pathname === '/api/rooms') {
        data = structuredClone(rooms);
        if (holdNextPoll) {
          holdNextPoll = false;
          await new Promise(resolve => onPollHeld(async () => { await route.fulfill({ json: data }); resolve(); }));
          return;
        }
      }
      else if (url.pathname === '/api/health') data = { database: true, rabbitmq: true, redis: true, collector: { online: true }, frames: 1, events: 1, quarantine: 0 };
      else if (url.pathname.endsWith('/snapshot')) data = { events: [], state_events: [], through_seq: '0' };
      else if (url.pathname.endsWith('/douyin-cookie')) data = { live_id: url.pathname.split('/').at(-2), auth_status: 'anonymous' };
      else if (url.pathname === '/api/messages/search') { searches.push(url.searchParams); data = { events: [history], total: 1, next_before: null }; }
      else throw Error('Unexpected API: ' + url.pathname);
      return route.fulfill({ json: data });
    });
    await context.routeWebSocket('**/ws', socket => socket.close());
    const page = await context.newPage(); page.setDefaultTimeout(5000); page.on('pageerror', error => errors.push(error.message));
    await page.goto('http://127.0.0.1:' + server.address().port);
    await page.getByRole('button', { name: '配置 主播123', exact: true }).click();
    await page.getByRole('button', { name: /暂停此房间监控$/ }).waitFor();
    assert.equal(await page.getByRole('button', { name: '删除此直播间', exact: true }).count(), 1, 'Room settings must expose deletion separately from pause; v2.1.1 cannot remove a room');
    await page.getByRole('button', { name: '删除此直播间', exact: true }).click();
    await page.getByRole('dialog', { name: '删除直播间', exact: true }).waitFor();
    await page.getByText('删除后会停止采集，并从监控列表移除。历史记录和房间配置会保留，重新添加相同房间号可继续使用。', { exact: true }).waitFor();
    await page.getByRole('dialog', { name: '删除直播间', exact: true }).getByRole('button', { name: '取消', exact: true }).click();
    assert.deepEqual(operations, []);
    assert.equal(await page.locator('.channel-card').count(), 2, 'Cancel must leave all rooms unchanged');
    await page.getByRole('button', { name: '暂停 主播123 监控', exact: true }).click();
    await page.getByRole('button', { name: '启动 主播123 监控', exact: true }).waitFor();
    assert.equal(await page.locator('.channel-card').count(), 2, 'Pause remains distinct from deletion');
    await page.getByRole('button', { name: '启动 主播123 监控', exact: true }).click();
    await page.getByRole('button', { name: '暂停 主播123 监控', exact: true }).waitFor();

    // Hold an actual refresh response containing the soon-to-be-deleted room.
    await page.getByRole('button', { name: '删除直播间 主播123', exact: true }).click();
    const heldPoll = new Promise(resolve => { onPollHeld = resolve; }); holdNextPoll = true;
    const releasePoll = await Promise.race([heldPoll, new Promise((_, reject) => setTimeout(() => reject(Error('Expected a real room refresh during deletion')), 5000).unref())]);
    await page.getByRole('button', { name: '确认删除', exact: true }).click();
    await page.locator('[data-room-id="123"]').waitFor({ state: 'detached' });
    await releasePoll();
    await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
    assert.equal(await page.locator('[data-room-id="123"]').count(), 0, 'An older in-flight poll cannot resurrect a deleted room');
    assert.deepEqual(await page.evaluate(() => JSON.parse(localStorage.getItem('monitor:pinned'))), []);
    assert.deepEqual(await page.evaluate(() => JSON.parse(localStorage.getItem('monitor:room-order:removal-test'))), ['456']);
    await page.reload();
    await page.locator('[data-room-id="456"]').waitFor();
    assert.equal(await page.locator('.channel-card').count(), 1, 'Deleted rooms remain absent after a full reload');
    await page.getByRole('button', { name: '信息汇总', exact: true }).click();
    await page.getByText(history.content, { exact: true }).waitFor();
    await page.getByRole('button', { name: '筛选已删除直播间 123 的记录', exact: true }).click();
    await page.waitForResponse(response => new URL(response.url()).searchParams.get('room') === '123');
    await page.getByText(history.content, { exact: true }).waitFor();
    assert.equal(searches.at(-1).get('room'), '123', 'Deleted room history stays accessible without reopening a different room');

    await page.getByRole('button', { name: '直播工作台', exact: true }).click();
    await page.getByRole('button', { name: /添加直播间$/ }).click();
    await page.getByLabel('直播间地址', { exact: true }).fill('123');
    await page.getByRole('button', { name: /添加并开始监控$/ }).click();
    await page.locator('[data-room-id="123"]').waitFor();
    await page.getByRole('checkbox', { name: '选择当前筛选的全部直播间', exact: true }).press('Space');
    failRemoval.add('456');
    await page.getByRole('button', { name: '批量删除直播间', exact: true }).click();
    await page.getByRole('button', { name: '确认删除', exact: true }).click();
    await page.getByText('主播456：删除暂时失败，请重试', { exact: true }).waitFor();
    assert.equal(await page.locator('[data-room-id="123"]').count(), 0);
    assert.equal(await page.locator('[data-room-id="456"]').count(), 1, 'Failed deletions must remain visible and retryable');
    failRemoval.delete('456');
    await page.getByRole('button', { name: '确认删除', exact: true }).click();
    await page.getByText('开始监控第一个直播间', { exact: true }).waitFor();
    assert.equal(await page.evaluate(() => localStorage.getItem('monitor:active-room:removal-test')), null, 'Removing the last room clears persisted active selection');
    assert.equal(operations.filter(([method, pathname]) => method === 'POST' && pathname === '/api/rooms/123/remove').length, 2, 'Retry only failed rooms, never repeat successful removals');
    rooms = fixtures.map(room => ({ ...structuredClone(room), enabled: true, status: 'failed' }));
    await page.reload();
    await page.getByRole('button', { name: '采集故障', exact: true }).click();
    await page.getByRole('button', { name: '删除直播间 主播123', exact: true }).click();
    await page.getByRole('button', { name: '确认删除', exact: true }).click();
    await page.locator('[data-room-id="123"]').waitFor({ state: 'detached' });
    assert.equal(await page.locator('.studio-header h1').textContent(), '采集故障', 'Deleting the implicit active room must retain the fault-list context');
    await page.getByRole('button', { name: '查看 主播456 的消息', exact: true }).click();
    await page.getByRole('button', { name: /暂停展示$/ }).click();
    await page.getByRole('button', { name: /主播信息$/ }).click();
    await page.getByRole('button', { name: '删除此直播间', exact: true }).click();
    await page.getByRole('button', { name: '确认删除', exact: true }).click();
    await page.getByText('开始监控第一个直播间', { exact: true }).waitFor();
    assert.equal(await page.locator('.studio-header h1').textContent(), '直播监控台', 'Deleting the current frozen room exits its detail view');

    rooms = structuredClone(fixtures);
    await page.reload(); await page.locator('[data-room-id="123"]').waitFor();
    await page.setViewportSize({ width: 360, height: 800 });
    if (await page.getByRole('button', { name: '收起侧边栏', exact: true }).isVisible()) await page.getByRole('button', { name: '收起侧边栏', exact: true }).click();
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), 'Deletion controls must fit on mobile');
    for (const box of await page.locator('.channel-footer .button').evaluateAll(buttons => buttons.map(button => { const r = button.getBoundingClientRect(); return { left: r.left, right: r.right }; }))) {
      assert(box.left >= 0 && box.right <= 361, 'All card actions remain within the mobile viewport');
    }
    await page.getByRole('button', { name: '删除直播间 主播123', exact: true }).click();
    await page.getByRole('button', { name: '确认删除', exact: true }).waitFor();
    if (process.env.UI_SCREENSHOT_DIR) await page.screenshot({ path: path.join(process.env.UI_SCREENSHOT_DIR, 'room-removal-mobile.png'), fullPage: true, animations: 'disabled' });
    await page.getByRole('dialog', { name: '删除直播间', exact: true }).getByRole('button', { name: '取消', exact: true }).click();
    assert.deepEqual(errors, []);
    console.log('PASS: cancel, pause/restart, single deletion, stale refresh, reload persistence, retained history, re-add, mixed enabled/paused batch deletion, failure retry, last-room cleanup, fault navigation, frozen-room deletion and mobile controls');
  } finally {
    if (browser) await browser.close();
    server.closeAllConnections(); await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
