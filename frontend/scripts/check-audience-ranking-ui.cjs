// Run with Playwright on NODE_PATH and UI_BASE_URL pointing to the frontend.
// Synthetic API and WebSocket data only; no live room or account is modified.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { chromium } = require('playwright');
const base = process.env.UI_BASE_URL || 'http://localhost:3000';
const now = Date.now();
const room = (id, name) => ({ live_id: id, source: 'douyin', enabled: true, status: 'collecting', detail: '', metadata_stale: false,
  stats: { chat: 40, gift: 0, enter: 20, like: 0, online: 321, total: 40 },
  metadata: { title: '观众榜验收', live_status: 'live', checked_at_ms: now, anchor: { nickname: name, avatar_url: '' } } });
const rooms = [room('111', '榜单测试主播'), room('222', '另一测试主播')];
const ranks = Array.from({ length: 100 }, (_, i) => ({ rank: i + 1, user_id: String(1000 + i), user_name: `扩展观众${i + 1}`, score: String(100 - i) }));
ranks[0].score = '0'; ranks[1].score = null; ranks[2].score = '9007199254740993';
ranks[3] = { ...ranks[3], hidden: true, user_name: '不可显示的昵称', score_description: '不可显示的贡献' };
ranks[4] = { ...ranks[4], score: '0', score_description: '1.2万贡献', exactly_score: '12000' };
const state = (live, seq, entries, type = 'audience_rank', received = now) => ({ live_id: live, seq, type, received_at_ms: received,
  method: type === 'audience_rank' ? 'WebcastRoomRankMessage' : 'WebcastRoomUserSeqMessage',
  audience_ranks: entries, audience_rank_source: type === 'audience_rank' ? 'room_rank' : 'room_user_seq', audience_ranks_total: entries.length,
  ...(type === 'online_count' ? { online_count: 321 } : {}) });
const events = Array.from({ length: 40 }, (_, i) => ({ live_id: '111', event_id: 'rank-chat-' + i, seq: String(i + 3), type: 'chat',
  user_id: '42', user_name: '消息观众', content: '观众榜展示时，实时消息仍有足够空间。', timestamp: now, received_at_ms: now, persisted_at_ms: now }));

(async () => {
  const browser = await chromium.launch({ headless: true });
  try {
    const context = await browser.newContext();
    const sockets = new Map(), errors = [];
    let username = 'audience-ui-test';
    await context.route('**/api/**', route => {
      const url = new URL(route.request().url());
      assert.equal(route.request().method(), 'GET', 'Display preferences must not mutate room settings');
      let data;
      if (url.pathname === '/api/auth/session') data = { user: { username, displayName: '观众榜验收', role: '管理员' }, setupRequired: false };
      else if (url.pathname === '/api/rooms') data = rooms;
      else if (url.pathname === '/api/health') data = { frames: 100, events: 100, quarantine: 0, rabbitmq: true, redis: true, database: true, collector: { online: true, spool_bytes: 0 } };
      else if (url.pathname.endsWith('/snapshot')) {
        const id = url.pathname.split('/').at(-2);
        data = { events: id === '111' ? events : [], state_events: id === '111'
          ? [state(id, '1', ranks.slice(0, 3), 'online_count'), { ...state(id, '2', ranks), audience_ranks_total: 130 }]
          : [state(id, '1', ranks.slice(0, 3).map(entry => ({ ...entry, hidden: false, user_name: '另一房观众' + entry.rank })), 'online_count')], through_seq: '200' };
      } else if (url.pathname.endsWith('/douyin-cookie')) data = { auth_status: 'anonymous', live_id: url.pathname.split('/').at(-2) };
      else throw new Error('Unexpected API: ' + url.pathname);
      return route.fulfill({ json: data });
    });
    await context.routeWebSocket('**/ws', socket => socket.onMessage(raw => {
      const command = JSON.parse(raw);
      if (command.action === 'subscribe') { sockets.set(command.live_id, socket); socket.send(JSON.stringify({ type: 'subscribed', live_id: command.live_id })); }
    }));
    const page = await context.newPage(); page.setDefaultTimeout(10000);
    page.on('pageerror', error => errors.push(error.message));
    await page.setViewportSize({ width: 1260, height: 740 });
    await page.goto(base);
    await page.getByRole('button', { name: '查看 榜单测试主播 的消息', exact: true }).click();
    const panel = page.getByRole('region', { name: '在线观众榜', exact: true });
    const rows = panel.locator('.audience-rank-list li');
    await rows.nth(9).waitFor(); assert.equal(await rows.count(), 10, 'Default limit is ten');
    assert.equal(await rows.nth(0).locator('.audience-rank-score').innerText(), '0');
    assert.equal(await rows.nth(1).locator('.audience-rank-score').innerText(), '未提供');
    assert.equal(await rows.nth(2).locator('.audience-rank-score').innerText(), '9,007,199,254,740,993');
    assert.equal(await rows.nth(3).locator('.audience-rank-name').innerText(), '神秘人');
    assert.equal(await rows.nth(3).locator('.audience-rank-score').innerText(), '未公开');
    assert.equal(await rows.nth(4).locator('.audience-rank-score').innerText(), '1.2万贡献');
    assert(!(await panel.innerText()).includes('不可显示'));
    assert((await panel.innerText()).includes('平台已下发 130 位'));
    const choose = async label => { await panel.locator('.studio-select').getByRole('button').click(); await page.getByRole('option', { name: label, exact: true }).click(); };
    await choose('前 20 位'); await rows.nth(19).waitFor(); assert.equal(await rows.count(), 20);
    await choose('自定义人数');
    await page.getByRole('spinbutton', { name: '自定义观众榜人数' }).fill('0');
    await panel.getByRole('button', { name: '应用人数', exact: true }).click();
    assert.equal(await panel.getByRole('alert').innerText(), '请输入 1–100 的整数'); assert.equal(await rows.count(), 20);
    await page.getByRole('spinbutton', { name: '自定义观众榜人数' }).fill('13');
    await panel.getByRole('button', { name: '应用人数', exact: true }).click();
    await page.waitForFunction(() => document.querySelectorAll('.audience-rank-list li').length === 13);
    await page.reload(); await rows.nth(12).waitFor(); assert.equal(await rows.count(), 13, 'Refresh restores the custom limit');
    username = 'audience-ui-other'; await page.reload();
    await page.getByRole('button', { name: '查看 榜单测试主播 的消息', exact: true }).click();
    await rows.nth(9).waitFor(); assert.equal(await rows.count(), 10, 'A second account has its own preference');
    username = 'audience-ui-test'; await page.reload(); await rows.nth(12).waitFor(); assert.equal(await rows.count(), 13);
    await page.getByRole('button', { name: /返回监控台$/ }).click();
    await page.getByRole('button', { name: '查看 另一测试主播 的消息', exact: true }).click();
    await rows.nth(2).waitFor(); assert.equal(await rows.count(), 3);
    assert((await panel.innerText()).includes('当前仅下发 3 位'));
    assert(!(await panel.innerText()).includes('扩展观众'));
    await page.getByRole('button', { name: /返回监控台$/ }).click();
    await page.getByRole('button', { name: '查看 榜单测试主播 的消息', exact: true }).click(); await rows.nth(12).waitFor();
    const send = update => sockets.get(update.live_id).send(JSON.stringify({ type: 'event_batch', live_id: update.live_id, events: [], state_events: [update], through_seq: update.seq }));
    send(state('111', '201', ranks)); send({ ...state('111', '202', ranks.slice(0, 3), 'online_count'), online_count: 777 });
    await page.waitForFunction(() => document.querySelector('.room-live-metrics').innerText.includes('777'));
    assert.equal(await rows.count(), 13, 'Online top-three updates cannot discard a fresh expanded list');
    send(state('111', '203', [])); await panel.getByText('平台本次未提供观众榜名单', { exact: true }).waitFor(); assert.equal(await rows.count(), 0);
    send(state('111', '201', ranks)); await page.waitForTimeout(140); assert.equal(await rows.count(), 0, 'Older replay cannot revive a cleared list');
    send(state('111', '204', ranks, 'audience_rank', now - 120000));
    await page.waitForFunction(() => document.querySelectorAll('.audience-rank-list li').length === 3);
    send(state('111', '205', ranks, 'audience_rank', now));
    await rows.nth(12).waitFor();
    await choose('前 100 位'); await rows.nth(99).waitFor(); assert.equal(await rows.count(), 100);
    await page.evaluate(() => document.fonts.ready);
    const output = path.resolve(__dirname, '../../.codex/reference/audience-ranking-ui'); fs.mkdirSync(output, { recursive: true });
    const layouts = [];
    for (const viewport of [{ width: 1260, height: 740 }, { width: 1260, height: 620 }, { width: 1050, height: 700 }, { width: 390, height: 844 }]) {
      await page.setViewportSize(viewport);
      if (viewport.width <= 760 && await page.getByRole('button', { name: '收起导航侧栏', exact: true }).isVisible()) {
        await page.getByRole('button', { name: '收起导航侧栏', exact: true }).click();
        await page.waitForFunction(() => getComputedStyle(document.querySelector('.sidebar-scrim')).visibility === 'hidden');
      }
      await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
      if (viewport.width <= 760) await page.locator('.message-list').scrollIntoViewIfNeeded();
      const layout = await page.evaluate(() => {
        const list = document.querySelector('.audience-rank-list'), feed = document.querySelector('.message-list'), rect = feed.getBoundingClientRect();
        return { rankHeight: list.clientHeight, rankScroll: list.scrollHeight, feedHeight: rect.height,
          visibleFeed: Math.min(innerHeight, rect.bottom) - Math.max(0, rect.top), overflow: document.documentElement.scrollWidth > innerWidth + 1 };
      });
      layouts.push({ ...viewport, ...layout });
      assert(!layout.overflow, 'No horizontal page overflow: ' + JSON.stringify(layout));
      assert(layout.rankHeight <= 101 && layout.rankScroll > layout.rankHeight, 'Large rankings scroll internally');
      assert(layout.feedHeight >= 120, 'Rankings must leave usable message space: ' + JSON.stringify(layout));
      if (viewport.width > 760) assert(layout.visibleFeed >= layout.feedHeight - 1, 'Desktop message area fits the viewport');
      if (viewport.width === 1260 && viewport.height === 740 || viewport.width === 390) {
        await page.evaluate(() => window.scrollTo(0, 0));
        await page.screenshot({ path: path.join(output, `audience-${viewport.width}.png`), fullPage: true });
      }
    }
    await panel.locator('.studio-select').getByRole('button').scrollIntoViewIfNeeded();
    await choose('前 10 位'); await page.waitForFunction(() => document.querySelectorAll('.audience-rank-list li').length === 10);
    await panel.getByRole('button', { name: '收起观众榜', exact: true }).click();
    await panel.getByRole('button', { name: '展开观众榜', exact: true }).click(); await rows.nth(9).waitFor();
    await page.setViewportSize({ width: 1260, height: 740 });
    await panel.getByRole('button', { name: '收起观众榜', exact: true }).click(); assert.equal(await rows.count(), 0);
    await panel.getByRole('button', { name: '展开观众榜', exact: true }).click(); await rows.nth(9).waitFor();
    assert.deepEqual(errors, []);
    console.log(JSON.stringify(layouts));
    console.log('PASS: default/preset/custom limits, validation, account preferences, room isolation, exact/unknown/hidden values, real-time updates, empty/replay/freshness and responsive scrolling.');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
