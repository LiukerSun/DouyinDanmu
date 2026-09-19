// Run with Playwright on NODE_PATH and UI_BASE_URL pointing to the frontend.
// All room, account and message data is synthetic; no collector is contacted.
const assert = require('node:assert/strict');
const { chromium } = require('playwright');
const base = process.env.UI_BASE_URL || 'http://localhost:3000';
const now = Date.now();
const room = { live_id: '123', source: 'douyin', enabled: true, status: 'collecting', detail: '', metadata_stale: false, stats: { chat: 30, gift: 2, enter: 30, like: 10, online: 100, total: 72 }, metadata: { title: '布局回归测试', live_status: 'live', checked_at_ms: now, anchor: { nickname: '测试主播', avatar_url: '' } } };
const pausedRoom = { ...room, live_id: '456', enabled: false, status: 'stopped', metadata: { ...room.metadata, anchor: { nickname: '暂停主播', avatar_url: '' } } };
const rooms = [room, pausedRoom];
const events = Array.from({ length: 30 }, (_, i) => ({ live_id: '123', event_id: 'layout-' + i, seq: String(i + 1), type: 'chat', method: 'WebcastChatMessage', user_name: '测试观众', user_id: '42', user_level: 5, content: '实时弹幕内容，检查消息区域能否完整展示。', timestamp: now + i, received_at_ms: now + i, persisted_at_ms: now + i }));

(async () => {
  const browser = await chromium.launch({ headless: true });
  try {
    const context = await browser.newContext();
    const errors = [];
    const operations = [];
    let roomReads = 0;
    await context.route('**/api/**', route => {
      const url = new URL(route.request().url());
      const method = route.request().method();
      if (method !== 'GET') {
        assert(method === 'POST' && url.pathname === '/api/rooms' || method === 'DELETE' && /^\/api\/rooms\/\d+$/.test(url.pathname), 'Unexpected mutation');
        const id = method === 'POST' ? route.request().postDataJSON().live_id : url.pathname.split('/').at(-1);
        const target = rooms.find(item => item.live_id === id);
        assert(target, 'Only synthetic rooms may be changed');
        target.enabled = method === 'POST';
        target.status = target.enabled ? 'collecting' : 'stopped';
        operations.push([method, id]);
        return route.fulfill({ json: { ok: true } });
      }
      let data;
      if (url.pathname === '/api/auth/session') data = { user: { username: 'layout-test', displayName: '布局测试', role: '管理员' }, setupRequired: false };
      else if (url.pathname === '/api/rooms') { roomReads++; data = rooms; }
      else if (url.pathname === '/api/health') data = { frames: 30, events: 30, quarantine: 0, rabbitmq: true, redis: true, database: true, collector: { online: true, spool_bytes: 0 } };
      else if (url.pathname.endsWith('/snapshot')) data = { events: url.pathname.includes('/123/') ? events : [], state_events: [], through_seq: '30' };
      else if (url.pathname.endsWith('/douyin-cookie')) data = { live_id: url.pathname.split('/').at(-2), auth_status: 'anonymous' };
      else throw new Error('Unexpected API: ' + url.pathname);
      return route.fulfill({ json: data });
    });
    await context.routeWebSocket('**/ws', socket => socket.onMessage(raw => {
      const command = JSON.parse(raw);
      if (command.action === 'subscribe') socket.send(JSON.stringify({ type: 'subscribed', live_id: command.live_id }));
    }));
    const page = await context.newPage();
    page.setDefaultTimeout(10000);
    page.on('pageerror', error => errors.push(error.message));
    await page.setViewportSize({ width: 1260, height: 740 });
    await page.goto(base);
    await page.getByRole('button', { name: '查看 测试主播 的消息', exact: true }).click();
    await page.locator('.message-list .sourced-message').first().waitFor();
    await page.evaluate(() => document.fonts.ready);
    const results = [];
    // The issue reporter uses a 2520 x 1680 screen. CSS viewports also cover
    // display scaling, browser chrome, short windows and the tablet breakpoint.
    for (const viewport of [{ width: 1680, height: 940 }, { width: 1260, height: 740 }, { width: 1260, height: 620 }, { width: 1050, height: 700 }, { width: 1008, height: 672 }, { width: 390, height: 844 }]) {
      await page.setViewportSize(viewport);
      await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
      if (viewport.width <= 760) await page.locator('.message-list').scrollIntoViewIfNeeded();
      const size = await page.locator('.message-list').evaluate(el => {
        el.scrollTop = el.scrollHeight;
        const rect = el.getBoundingClientRect();
        const visibleTop = Math.max(0, rect.top), visibleBottom = Math.min(innerHeight, rect.bottom);
        const fullRows = [...el.children].filter(child => { const r = child.getBoundingClientRect(); return r.top >= visibleTop && r.bottom <= visibleBottom; }).length;
        return { height: Math.round(rect.height), visibleHeight: Math.round(visibleBottom - visibleTop), fullRows, bottom: Math.round(rect.bottom), horizontalOverflow: document.documentElement.scrollWidth > innerWidth + 1 };
      });
      results.push({ width: viewport.width, viewportHeight: viewport.height, ...size });
      if (process.env.UI_SCREENSHOT_DIR && viewport.width === 1260 && viewport.height === 740) await page.screenshot({ path: process.env.UI_SCREENSHOT_DIR + '/monitor-layout.png', fullPage: true });
    }
    console.log(JSON.stringify(results, null, 2));
    for (const result of results.filter(r => r.width > 760)) {
      assert(result.visibleHeight >= result.height - 1, 'Message feed must fit the window: ' + JSON.stringify(result));
      assert(result.fullRows >= 4, 'Show at least four complete normal chat messages: ' + JSON.stringify(result));
      assert(!result.horizontalOverflow, 'No horizontal page overflow: ' + JSON.stringify(result));
    }
    assert(!results.at(-1).horizontalOverflow, 'Mobile must not overflow horizontally');
    assert(results.at(-1).fullRows >= 2, 'Mobile feed must have room for multiple messages');
    await page.setViewportSize({ width: 1260, height: 740 });
    await page.getByRole('button', { name: /返回监控台$/ }).click();
    await page.getByRole('button', { name: '暂停 测试主播 监控', exact: true }).click();
    await page.getByRole('button', { name: '启动 测试主播 监控', exact: true }).waitFor();
    assert.equal(await page.locator('.channel-card').count(), 2, 'Paused rooms remain available to restart');
    await page.locator('[data-room-id="123"] [data-slot="checkbox-control"]').click();
    await page.getByRole('checkbox', { name: '选择 暂停主播', exact: true }).press('Space');
    const previousReads = roomReads;
    await page.waitForTimeout(2800); // Cross the real 2.5-second refresh interval.
    assert(roomReads > previousReads, 'Exercise a room refresh before restarting');
    assert(await page.getByRole('checkbox', { name: '选择 测试主播', exact: true }).isChecked());
    assert(await page.getByRole('checkbox', { name: '选择 暂停主播', exact: true }).isChecked());
    assert(await page.getByRole('button', { name: '批量暂停监控', exact: true }).isDisabled());
    await page.getByRole('button', { name: '批量启动监控', exact: true }).click();
    await page.getByRole('button', { name: '暂停 暂停主播 监控', exact: true }).waitFor();
    assert.deepEqual(operations, [['DELETE', '123'], ['POST', '123'], ['POST', '456']]);
    await page.getByRole('button', { name: '配置 测试主播', exact: true }).click();
    await page.getByRole('button', { name: /暂停此房间监控$/ }).click();
    await page.getByRole('button', { name: /启动此房间监控$/ }).waitFor();
    assert.deepEqual(operations.at(-1), ['DELETE', '123']);
    assert.deepEqual(errors, []);
    console.log('PASS: scaled desktop/tablet/mobile layout, individual pause, retained selections after polling, batch restart and settings controls.');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
