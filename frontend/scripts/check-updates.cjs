// Exercise the production UI with mocked update services; never downloads a release.
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const { once } = require('node:events');
const { chromium } = require('playwright');
const dist = path.resolve(__dirname, '../dist');
const screenshots = path.resolve(__dirname, '../../.cache/update-ui-review');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.woff2': 'font/woff2' };
const now = Date.now();
const room = { live_id: '123', source: 'douyin', enabled: true, status: 'collecting', detail: '', metadata_stale: false,
  stats: { chat: 12, gift: 0, enter: 0, like: 0, online: 1, total: 12 },
  metadata: { title: '更新功能验证', live_status: 'live', checked_at_ms: now, anchor: { nickname: '测试直播间', avatar_url: '' } } };
const fixture = () => ({ current_version: '2.5.0', latest_version: '2.6.0', update_available: true, can_install: true,
  release_url: 'https://github.com/LiukerSun/DouyinDanmu/releases/tag/v2.6.0',
  release_notes: '新增版本更新管理。\n保留账号、Cookie、配置和历史数据。\n<script>window.releaseScriptExecuted = true</script>',
  published_at: '2026-10-27T00:00:00Z', checked_at: now, checking: false, error: null,
  install: { phase: 'idle', downloaded_bytes: 0, total_bytes: 0, error: null } });

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
    await fs.mkdir(screenshots, { recursive: true });
    async function scenario(initial = fixture(), viewport = { width: 1260, height: 800 }, firstStatusGate = null) {
      const context = await browser.newContext({ viewport });
      let state = structuredClone(initial), checkCount = 0, installCount = 0, offline = false, failCheck = false, holdInstall = null;
      const errors = [];
      await context.route('**/api/**', async route => {
        const pathname = new URL(route.request().url()).pathname, method = route.request().method();
        if (pathname === '/api/updates/status') {
          if (offline) return route.abort('connectionrefused');
          if (firstStatusGate) {
            const snapshot = structuredClone(state), gate = firstStatusGate; firstStatusGate = null;
            await gate;
            return route.fulfill({ json: snapshot });
          }
          return route.fulfill({ json: state });
        }
        if (pathname === '/api/updates/check') {
          assert.equal(method, 'POST'); checkCount++;
          state.error = failCheck ? 'GitHub 暂时无法连接，请稍后重试。' : null;
          return route.fulfill({ json: state });
        }
        if (pathname === '/api/updates/install') {
          assert.equal(method, 'POST'); installCount++;
          if (holdInstall) await holdInstall;
          state.can_install = false;
          state.install = { phase: 'downloading', downloaded_bytes: 1024, total_bytes: 4096, error: null };
          return route.fulfill({ status: 202, json: state });
        }
        let data;
        if (pathname === '/api/auth/session') data = { user: { username: 'update-test', displayName: '更新测试', role: '管理员' }, setupRequired: false };
        else if (pathname === '/api/rooms') data = [room];
        else if (pathname === '/api/health') data = { version: state.current_version, database: true, rabbitmq: true, redis: true, collector: { online: true }, frames: 12, events: 12, quarantine: 0 };
        else if (pathname.endsWith('/snapshot')) data = { events: [], state_events: [], through_seq: '0' };
        else if (pathname.endsWith('/douyin-cookie')) data = { live_id: '123', auth_status: 'anonymous' };
        else throw Error('Unexpected API: ' + pathname);
        return route.fulfill({ json: data });
      });
      await context.routeWebSocket('**/ws', socket => socket.close());
      const page = await context.newPage(); page.setDefaultTimeout(6000); page.on('pageerror', error => errors.push(error.message));
      await page.goto('http://127.0.0.1:' + server.address().port);
      await page.locator('[data-room-id="123"]').waitFor();
      const open = async () => {
        if (state.update_available) await page.getByRole('button', { name: /新版本/ }).click();
        else {
          if (viewport.width < 760) await page.getByRole('button', { name: '展开侧边栏', exact: true }).click();
          await page.getByRole('button', { name: '更新记录', exact: true }).click();
        }
        const dialog = page.getByRole('dialog', { name: '版本与更新', exact: true }); await dialog.waitFor(); return dialog;
      };
      return { context, page, errors, open, state, counts: () => ({ checkCount, installCount }),
        failCheck: value => { failCheck = value; }, offline: value => { offline = value; },
        hold: promise => { holdInstall = promise; },
      };
    }

    let releaseFirstStatus;
    const concurrent = await scenario(fixture(), undefined, new Promise(resolve => { releaseFirstStatus = resolve; }));
    await concurrent.page.getByRole('button', { name: '更新记录', exact: true }).click();
    const concurrentPanel = concurrent.page.getByRole('dialog', { name: '版本与更新', exact: true });
    concurrent.state.latest_version = '2.7.0';
    await concurrentPanel.getByRole('button', { name: '检查更新', exact: true }).click();
    await concurrentPanel.getByText('发现新版本 v2.7.0', { exact: true }).waitFor();
    const staleResponse = concurrent.page.waitForResponse(response => new URL(response.url()).pathname === '/api/updates/status');
    releaseFirstStatus();
    await staleResponse;
    assert.equal(await concurrentPanel.getByText('发现新版本 v2.7.0', { exact: true }).count(), 1, 'An old automatic status response must not overwrite a newer manual check');
    assert.equal(concurrent.counts().checkCount, 1, 'A background status request must not swallow a manual check');
    await concurrent.context.close();

    const update = await scenario();
    const dialog = await update.open();
    await dialog.getByText('发现新版本 v2.6.0', { exact: true }).waitFor();
    assert.equal(await dialog.getByRole('link', { name: '前往 GitHub' }).getAttribute('href'), fixture().release_url);
    assert.equal(await update.page.evaluate(() => window.releaseScriptExecuted), undefined, 'Release text must never execute as HTML');
    await update.page.screenshot({ path: path.join(screenshots, 'desktop.png'), fullPage: true, animations: 'disabled' });
    update.failCheck(true); await dialog.getByRole('button', { name: '检查更新', exact: true }).click();
    await dialog.getByText('GitHub 暂时无法连接，请稍后重试。', { exact: true }).waitFor();
    assert.equal(await update.page.locator('[data-room-id="123"]').count(), 1, 'Check failures must leave the workspace intact');
    update.failCheck(false); await dialog.getByRole('button', { name: '检查更新', exact: true }).click();
    await dialog.getByText('GitHub 暂时无法连接，请稍后重试。', { exact: true }).waitFor({ state: 'hidden' });
    assert.equal(update.counts().checkCount, 2);

    let releaseInstall; update.hold(new Promise(resolve => { releaseInstall = resolve; }));
    await dialog.getByRole('button', { name: '下载并重启更新', exact: true }).click();
    const busyButton = dialog.getByRole('button', { name: '更新进行中…', exact: true });
    await busyButton.waitFor(); assert(await busyButton.isDisabled());
    assert.equal(update.counts().installCount, 1);
    releaseInstall();
    await dialog.getByText('正在下载更新包', { exact: true }).waitFor();
    assert.equal(await dialog.getByRole('progressbar', { name: '更新包下载进度' }).getAttribute('value'), '1024');
    update.state.install.phase = 'verifying';
    await dialog.getByText('正在校验更新包', { exact: true }).waitFor();
    update.state.install.phase = 'staging';
    await dialog.getByText('正在准备新版本', { exact: true }).waitFor();
    update.state.install.phase = 'restarting';
    await dialog.getByText('正在重启程序', { exact: true }).waitFor();
    update.offline(true);
    await dialog.getByText('更新连接暂时中断', { exact: true }).waitFor();
    assert(await dialog.getByRole('button', { name: '重新连接', exact: true }).isEnabled());
    assert.equal(update.counts().installCount, 1, 'Progress polling must not resubmit installation');
    assert.deepEqual(update.errors, []); await update.context.close();

    const failedState = fixture(); failedState.install = { phase: 'error', downloaded_bytes: 0, total_bytes: 0, error: '更新包校验失败，请重新下载。' };
    const failed = await scenario(failedState); const retryPanel = await failed.open();
    await retryPanel.getByText('更新包校验失败，请重新下载。', { exact: true }).waitFor();
    await retryPanel.getByRole('button', { name: '下载并重启更新', exact: true }).click();
    await retryPanel.getByText('正在下载更新包', { exact: true }).waitFor();
    assert.equal(failed.counts().installCount, 1, 'An installation failure must allow an explicit retry');
    assert.deepEqual(failed.errors, []); await failed.context.close();

    for (const current of ['2.6.0', '2.7.0']) {
      const state = fixture(); state.current_version = current; state.update_available = false; state.can_install = false;
      const scenarioCase = await scenario(state); const panel = await scenarioCase.open();
      await panel.getByText(current === '2.6.0' ? '当前已是最新稳定版' : '未发现比当前版本更新的稳定版', { exact: true }).waitFor();
      assert.equal(await panel.getByRole('button', { name: '下载并重启更新', exact: true }).count(), 0);
      assert.deepEqual(scenarioCase.errors, []); await scenarioCase.context.close();
    }

    const unknownState = fixture(); unknownState.latest_version = null; unknownState.checked_at = null; unknownState.update_available = false; unknownState.can_install = false;
    const unknown = await scenario(unknownState); const unknownPanel = await unknown.open();
    await unknownPanel.getByText('尚未取得最新版本信息', { exact: true }).waitFor();
    assert.equal(await unknownPanel.getByText('当前已是最新稳定版', { exact: true }).count(), 0);
    await unknown.context.close();

    const dockerState = fixture(); dockerState.can_install = false; dockerState.release_url = 'https://untrusted.example/update';
    const docker = await scenario(dockerState, { width: 390, height: 844 }); const dockerPanel = await docker.open();
    await dockerPanel.getByText('当前部署或发布包暂不支持一键更新，请前往 GitHub 按发布说明更新。', { exact: true }).waitFor();
    assert.equal(await dockerPanel.getByRole('button', { name: '下载并重启更新', exact: true }).count(), 0);
    assert.equal(await dockerPanel.getByRole('link', { name: '前往 GitHub' }).getAttribute('href'), 'https://github.com/LiukerSun/DouyinDanmu/releases/latest');
    assert(await docker.page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), 'The narrow viewport must not overflow horizontally');
    await docker.page.screenshot({ path: path.join(screenshots, 'mobile.png'), fullPage: true, animations: 'disabled' });
    assert.deepEqual(docker.errors, []); await docker.context.close();
    console.log('PASS: automatic update notice and manual-check race; check failure/retry; stable/current/ahead/unknown versions; Docker link; install deduplication, progress and failure retry; restart disconnect; safe release text/URLs; desktop and mobile layouts');
  } finally {
    if (browser) await browser.close();
    server.closeAllConnections(); await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
