const fs = require('node:fs');
const { syncDirectory } = require('./durable-files');
const path = require('node:path');
const crypto = require('node:crypto');

const INVALID_COOKIE = '抖音 Cookie 配置格式不正确，请粘贴完整 Cookie 请求头值';
const COOKIE_FILE_ERROR = '无法读取抖音 Cookie 配置文件';
const MAX_COOKIE_LENGTH = 32768;

function cookieEntries(header) {
  const value = String(header).replace(/^\uFEFF/, '').trim().replace(/^cookie\s*:\s*/i, '');
  if (value.length > MAX_COOKIE_LENGTH || /[^\x20-\x7e]/.test(value)) throw new Error(INVALID_COOKIE);
  const entries = new Map();
  for (const part of value.split(';')) {
    const item = part.trim();
    if (!item) continue;
    const equal = item.indexOf('=');
    const name = item.slice(0, equal).trim(), content = item.slice(equal + 1).trim();
    if (equal < 1 || !/^[!#$%&'*+.^_`|~\da-z-]+$/i.test(name)) throw new Error(INVALID_COOKIE);
    entries.set(name, content);
  }
  return entries;
}

function mergeCookieHeaders(anonymous, configured) {
  // The operator's complete browser Cookie wins, especially its own ttwid.
  const entries = new Map([...cookieEntries(anonymous), ...cookieEntries(configured)]);
  return [...entries].map(([name, value]) => `${name}=${value}`).join('; ');
}

function hasTtwid(cookie) { return Boolean(cookieEntries(cookie).get('ttwid')); }

function createCookieSource({ filePath = process.env.DOUYIN_COOKIE_FILE || '/run/secrets/douyin_cookie', storePath = process.env.DOUYIN_COOKIE_STORE || '', readFile = fs.readFileSync } = {}) {
  function read() {
    let content;
    try {
      // An explicitly empty web setting means anonymous, even if a legacy
      // read-only secret exists. Never silently reactivate a cleared account.
      if (storePath) {
        try { content = readFile(storePath, 'utf8'); }
        catch (error) { if (error.code !== 'ENOENT') throw error; }
      }
      if (content === undefined) content = readFile(filePath, 'utf8');
    }
    catch (error) { if (error.code === 'ENOENT') return ''; throw new Error(COOKIE_FILE_ERROR); }
    return mergeCookieHeaders('', content);
  }
  function write(value) {
    if (typeof value !== 'string') throw new Error(INVALID_COOKIE);
    const normalized = mergeCookieHeaders('', value);
    if (!normalized && value.trim()) throw new Error(INVALID_COOKIE);
    if (!storePath) throw new Error('尚未启用网页 Cookie 存储');
    const directory = path.dirname(storePath), temporary = path.join(directory, '.' + crypto.randomUUID() + '.tmp');
    let descriptor;
    try {
      fs.mkdirSync(directory, { recursive: true, mode: 0o700 });
      descriptor = fs.openSync(temporary, 'wx', 0o600);
      fs.writeFileSync(descriptor, normalized); fs.fsyncSync(descriptor);
      fs.closeSync(descriptor); descriptor = undefined;
      fs.renameSync(temporary, storePath);
      syncDirectory(directory);
    } catch {
      throw new Error('无法保存 Cookie，请检查采集服务存储');
    } finally {
      if (descriptor !== undefined) fs.closeSync(descriptor);
      try { fs.unlinkSync(temporary); } catch {}
    }
    return status();
  }
  function status() {
    try {
      const configured = Boolean(read());
      // This is configuration presence, not a claim that Douyin accepted a login.
      return { auth_mode: configured ? 'authenticated' : 'anonymous', auth_status: configured ? 'configured' : 'anonymous' };
    } catch {
      return { auth_mode: 'anonymous', auth_status: 'configuration_error' };
    }
  }
  return { read, status, write };
}

function createRoomCookieStore({ directory = process.env.DOUYIN_ROOM_COOKIE_DIR || '/data/config/rooms' } = {}) {
  function forRoom(liveId) {
    if (typeof liveId !== 'string' || !/^\d{1,30}$/.test(liveId)) throw new Error('直播间 ID 必须为数字字符串');
    const filename = path.join(directory, liveId + '.cookie');
    // Both paths refer to this room only. Never fall back to a global account.
    return createCookieSource({ filePath: filename, storePath: filename });
  }
  return { forRoom };
}

const roomCookies = createRoomCookieStore();
module.exports = { createCookieSource, createRoomCookieStore, mergeCookieHeaders, hasTtwid, roomCookies };
