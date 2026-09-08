const { roomCookies: defaultRoomCookies, mergeCookieHeaders, hasTtwid } = require('./auth-cookie');
const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36';
const ORIGIN = 'https://live.douyin.com';

// Protect integer tokens before JSON.parse: both room and user IDs exceed 2^53.
function parseLosslessJson(text) {
  let normalized = '', inString = false, escaped = false;
  for (let i = 0; i < text.length;) {
    const char = text[i];
    if (inString) {
      normalized += char; i++;
      if (escaped) escaped = false;
      else if (char === '\\') escaped = true;
      else if (char === '"') inString = false;
    } else if (char === '"') {
      normalized += char; inString = true; i++;
    } else if (char === '-' || /\d/.test(char)) {
      const token = text.slice(i).match(/^-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?/)?.[0];
      if (!token) { normalized += char; i++; continue; }
      normalized += /^-?\d+$/.test(token) && !Number.isSafeInteger(Number(token)) ? JSON.stringify(token) : token;
      i += token.length;
    } else { normalized += char; i++; }
  }
  return JSON.parse(normalized);
}

const textValue = value => typeof value === 'string' && !value.startsWith('$') ? value : '';
const idValue = value => typeof value === 'string' && /^\d+$/.test(value) ? value : Number.isSafeInteger(value) && value >= 0 ? String(value) : '';
function nonnegativeNumber(value) {
  if (value === undefined || value === null || value === '') return null;
  const parsed = Number(value);
  return Number.isSafeInteger(parsed) && parsed >= 0 ? parsed : null;
}
function avatarUrl(anchor) {
  for (const value of [anchor.avatar_medium, anchor.avatar_thumb, anchor.avatar_large]) {
    for (const url of value?.url_list || []) {
      try { const parsed = new URL(url); if (parsed.protocol === 'https:') return parsed.href; } catch {}
    }
  }
  return '';
}

function normalize(info, checkedAt) {
  const room = info.room || {}, anchor = info.anchor || room.owner || {};
  const roomId = idValue(room.id_str) || idValue(info.roomId) || idValue(room.id);
  if (!roomId) throw new Error('抖音未返回有效房间 ID');
  const status = String(room.status ?? room.status_str ?? '');
  const started = nonnegativeNumber(room.start_time ?? room.live_start_time);
  return {
    room_id: roomId,
    title: textValue(room.title),
    // Only explicit server room states are authoritative; missing data is not offline.
    live_status: status === '2' ? 'live' : status === '4' ? 'offline' : 'unknown',
    anchor: {
      id: idValue(anchor.id_str) || idValue(anchor.id),
      sec_uid: textValue(anchor.sec_uid),
      nickname: textValue(anchor.nickname),
      avatar_url: avatarUrl(anchor),
      signature: textValue(anchor.signature),
      display_id: textValue(anchor.display_id) || idValue(anchor.short_id),
      follower_count: nonnegativeNumber(anchor.follow_info?.follower_count ?? anchor.follower_count),
    },
    live_started_at_ms: started && Number.isSafeInteger(started * 1000) ? started * 1000 : null,
    checked_at_ms: checkedAt,
  };
}

function parseRoomApi(body, liveId, checkedAt = Date.now()) {
  const payload = parseLosslessJson(body);
  if (payload.status_code !== 0) throw new Error('抖音房间接口未返回成功状态');
  const entries = payload.data?.data;
  if (!Array.isArray(entries) || entries.length !== 1) throw new Error('抖音房间接口未返回唯一房间');
  const room = entries[0];
  const returnedId = textValue(room.web_rid) || textValue(payload.data.web_rid);
  if (returnedId && returnedId !== liveId) throw new Error('抖音房间接口返回的直播间不匹配');
  return normalize({ room, anchor: room.owner || payload.data.user }, checkedAt);
}

function findRoomInfo(root, liveId) {
  const pending = [root];
  let visited = 0;
  while (pending.length) {
    if (++visited > 100000) throw new Error('抖音房间页面数据超限');
    const node = pending.pop();
    if (!node || typeof node !== 'object') continue;
    const info = node.roomStore?.roomInfo;
    if (info && textValue(info.web_rid) === liveId && info.room) return info;
    for (const value of Object.values(node)) if (value && typeof value === 'object') pending.push(value);
  }
  return null;
}

function flightDocuments(flight) {
  const buffer = Buffer.from(flight, 'utf8'), documents = [];
  let offset = 0;
  while (offset < buffer.length) {
    if (buffer[offset] === 10) { offset++; continue; }
    const colon = buffer.indexOf(58, offset);
    if (colon < 0 || !/^[a-f\d]+$/i.test(buffer.toString('utf8', offset, colon))) break;
    if (buffer[colon + 1] === 84) {
      // React Flight T records carry raw text by UTF-8 byte length and have no
      // trailing newline. Stream configuration precedes the room JSON this way.
      const comma = buffer.indexOf(44, colon + 2);
      if (comma < 0) break;
      const hex = buffer.toString('ascii', colon + 2, comma);
      if (!/^[a-f\d]+$/i.test(hex)) break;
      const length = parseInt(hex, 16), end = comma + 1 + length;
      if (!Number.isSafeInteger(end) || end > buffer.length) break;
      offset = end;
      continue;
    }
    const newline = buffer.indexOf(10, colon + 1);
    const end = newline < 0 ? buffer.length : newline;
    const value = buffer.toString('utf8', colon + 1, end);
    if (value.startsWith('[') || value.startsWith('{')) {
      try { documents.push(parseLosslessJson(value)); } catch {}
    }
    offset = end + 1;
  }
  return documents;
}

function parseRoomPage(html, liveId, checkedAt = Date.now()) {
  let flight = '';
  const documents = [];
  for (const script of html.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/gi)) {
    // These arguments are JSON data. Never evaluate JavaScript downloaded from Douyin.
    const push = script[2].match(/^\s*self\.__pace_f\.push\(([\s\S]*)\);?\s*$/);
    if (push) {
      try {
        const chunk = parseLosslessJson(push[1]);
        if (chunk[0] === 1 && typeof chunk[1] === 'string') flight += chunk[1];
      } catch {}
    } else if (/\bid\s*=\s*["'](?:RENDER_DATA|__NEXT_DATA__)["']/i.test(script[1])) {
      try { documents.push(parseLosslessJson(script[2].trim().startsWith('%') ? decodeURIComponent(script[2]) : script[2])); } catch {}
    }
  }
  // Flight records can be split across script tags. Join the payloads before parsing.
  documents.push(...flightDocuments(flight));
  // Later SSR records contain the resolved room rather than the initial empty store.
  for (const document of documents.reverse()) {
    const info = findRoomInfo(document, liveId);
    if (info) return normalize(info, checkedAt);
  }
  throw new Error('抖音页面未返回该直播间的可验证资料');
}

function createRoomInfoClient({ fetchImpl = globalThis.fetch, now = Date.now, timeoutMs = 10000, cookieTtlMs = 20 * 60 * 1000, roomCookies = defaultRoomCookies } = {}) {
  const cookieCaches = new Map();
  async function request(url, cookie = '') {
    let response;
    try {
      response = await fetchImpl(url, {
        headers: { 'User-Agent': UA, Referer: ORIGIN + '/', ...(cookie ? { Cookie: cookie } : {}) },
        signal: AbortSignal.timeout(timeoutMs), redirect: 'error',
      });
    } catch { throw new Error('抖音房间资料请求失败或超时'); }
    if (!response.ok) {
      await response.body?.cancel().catch(() => {});
      throw new Error(`抖音房间资料请求返回 HTTP ${response.status}`);
    }
    let body;
    try { body = await response.text(); }
    catch { throw new Error('抖音房间资料响应读取失败或超时'); }
    if (!body || body.length > 8 * 1024 * 1024) throw new Error('抖音房间资料响应为空或超限');
    return { body, response };
  }
  async function getCookie(liveId) {
    const configured = roomCookies.forRoom(liveId).read();
    if (hasTtwid(configured)) return configured;
    // A replacement/removed file must not retain the prior account's cookies.
    let cache = cookieCaches.get(liveId);
    if (!cache || cache.configured !== configured) {
      cache = { configured, cookie: '', expires: 0, pending: undefined };
      cookieCaches.set(liveId, cache);
    }
    if (cache.cookie && now() < cache.expires) return cache.cookie;
    if (!cache.pending) {
      cache.pending = (async () => {
        const { response } = await request(ORIGIN + '/', configured);
        const setCookie = response.headers.getSetCookie().find(value => value.startsWith('ttwid='));
        const cookie = setCookie?.split(';')[0];
        if (!cookie || cookie === 'ttwid=') throw new Error('抖音未返回直播访问凭据，请稍后重试');
        cache.cookie = mergeCookieHeaders(cookie, configured); cache.expires = now() + cookieTtlMs;
        return cache.cookie;
      })().finally(() => { cache.pending = undefined; });
    }
    return cache.pending;
  }
  async function fetchRoomInfo(liveId) {
    if (typeof liveId !== 'string' || !/^\d{1,30}$/.test(liveId)) throw new Error('直播间 ID 必须为数字字符串');
    const cookie = await getCookie(liveId);
    const params = new URLSearchParams({ aid: '6383', app_name: 'douyin_web', live_id: '1', device_platform: 'web', web_rid: liveId, enter_from: 'web_live', cookie_enabled: 'true' });
    let metadata;
    try {
      const { body } = await request(ORIGIN + '/webcast/room/web/enter/?' + params, cookie);
      metadata = parseRoomApi(body, liveId, now());
    } catch {
      const { body } = await request(ORIGIN + '/' + liveId, cookie);
      metadata = parseRoomPage(body, liveId, now());
    }
    return { roomId: metadata.room_id, cookie, metadata };
  }
  fetchRoomInfo.clearCookieCache = liveId => cookieCaches.delete(liveId);
  return fetchRoomInfo;
}

const fetchRoomInfo = createRoomInfoClient();
module.exports = { fetchRoomInfo, createRoomInfoClient, parseRoomApi, parseRoomPage };
