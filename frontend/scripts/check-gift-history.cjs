// Run after npm run build, with Playwright available on NODE_PATH.
// Exercise the real ranking -> frozen event-history UI with isolated API fixtures.
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const { once } = require('node:events');
const { chromium } = require('playwright');
const dist = path.resolve(__dirname, '../dist');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.woff2': 'font/woff2' };
const timestamp = new Date('2026-09-22T21:56:01+08:00').getTime();
const rooms = [{ live_id: '111', source: 'douyin', enabled: false, status: 'stopped', detail: '', metadata_stale: false,
  stats: { chat: 0, gift: 12, enter: 0, like: 0, online: 1, total: 12 },
  metadata: { room_id: '111', title: '礼物连送测试', live_status: 'live', checked_at_ms: timestamp, anchor: { nickname: '测试房间', avatar_url: '', signature: '' } } }];
const event = (count, index, delta) => ({ seq: String(index), event_id: 'gift-' + index, display_id: 'combo-1', live_id: '111', source: 'douyin', type: 'gift',
  timestamp: timestamp + index * 200, user_id: '58553866435', user_name: '送礼观众', user_level: 59, fans_club: { member: true, level: 8, name: '粉丝团', status: 1 },
  content: '嘉年华', gift_count: count, gift_unit_price: 30000, gift_combo: false, gift_final: index === 11,
  persisted_at_ms: timestamp + index * 200, received_at_ms: timestamp + index * 200,
  gift_statistics: { quantity_delta: String(delta), unit_price: 30000, value_delta: String(delta * 30000) } });
// Ten increasing deliveries, the terminal frame, then a delayed older count.
const events = [...Array.from({ length: 10 }, (_, i) => event(i + 1, i + 1, 1)), event(10, 11, 0), event(4, 12, 0)].reverse();
const { gift_statistics: omittedStatistics, ...mergedGift } = { ...events[1], gift_combo: true };
const user = { rank: 1, user_id: '58553866435', user_name: '送礼观众', user_level: 59, chat_count: '0', gift_quantity: '10', known_gift_value: '300000', unknown_price_quantity: '0', value_complete: true, last_seen_at_ms: timestamp };
const meta = { room: '111', as_of_seq: '12', from_ms: timestamp - 1000, to_ms: timestamp + 5000, unattributed_event_count: '0' };

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
    const context = await browser.newContext({ viewport: { width: 1600, height: 1100 } });
    const errors = [], historyQueries = [];
    await context.route('**/api/**', route => {
      const url = new URL(route.request().url());
      assert.equal(route.request().method(), 'GET');
      let data;
      if (url.pathname === '/api/auth/session') data = { user: { username: 'gift-history-test', displayName: '测试', role: '管理员' }, setupRequired: false };
      else if (url.pathname === '/api/rooms') data = rooms;
      else if (url.pathname === '/api/health') data = { database: true, rabbitmq: true, redis: true, collector: { online: true }, frames: 12, events: 12, quarantine: 0 };
      else if (url.pathname.endsWith('/snapshot')) data = { events: [mergedGift], state_events: [], through_seq: '12' };
      else if (url.pathname.includes('/analytics/')) data = { items: [user], total: '1', next_offset: null, meta };
      else if (url.pathname === '/api/messages/search') {
        historyQueries.push(url.searchParams);
        data = { events, total: events.length, next_before: null };
      } else throw Error('Unexpected API ' + url.pathname);
      return route.fulfill({ json: data });
    });
    await context.routeWebSocket('**/ws', socket => socket.close());
    const page = await context.newPage(); page.on('pageerror', error => errors.push(error.message));
    await page.goto('http://127.0.0.1:' + server.address().port);
    await page.getByRole('button', { name: '查看 测试房间 的消息', exact: true }).click();
    await page.locator('.gift-price').waitFor();
    assert.equal(await page.locator('.gift-number').innerText(), '× 10');
    assert.equal(await page.locator('.gift-price').innerText(), '300,000 钻石', 'The live merged gift still shows its cumulative value');
    await page.getByRole('radio', { name: '本房间排行榜', exact: true }).click();
    await page.getByRole('radio', { name: '礼物榜', exact: true }).click();
    await page.getByRole('table', { name: '测试房间的礼物榜' }).waitFor();
    assert.match(await page.locator('.ranking-table').innerText(), /300,000/);
    await page.getByRole('button', { name: '查看 送礼观众 在本房间的记录', exact: true }).click();
    const history = page.locator('.ranking-history-list');
    await history.locator('.message-row').first().waitFor();
    assert.equal(await history.locator('.message-row').count(), 12, 'Retain every original event, including terminal and out-of-order deliveries');
    assert.deepEqual(await history.locator('.gift-number').allTextContents(), ['× 0', '× 0', ...Array(10).fill('× 1')], 'History gift counts must be authoritative increments, never the cumulative 1..10 frames');
    assert.deepEqual(await history.locator('.gift-price').allTextContents(), ['0 钻石', '0 钻石', ...Array(10).fill('30,000 钻石')]);
    assert.equal(await history.getByText('本次新增', { exact: true }).count(), 10);
    assert.equal(await history.getByText('本次未新增', { exact: true }).count(), 2);
    assert.equal(await history.getByText('不重复计入', { exact: true }).count(), 2);
    assert.deepEqual(await history.locator('.gift-reported-count').allTextContents(), [4, 10, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1].map(count => '上报数量 × ' + count));
    const query = historyQueries.at(-1);
    assert.equal(query.get('room'), '111'); assert.equal(query.get('user_id'), user.user_id);
    assert.equal(query.get('view'), 'events'); assert.equal(query.get('as_of_seq'), meta.as_of_seq);
    assert.equal(query.get('from_ms'), String(meta.from_ms)); assert.equal(query.get('to_ms'), String(meta.to_ms));
    await page.getByRole('button', { name: '返回排行榜', exact: true }).click();
    await page.getByRole('table', { name: '测试房间的礼物榜' }).waitFor();
    assert.match(await page.locator('.ranking-table').innerText(), /300,000/);
    assert.deepEqual(errors, []);
    console.log('PASS: real ranking history uses exact gift increments, preserves cumulative source counts and frozen event queries, retains zero-delta frames and leaves live merged display unchanged');
  } finally {
    if (browser) await browser.close();
    server.closeAllConnections(); await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
