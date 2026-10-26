'use strict';
const fs = require('node:fs/promises');
const path = require('node:path');
const crypto = require('node:crypto');
const { spawn } = require('node:child_process');

const MAX_DOWNLOAD = 512 * 1024 * 1024;
const RELEASE_PREFIX = 'https://github.com/LiukerSun/DouyinDanmu/releases/download/';
const REDIRECT_HOSTS = new Set(['github.com', 'release-assets.githubusercontent.com', 'objects.githubusercontent.com']);
const stableVersion = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/;

async function directory(parent, name) {
  const target = path.join(parent, name);
  await fs.mkdir(target, { recursive: false }).catch(error => { if (error.code !== 'EEXIST') throw error; });
  const stat = await fs.lstat(target);
  if (!stat.isDirectory() || stat.isSymbolicLink() || path.resolve(await fs.realpath(target)).toLowerCase() !== path.resolve(target).toLowerCase())
    throw new Error('更新目录不能是链接或重解析点');
  return target;
}

async function regularFile(file) {
  const stat = await fs.lstat(file);
  if (!stat.isFile() || stat.isSymbolicLink() || path.resolve(await fs.realpath(file)).toLowerCase() !== path.resolve(file).toLowerCase())
    throw new Error('更新文件不能是链接或目录');
  return stat;
}

async function removeOwnedDirectory(parent, target) {
  if (path.dirname(path.resolve(target)) !== path.resolve(parent) || !/^[a-f0-9]{32}$/.test(path.basename(target)))
    throw new Error('Refusing cleanup outside the owned update directory');
  const stat = await fs.lstat(target);
  if (!stat.isDirectory() || stat.isSymbolicLink() || path.resolve(await fs.realpath(target)).toLowerCase() !== path.resolve(target).toLowerCase())
    throw new Error('Refusing cleanup through a linked update directory');
  await fs.rm(target, { recursive: true, force: true });
}

async function fetchAsset(fetchImpl, initial) {
  let url = initial;
  const controller = new AbortController();
  const total = setTimeout(() => controller.abort(new Error('Update download timed out')), 15 * 60 * 1000);
  total.unref();
  let idle;
  const touch = () => { clearTimeout(idle);idle=setTimeout(() => controller.abort(new Error('Update download stalled')),30000);idle.unref(); };
  const close = () => { clearTimeout(total);clearTimeout(idle);controller.abort(); };
  touch();
  try { for (let count = 0; count < 6; count++) {
    const parsed = new URL(url);
    if (parsed.protocol !== 'https:' || parsed.username || parsed.password || parsed.port || !REDIRECT_HOSTS.has(parsed.hostname))
      throw new Error('更新下载地址不受信任');
    const response = await fetchImpl(url, { redirect: 'manual', signal: controller.signal, headers: { 'User-Agent': 'DouyinDanmu-Updater' } });
    touch();
    if ([301, 302, 303, 307, 308].includes(response.status)) {
      const location = response.headers.get('location');
      await response.body?.cancel();
      if (!location) throw new Error('更新下载重定向无效');
      url = new URL(location, url).href;
      continue;
    }
    if (!response.ok || !response.body) throw new Error('下载更新失败，请稍后重试或前往 GitHub');
    return { response, touch, close };
  }
  throw new Error('更新下载重定向次数过多');
  } catch(error) { close();throw error; }
}

function extractPackage(script, archive, destination, version) {
  const systemRoot = process.env.SystemRoot || 'C:\\Windows';
  const powershell = path.join(systemRoot, 'System32', 'WindowsPowerShell', 'v1.0', 'powershell.exe');
  return new Promise((resolve, reject) => {
    const child = spawn(powershell, ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', script,
      '-ArchivePath', archive, '-DestinationPath', destination, '-ExpectedVersion', version], { windowsHide: true, stdio: ['ignore', 'ignore', 'pipe'] });
    let errorText = '';
    child.stderr.on('data', chunk => { if (errorText.length < 8192) errorText += chunk.toString('utf8'); });
    child.once('error', reject);
    child.once('exit', code => code === 0 ? resolve() : reject(new Error('更新包校验失败：' + errorText.trim().slice(0, 1500))));
  });
}

function createPortableInstaller({ installRoot, appRoot, currentVersion, fetchImpl = fetch, onReady }) {
  let installing = false;
  return async function installRelease(release, onProgress = () => {}) {
    if (installing) throw new Error('已有更新任务正在执行');
    installing = true;
    let transaction, published, updatesRoot, versionsRoot, pendingPublished = false;
    try {
      if (!stableVersion.test(release.version || '') || !stableVersion.test(currentVersion || '') || release.tag !== 'v' + release.version)
        throw new Error('更新版本无效');
      const next = release.version.split('.').map(BigInt), current = currentVersion.split('.').map(BigInt);
      const difference = next.findIndex((part, index) => part !== current[index]);
      if (difference < 0 || next[difference] < current[difference]) throw new Error('只能安装更新的版本');
      const asset = release.asset;
      const expectedUrl = RELEASE_PREFIX + encodeURIComponent(release.tag) + '/DouyinDanmu-win-x64-portable.zip';
      if (!asset || asset.name !== 'DouyinDanmu-win-x64-portable.zip' || asset.url !== expectedUrl ||
          !Number.isSafeInteger(asset.size) || asset.size <= 0 || asset.size > MAX_DOWNLOAD || !/^[a-f0-9]{64}$/i.test(asset.sha256 || ''))
        throw new Error('更新包地址、大小或校验值无效');
      if (typeof onReady !== 'function') throw new Error('当前启动方式不支持自动更新');
      const root = await fs.realpath(path.resolve(installRoot));
      const source = await fs.realpath(path.resolve(appRoot));
      if (source !== root && !source.startsWith(path.join(root, '.versions') + path.sep)) throw new Error('程序目录不属于此安装');
      const updates = await directory(root, '.updates'), versions = await directory(root, '.versions');
      updatesRoot = updates;versionsRoot = versions;
      const pending = path.join(updates, 'pending-version');
      try { await fs.lstat(pending); throw new Error('已有更新等待重启，请先重启程序'); }
      catch (error) { if (error.code !== 'ENOENT') throw error; }
      const id = crypto.randomBytes(16).toString('hex');
      transaction = await directory(updates, id);
      const archive = path.join(transaction, 'package.zip'), staging = path.join(transaction, 'stage');
      const helper = path.join(source, 'app', 'portable', 'extract-update.ps1');
      await regularFile(helper);
      let downloaded = 0;
      const hash = crypto.createHash('sha256');
      onProgress({ phase: 'downloading', downloaded_bytes: 0, total_bytes: asset.size });
      const download = await fetchAsset(fetchImpl, asset.url);
      let file;
      try {
        const declared = download.response.headers.get('content-length');
        if (declared !== null && (!/^\d+$/.test(declared) || Number(declared) !== asset.size)) {
          await download.response.body.cancel();throw new Error('更新包大小与发布记录不一致');
        }
        file = await fs.open(archive, 'wx');
        for await (const chunk of download.response.body) {
          download.touch();
          downloaded += chunk.byteLength;
          if (downloaded > asset.size || downloaded > MAX_DOWNLOAD) throw new Error('更新包超过允许大小');
          hash.update(chunk);
          await file.writeFile(chunk);
          onProgress({ phase: 'downloading', downloaded_bytes: downloaded, total_bytes: asset.size });
        }
        await file.sync();
      } finally { download.close();await file?.close(); }
      onProgress({ phase: 'verifying', downloaded_bytes: downloaded, total_bytes: asset.size });
      if (downloaded !== asset.size || hash.digest('hex') !== asset.sha256.toLowerCase()) throw new Error('更新包校验失败，未修改当前程序');
      onProgress({ phase: 'staging', downloaded_bytes: downloaded, total_bytes: asset.size });
      await extractPackage(helper, archive, staging, release.version);
      published = path.join(versions, id);
      await fs.rename(staging, published);
      const marker = path.join(transaction, 'pending-version');
      const handle = await fs.open(marker, 'wx');
      try { await handle.writeFile(id + '\n', 'utf8');await handle.sync(); } finally { await handle.close(); }
      // The launcher owns the installation lock. Recheck before the atomic
      // rename; unlike hard links, this also works on exFAT portable drives.
      try { await fs.lstat(pending);throw new Error('已有更新等待重启，请先重启程序'); }
      catch(error) { if(error.code!=='ENOENT')throw error; }
      await fs.rename(marker, pending);
      pendingPublished = true;
      onProgress({ phase: 'restarting', downloaded_bytes: downloaded, total_bytes: asset.size });
      await onReady();
    } catch (error) {
      // Once handed off, leave the verified version and pending pointer intact.
      // A normal restart can still continue safely; no user directory is touched.
      if (!pendingPublished && published) await removeOwnedDirectory(versionsRoot, published).catch(() => {});
      throw error;
    } finally {
      if (transaction) await removeOwnedDirectory(updatesRoot, transaction).catch(() => {});
      installing = false;
    }
  };
}

module.exports = { createPortableInstaller };
