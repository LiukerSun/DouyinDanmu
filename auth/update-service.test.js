'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { createUpdateService, compareVersions } = require('./update-service');
const tick = () => new Promise(resolve => setImmediate(resolve));
const sha256 = 'a'.repeat(64);
const release = (version = '2.6.0') => ({ tag_name: 'v' + version, draft: false, prerelease: false,
  html_url: 'https://untrusted.invalid/ignored', body: '<script>plain release notes</script>', published_at: '2026-10-27T00:00:00Z',
  assets: [{ name: 'DouyinDanmu-win-x64-portable.zip', state: 'uploaded', size: 100,
    browser_download_url: `https://github.com/LiukerSun/DouyinDanmu/releases/download/v${version}/DouyinDanmu-win-x64-portable.zip`, digest: 'sha256:' + sha256 }] });
const response = (body, headers = {}) => new Response(JSON.stringify(body), { status: 200, headers });

test('SemVer uses numeric components and prerelease precedence, without coercing malformed versions', () => {
  for (const [a, b, expected] of [
    ['2.10.0', '2.9.9', 1], ['2.6.0', '2.6.0', 0], ['2.5.0', '2.6.0', -1],
    ['2.6.0', '2.6.0-rc.1', 1], ['2.6.0-beta.10', '2.6.0-beta.2', 1],
    ['2.6.0-1', '2.6.0-alpha', -1], ['2.6.0-alpha', '2.6.0-alpha.1', -1],
    ['2.6.0+build.1', '2.6.0+build.2', 0], ['99999999999999999.0.0', '99999999999999998.0.0', 1],
  ]) assert.equal(compareVersions(a, b), expected, `${a} vs ${b}`);
  for (const invalid of ['v2.6.0', '2.6', '02.6.0', '2.6.0-01', '2.6.0-', '2.6.0\n', '2.6.0/../../x'])
    assert.equal(compareVersions(invalid, '2.5.0'), null, invalid);
});

test('checks only the fixed public repository and validates the installer release contract', async () => {
  let supplied, progress;
  const service = createUpdateService({ currentVersion: '2.5.0', fetchImpl: async (url, options) => {
    assert.equal(url, 'https://api.github.com/repos/LiukerSun/DouyinDanmu/releases/latest');
    assert.equal(options.redirect, 'error'); assert.equal(options.headers.Authorization, undefined);
    assert(options.signal instanceof AbortSignal);
    return response(release());
  }, installRelease: async (value, onProgress) => { supplied = value; progress = onProgress; await new Promise(() => {}); } });
  const checked = await service.check();
  assert.equal(checked.checking, false); assert.equal(checked.update_available, true); assert.equal(checked.can_install, true);
  assert.equal(checked.release_url, 'https://github.com/LiukerSun/DouyinDanmu/releases/tag/v2.6.0');
  assert.equal(checked.release_notes, '<script>plain release notes</script>');
  assert.equal(service.install().install.phase, 'downloading');
  assert.throws(() => service.install(), error => error.statusCode === 409);
  await tick();
  assert.equal(supplied.asset.sha256, sha256); assert.equal(supplied.version, '2.6.0'); assert.equal(supplied.tag, 'v2.6.0');
  progress({ phase: 'verifying', downloaded_bytes: 100, total_bytes: 100 });
  assert.equal(service.getStatus().install.phase, 'verifying');
  const copy = service.getStatus(); copy.install.phase = 'idle';
  assert.equal(service.getStatus().install.phase, 'verifying', 'Callers cannot mutate internal state');
});

test('checks deduplicate, cache successful results and revalidate with ETag', async () => {
  let clock = 0, calls = 0, respond;
  const service = createUpdateService({ currentVersion: '2.5.0', now: () => clock, fetchImpl: async (_, options) => {
    calls++;
    if (calls === 1) return await new Promise(resolve => { respond = resolve; });
    assert.equal(options.headers['If-None-Match'], '"release-a"');
    return new Response(null, { status: 304 });
  } });
  const first = service.check(); assert.equal(service.getStatus().checking, true);
  const duplicate = service.check({ force: true }); assert.equal(first, duplicate);
  respond(response(release(), { etag: '"release-a"' })); await first;
  clock = 5000; await service.check({ force: true }); assert.equal(calls, 1, 'Manual requests also have a minimum retry interval');
  clock = 61000; await service.check(); assert.equal(calls, 1, 'Normal polling uses the six-hour cache');
  await service.check({ force: true }); assert.equal(calls, 2);
  clock += 6 * 60 * 60 * 1000; const current = await service.check();
  assert.equal(calls, 3); assert.equal(current.latest_version, '2.6.0'); assert.equal(current.checked_at, clock);
});

test('network errors and rate limits preserve the last checked result and bound retries', async () => {
  let clock = 0, calls = 0;
  const service = createUpdateService({ currentVersion: '2.5.0', now: () => clock, fetchImpl: async () => {
    calls++;
    if (calls === 1) return response(release());
    if (calls === 2) return new Response('{}', { status: 429, headers: { 'retry-after': '120' } });
    throw new TypeError('fetch failed');
  } });
  await service.check(); clock = 61000;
  let status = await service.check({ force: true }); assert.match(status.error, /频率/); assert.equal(status.latest_version, '2.6.0');
  clock += 60000; await service.check({ force: true }); assert.equal(calls, 2);
  clock += 61000; status = await service.check({ force: true }); assert.match(status.error, /网络/); assert.equal(calls, 3);
  await service.check({ force: true }); assert.equal(calls, 3);
});

test('drafts, prereleases, older versions and invalid current versions never offer installation', async () => {
  for (const candidate of [{ ...release(), draft: true }, { ...release(), prerelease: true }, { ...release(), tag_name: '2.6.0' },
    release('2.6.0-beta.1'), release('2.6.0+build.1'), release('65536.0.0'), release('2.4.0'), release('2.5.0')]) {
    const service = createUpdateService({ currentVersion: '2.5.0', fetchImpl: async () => response(candidate), installRelease: async () => {} });
    const status = await service.check(); assert.equal(status.update_available, false); assert.equal(status.can_install, false);
    assert.throws(() => service.install(), error => error.statusCode === 409);
  }
  const unknown = createUpdateService({ currentVersion: 'dev', fetchImpl: async () => assert.fail('Unknown versions must fail closed') });
  assert.match((await unknown.check()).error, /当前版本/);
});

test('missing digests, foreign asset URLs and ambiguous assets keep the GitHub fallback only', async () => {
  for (const modify of [
    value => { delete value.assets[0].digest; },
    value => { value.assets[0].browser_download_url = 'https://evil.invalid/update.zip'; },
    value => { value.assets[0].digest = 'sha256:bad'; },
    value => { value.assets.push({ ...value.assets[0] }); },
    value => { value.assets[0].size = 513 * 1024 * 1024; },
  ]) {
    const candidate = release(); modify(candidate);
    const service = createUpdateService({ currentVersion: '2.5.0', fetchImpl: async () => response(candidate), installRelease: async () => assert.fail('Unverified packages must not be installed') });
    const status = await service.check(); assert.equal(status.update_available, true); assert.equal(status.can_install, false);
    assert.throws(() => service.install(), error => error.statusCode === 409);
  }
  const container = createUpdateService({ currentVersion: '2.5.0', fetchImpl: async () => response(release()) });
  assert.equal((await container.check()).can_install, false); assert.throws(() => container.install(), error => error.statusCode === 409);
});

test('installer failures expose a retryable error without losing release details', async () => {
  let calls = 0;
  const service = createUpdateService({ currentVersion: '2.5.0', fetchImpl: async () => response(release()), installRelease: async () => {
    if (++calls === 1) throw new Error('校验失败');
  } });
  await service.check(); service.install(); await tick();
  assert.equal(service.getStatus().install.phase, 'error'); assert.equal(service.getStatus().install.error, '校验失败');
  assert.equal(service.getStatus().can_install, true);
  service.install(); await tick(); assert.equal(service.getStatus().install.phase, 'restarting');
});

test('oversized responses are rejected before JSON parsing', async () => {
  const service = createUpdateService({ currentVersion: '2.5.0', fetchImpl: async () => new Response('x'.repeat(1024 * 1024 + 1)) });
  assert.match((await service.check()).error, /过大/);
});
