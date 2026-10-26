'use strict';
const fs = require('node:fs/promises');
const path = require('node:path');

const REPOSITORY = 'LiukerSun/DouyinDanmu';
const RELEASES_URL = `https://github.com/${REPOSITORY}/releases/latest`;
const API_URL = `https://api.github.com/repos/${REPOSITORY}/releases/latest`;
const ASSET_NAME = 'DouyinDanmu-win-x64-portable.zip';
const CACHE_MS = 6 * 60 * 60 * 1000, RETRY_MS = 60 * 1000;
const INSTALL_PHASES = new Set(['downloading', 'verifying', 'staging', 'restarting']);

function parseVersion(value) {
  if (typeof value !== 'string' || value.length > 128) return null;
  const match = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$/.exec(value);
  if (!match) return null;
  const prerelease = match[4] ? match[4].split('.') : [];
  if (prerelease.some(part => /^\d+$/.test(part) && part.length > 1 && part[0] === '0')) return null;
  return { core: match.slice(1, 4).map(BigInt), prerelease };
}

function compareVersions(left, right) {
  const a = parseVersion(left), b = parseVersion(right);
  if (!a || !b) return null;
  for (let i = 0; i < 3; i++) if (a.core[i] !== b.core[i]) return a.core[i] > b.core[i] ? 1 : -1;
  if (!a.prerelease.length || !b.prerelease.length) return a.prerelease.length === b.prerelease.length ? 0 : a.prerelease.length ? -1 : 1;
  for (let i = 0; i < Math.max(a.prerelease.length, b.prerelease.length); i++) {
    const x = a.prerelease[i], y = b.prerelease[i];
    if (x === undefined || y === undefined) return x === undefined ? -1 : 1;
    if (x === y) continue;
    const nx = /^\d+$/.test(x), ny = /^\d+$/.test(y);
    if (nx && ny) return BigInt(x) > BigInt(y) ? 1 : -1;
    if (nx !== ny) return nx ? -1 : 1;
    return x > y ? 1 : -1;
  }
  return 0;
}

async function readCurrentVersion() {
  // Development keeps VERSION beside auth/. The Docker image copies it into /app.
  for (const filename of [path.join(__dirname, '..', 'VERSION'), path.join(__dirname, 'VERSION')]) {
    try {
      const value = (await fs.readFile(filename, 'utf8')).trim();
      if (parseVersion(value)) return value;
    } catch (error) { if (error.code !== 'ENOENT') throw error; }
  }
  return 'unknown';
}

function normalizeRelease(value) {
  const tag = typeof value?.tag_name === 'string' ? value.tag_name : '';
  // Published desktop packages share the build tool's three-part Windows file
  // version contract, even though comparison also understands SemVer prereleases.
  const components = /^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.exec(tag);
  const version = tag.slice(1);
  const parsed = parseVersion(version);
  if (!components || components.slice(1).some(part => BigInt(part) > 65535n) || !parsed || parsed.prerelease.length || value.draft !== false || value.prerelease !== false)
    throw new Error('GitHub 未返回可用的正式版本');
  const url = `https://github.com/${REPOSITORY}/releases/tag/${encodeURIComponent(tag)}`;
  const assetUrl = `https://github.com/${REPOSITORY}/releases/download/${encodeURIComponent(tag)}/${ASSET_NAME}`;
  const candidates = Array.isArray(value.assets) ? value.assets.filter(item => item?.name === ASSET_NAME) : [];
  const candidate = candidates.length === 1 ? candidates[0] : null;
  const sha256 = typeof candidate?.digest === 'string' && /^sha256:[a-f0-9]{64}$/i.test(candidate.digest) ? candidate.digest.slice(7).toLowerCase() : null;
  const asset = candidate?.browser_download_url === assetUrl && candidate.state === 'uploaded' && sha256 &&
    Number.isSafeInteger(candidate.size) && candidate.size > 0 && candidate.size <= 512 * 1024 * 1024
    ? { name: ASSET_NAME, url: assetUrl, size: candidate.size, sha256 } : null;
  return { version, tag, url, notes: typeof value.body === 'string' ? value.body.slice(0, 100000) : '',
    published_at: typeof value.published_at === 'string' && Number.isFinite(Date.parse(value.published_at)) ? value.published_at : null, asset };
}

async function responseJson(response) {
  let size = 0; const chunks = [];
  for await (const chunk of response.body) {
    size += chunk.length;
    if (size > 1024 * 1024) throw new Error('GitHub 版本信息过大');
    chunks.push(chunk);
  }
  return JSON.parse(Buffer.concat(chunks).toString('utf8'));
}

function conflict(message) { return Object.assign(new Error(message), { statusCode: 409 }); }

function createUpdateService({ currentVersion, installRelease = null, fetchImpl = fetch, now = Date.now } = {}) {
  const knownVersion = parseVersion(currentVersion);
  let release = null, checkedAt = null, lastAttempt = -Infinity, retryAt = 0, etag = '', checking = null;
  let error = knownVersion ? null : '无法识别当前版本，请前往 GitHub 下载正式版本';
  let installState = { phase: 'idle', downloaded_bytes: 0, total_bytes: 0, error: null };
  let installId = 0;
  const getStatus = () => {
    const available = !!release && compareVersions(release.version, currentVersion) === 1;
    return { current_version: currentVersion || 'unknown', latest_version: release?.version || null,
      update_available: available, can_install: !!installRelease && available && !!release.asset && !INSTALL_PHASES.has(installState.phase),
      release_url: release?.url || RELEASES_URL, release_notes: release?.notes || '', published_at: release?.published_at || null,
      checked_at: checkedAt, checking: checking !== null, error, install: { ...installState } };
  };

  const check = ({ force = false } = {}) => {
    if (checking) return checking;
    const clock = now();
    if (!knownVersion || clock < retryAt || clock - lastAttempt < RETRY_MS ||
        (!force && checkedAt !== null && clock - checkedAt < CACHE_MS)) return Promise.resolve(getStatus());
    lastAttempt = clock;
    checking = (async () => {
      try {
        const headers = { Accept: 'application/vnd.github+json', 'X-GitHub-Api-Version': '2026-03-10', 'User-Agent': 'DouyinDanmu-update-check' };
        if (etag) headers['If-None-Match'] = etag;
        const response = await fetchImpl(API_URL, { headers, redirect: 'error', signal: AbortSignal.timeout(8000) });
        if (response.status === 304 && release) { checkedAt = now(); error = null; retryAt = 0; return; }
        if (!response.ok) {
          if (response.status === 403 || response.status === 429) {
            const seconds = Number(response.headers.get('retry-after'));
            const reset = Number(response.headers.get('x-ratelimit-reset')) * 1000;
            retryAt = now() + Math.min(24 * 60 * 60 * 1000, Math.max(RETRY_MS, Number.isFinite(seconds) ? seconds * 1000 : 0, Number.isFinite(reset) ? reset - now() : 0));
            throw new Error('GitHub 检查频率受限，请稍后重试或前往发布页面');
          }
          throw new Error(response.status === 404 ? '暂未找到正式发布版本，请稍后重试' : '无法读取 GitHub 版本信息，请稍后重试');
        }
        const next = normalizeRelease(await responseJson(response));
        release = next; checkedAt = now(); error = null; retryAt = 0;
        etag = response.headers.get('etag') || '';
      } catch (failure) {
        error = failure.name === 'TimeoutError' || failure.name === 'AbortError' ? '检查更新超时，请稍后重试或前往 GitHub' :
          failure instanceof TypeError ? '无法连接 GitHub，请检查网络后重试' : String(failure.message || '检查更新失败').slice(0, 300);
        retryAt = Math.max(retryAt, now() + RETRY_MS);
      }
    })().finally(() => { checking = null; }).then(getStatus);
    return checking;
  };

  const install = () => {
    if (INSTALL_PHASES.has(installState.phase)) throw conflict('更新已在进行中，请等待完成');
    if (!installRelease) throw conflict('当前运行方式不支持一键更新，请前往 GitHub 查看更新说明');
    if (!getStatus().update_available) throw conflict('没有可安装的新版本，请先检查更新');
    if (!release.asset) throw conflict('发布包缺少可靠的校验信息，请前往 GitHub 下载');
    const selected = structuredClone(release), id = ++installId;
    installState = { phase: 'downloading', downloaded_bytes: 0, total_bytes: selected.asset.size, error: null };
    // Reserve synchronously before yielding; several clicks cannot launch parallel installers.
    void Promise.resolve().then(() => installRelease(selected, progress => {
      if (id !== installId || !INSTALL_PHASES.has(progress?.phase)) return;
      const total = Number.isSafeInteger(progress.total_bytes) && progress.total_bytes >= 0 ? progress.total_bytes : installState.total_bytes;
      const downloaded = Number.isSafeInteger(progress.downloaded_bytes) && progress.downloaded_bytes >= 0 ? Math.min(progress.downloaded_bytes, total) : installState.downloaded_bytes;
      installState = { phase: progress.phase, downloaded_bytes: downloaded, total_bytes: total, error: null };
    })).then(() => { if (id === installId) installState = { ...installState, phase: 'restarting' }; }).catch(failure => {
      if (id === installId) installState = { ...installState, phase: 'error', error: String(failure.message || '更新失败，请重试或前往 GitHub').slice(0, 400) };
    });
    return getStatus();
  };
  return { getStatus, check, install };
}

module.exports = { createUpdateService, readCurrentVersion, compareVersions };
