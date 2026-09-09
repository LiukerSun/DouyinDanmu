// Isolated browser checks: mock every API and WebSocket; never alter live rooms.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { chromium } = require('playwright');
const base = process.env.UI_BASE_URL || 'http://127.0.0.1:4179';
const timestamp = Date.now();
const rooms = ['111', '222'].map((id, index) => ({ live_id: id, source: 'douyin', enabled: true, status: 'collecting', detail: '', metadata_stale: false,
  stats: { chat: 31, gift: 1, enter: 1, like: 1, online: 1, total: 35 }, metadata: { room_id: id, title: '测试房间', live_status: 'live', checked_at_ms: timestamp, anchor: { nickname: ['青禾的直播间', '小林的直播间'][index], avatar_url: '', signature: '' } } }));
const user = (room, rank = 1) => ({ rank, user_id: `${room}${rank}`, user_name: (room === '111' ? '青禾观众' : '小林观众') + rank, user_level: 32,
  chat_count: '9007199254740993', gift_quantity: '12', known_gift_value: '320', unknown_price_quantity: '2', value_complete: false, last_seen_at_ms: timestamp });
(async () => {
  const browser = await chromium.launch({ headless: true });
  try {
    const context = await browser.newContext({ viewport: { width: 1440, height: 1000 } });
    const requests = [], errors = []; let failNext = false;
    await context.route('**/api/**', async route => {
      const url = new URL(route.request().url()); requests.push(url);
      assert.equal(route.request().method(), 'GET');
      let data;
      if (url.pathname === '/api/auth/session') data = { user: { username: 'ranking-test', displayName: '测试', role: '管理员' }, setupRequired: false };
      else if (url.pathname === '/api/rooms') data = rooms;
      else if (url.pathname === '/api/health') data = { database: true, rabbitmq: true, redis: true, collector: { online: true }, frames: 1, events: 1, quarantine: 0 };
      else if (url.pathname.endsWith('/snapshot')) data = { events: [], state_events: [], through_seq: '0' };
      else if (url.pathname.includes('/analytics/')) {
        assert(['111', '222'].includes(url.searchParams.get('room')), 'Every ranking request must explicitly target one room');
        if (failNext) { failNext = false; return route.fulfill({ status: 500, json: { error: '测试请求失败' } }); }
        const room = url.searchParams.get('room'), offset = Number(url.searchParams.get('offset'));
        const empty = room === '222' && url.pathname.includes('/gift-');
        data = { items: empty ? [] : Array.from({ length: offset ? 1 : 20 }, (_, index) => user(room, offset + index + 1)), total: empty ? '0' : '21', next_offset: offset || empty ? null : 20,
          meta: { room, as_of_seq: '9876', from_ms: url.searchParams.has('from_ms') ? Number(url.searchParams.get('from_ms')) : null, to_ms: url.searchParams.has('to_ms') ? Number(url.searchParams.get('to_ms')) : null, unattributed_event_count: '0' } };
      } else if (url.pathname === '/api/messages/search') {
        assert.equal(url.searchParams.get('room'), '111', 'History must retain room scope');
        assert.equal(url.searchParams.get('user_id'), '1111');
        assert.equal(url.searchParams.get('as_of_seq'), '9876');
        assert.equal(url.searchParams.get('view'), 'events');
        data = { events: [{ event_id: 'history-one', seq: '1', live_id: '111', type: 'chat', content: '只属于青禾房间的记录', user_id: '1111', user_name: '青禾观众1', timestamp, persisted_at_ms: timestamp, received_at_ms: timestamp }], total: 1, next_before: null };
      } else throw new Error('Unexpected API ' + url.pathname);
      await route.fulfill({ json: data });
    });
    await context.routeWebSocket('**/ws', socket => socket.close());
    const page = await context.newPage(); page.on('pageerror', error => { errors.push(error.message); console.error(error.message); });
    await page.goto(base);
    await page.getByRole('button', { name: '查看 青禾的直播间 的消息', exact: true }).click();
    await page.getByRole('radio', { name: '本房间排行榜', exact: true }).click();
    await page.getByRole('table', { name: '青禾的直播间的弹幕榜' }).waitFor();
    assert((await page.locator('.ranking-table').innerText()).includes('9,007,199,254,740,993'), 'Counts must not lose precision');
    await page.getByRole('button', { name: '下一页', exact: true }).click();
    await page.getByRole('button', { name: '青禾观众21', exact: true }).waitFor();
    const second = requests.filter(url => url.pathname.includes('ranking')).at(-1);
    assert.equal(second.searchParams.get('as_of_seq'), '9876'); assert.equal(second.searchParams.get('offset'), '20');
    await page.getByRole('button', { name: '上一页', exact: true }).click();
    await page.getByRole('button', { name: '青禾观众1', exact: true }).click();
    await page.getByText('只属于青禾房间的记录', { exact: true }).waitFor();
    await page.getByRole('button', { name: '返回排行榜', exact: true }).click();
    await page.getByRole('radio', { name: '礼物榜', exact: true }).click();
    await page.getByRole('table', { name: '青禾的直播间的礼物榜' }).waitFor();
    assert((await page.locator('.ranking-table').innerText()).includes('2 件价格未知'));
    await page.locator('.ranking-options .studio-select').first().getByRole('button').click();
    await page.getByRole('option', { name: '今天', exact: true }).click();
    await page.getByRole('table').waitFor();
    await page.getByRole('listbox').waitFor({ state: 'hidden' });
    const today = requests.filter(url => url.pathname.includes('ranking')).at(-1);
    assert(today.searchParams.has('from_ms') && today.searchParams.has('to_ms'));
    assert.equal(today.searchParams.get('offset'), '0'); assert.equal(today.searchParams.get('as_of_seq'), null);
    const output = path.resolve(__dirname, '../../.codex/reference/ranking-ui'); fs.mkdirSync(output, { recursive: true });
    await page.screenshot({ path: path.join(output, 'desktop.png'), fullPage: true, animations: 'disabled' });
    failNext = true;
    await page.getByRole('button', { name: '刷新榜单', exact: true }).click();
    await page.getByText('测试请求失败', { exact: true }).waitFor();
    await page.getByRole('button', { name: '重试', exact: true }).click();
    await page.getByRole('table').waitFor();
    await page.setViewportSize({ width: 390, height: 844 });
    await page.getByRole('button', { name: '收起侧边栏', exact: true }).click();
    await page.locator('.sidebar-collapsed').waitFor();
    assert(await page.locator('.ranking-table-scroll').evaluate(node => node.clientHeight >= 240), 'Mobile controls must leave usable room for ranking rows');
    await page.screenshot({ path: path.join(output, 'mobile.png'), fullPage: true, animations: 'disabled' });
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth + 1), 'Mobile table must scroll internally, not overflow the page');
    await page.setViewportSize({ width: 1440, height: 1000 });
    await page.getByRole('button', { name: '监控台', exact: true }).click();
    await page.getByRole('button', { name: '查看 小林的直播间 的消息', exact: true }).click();
    await page.getByRole('radio', { name: '本房间排行榜', exact: true }).click();
    await page.getByRole('table', { name: '小林的直播间的弹幕榜' }).waitFor();
    assert(!(await page.locator('.ranking-table').innerText()).includes('青禾观众'));
    await page.getByRole('radio', { name: '礼物榜', exact: true }).click();
    await page.getByText('还没有可排行的送礼记录', { exact: true }).waitFor();
    assert.deepEqual(errors, []);
    console.log('Room rankings passed: room isolation, exact amounts, frozen pagination, scoped history, dates, retry, empty state and mobile overflow.');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exit(1); });
