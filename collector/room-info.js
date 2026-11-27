const { roomCookies: defaultRoomCookies, mergeCookieHeaders, hasTtwid } = require('./auth-cookie');
const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36';
const ORIGIN = 'https://live.douyin.com';
const roomAddressId = /^\d{1,32}$/;

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

function normalize(info, checkedAt, allowMissingRoomId = false) {
  const room = info.room || {}, anchor = info.anchor || room.owner || {};
  const roomId = idValue(room.id_str) || idValue(info.roomId) || idValue(room.id);
  if (!roomId && !allowMissingRoomId) throw new Error('抖音未返回有效房间 ID');
  const status = String(room.status ?? room.status_str ?? '');
  const started = nonnegativeNumber(room.start_time ?? room.live_start_time);
  return {
    room_id: roomId,
    title: textValue(room.title),
    // Only explicit server room states are authoritative; missing data is not offline.
    live_status: status === '2' ? 'live' : status === '4' ? 'offline' : 'unknown',
    anchor: {
      id: idValue(anchor.id_str) || idValue(anchor.id) || idValue(anchor.uid),
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

function parseAnchorProfile(body, anchorId, checkedAt, step = {}) {
  const { status_code, user } = parseLosslessJson(body);
  step.apiStatus = Number.isSafeInteger(status_code) ? status_code : null;
  step.anchorMatches = idValue(user?.uid) === anchorId;
  if (status_code !== 0 || idValue(user?.uid) !== anchorId) return null;

  const status = String(user.live_status);
  const roomId = idValue(user.room_id_str) || idValue(user.room_id);
  // Offline anchors can still carry the room ID from their last session.
  if (status === '0') {
    const lastRoomId = roomId && roomId !== '0' ? roomId : '';
    const metadata = normalize({ room: { id_str: lastRoomId, status: 4 }, anchor: user }, checkedAt, true);
    return { metadata, webRid: '' };
  }
  if (status !== '1' || !roomId || roomId === '0') {
    throw new Error('抖音未返回可用的主播开播状态或房间 ID');
  }

  let room = {};
  if (typeof user.room_data === 'string' && user.room_data) {
    try { room = parseLosslessJson(user.room_data) || {}; } catch {}
  }
  const webRid = idValue(room.owner?.web_rid);
  const metadata = normalize({ room: { ...room, id_str: roomId, status: 2 }, anchor: user }, checkedAt);
  return { metadata, webRid: roomAddressId.test(webRid) && webRid !== anchorId ? webRid : '' };
}

function parseRoomApi(body, liveId, checkedAt = Date.now(), step = {}) {
  const payload = parseLosslessJson(body);
  step.apiStatus = Number.isSafeInteger(payload?.status_code) ? payload.status_code : null;
  step.roomCount = Array.isArray(payload?.data?.data) ? payload.data.data.length : null;
  if (payload.status_code !== 0) throw new Error('抖音房间接口未返回成功状态');
  const entries = payload.data?.data;
  if (!Array.isArray(entries) || entries.length !== 1) throw new Error('抖音房间接口未返回唯一房间');
  const room = entries[0];
  const returnedId = textValue(room.web_rid) || textValue(payload.data.web_rid);
  step.roomMatches = returnedId ? returnedId === liveId : null;
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

function parseRoomPage(html, liveId, checkedAt = Date.now(), step = {}) {
  const title = html.match(/<title\b[^>]*>([^<]{0,300})<\/title\s*>/i)?.[1];
  if (!title) step.titleHint = 'missing';
  else if (/验证|captcha|verification/i.test(title)) step.titleHint = 'verification';
  else if (/登录|登錄|log\s?in|sign\s?in/i.test(title)) step.titleHint = 'login';
  else step.titleHint = 'other';
  step.scripts = 0;
  step.recognizedScripts = 0;
  let flight = '';
  const documents = [];
  for (const script of html.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/gi)) {
    step.scripts++;
    // These arguments are JSON data. Never evaluate JavaScript downloaded from Douyin.
    const push = script[2].match(/^\s*self\.__pace_f\.push\(([\s\S]*)\);?\s*$/);
    if (push) {
      step.recognizedScripts++;
      try {
        const chunk = parseLosslessJson(push[1]);
        if (chunk[0] === 1 && typeof chunk[1] === 'string') flight += chunk[1];
      } catch {}
    } else if (/\bid\s*=\s*["'](?:RENDER_DATA|__NEXT_DATA__)["']/i.test(script[1])) {
      step.recognizedScripts++;
      try { documents.push(parseLosslessJson(script[2].trim().startsWith('%') ? decodeURIComponent(script[2]) : script[2])); } catch {}
    }
  }
  // Flight records can be split across script tags. Join the payloads before parsing.
  documents.push(...flightDocuments(flight));
  step.documents = documents.length;
  // Later SSR records contain the resolved room rather than the initial empty store.
  for (const document of documents.reverse()) {
    const info = findRoomInfo(document, liveId);
    if (info) return normalize(info, checkedAt);
  }
  throw new Error('抖音页面未返回该直播间的可验证资料');
}

const knownMimeTypes = new Set(['application/json', 'text/html', 'text/plain', 'application/octet-stream', 'application/xhtml+xml', 'text/javascript']);
const networkCodes = new Set(['ENOTFOUND', 'EAI_AGAIN', 'ECONNREFUSED', 'ECONNRESET', 'ETIMEDOUT',
  'ENETUNREACH', 'EHOSTUNREACH', 'UND_ERR_CONNECT_TIMEOUT', 'UND_ERR_HEADERS_TIMEOUT', 'UND_ERR_SOCKET',
  'CERT_HAS_EXPIRED', 'UNABLE_TO_VERIFY_LEAF_SIGNATURE']);

function createRoomInfoClient({
  fetchImpl = globalThis.fetch, now = Date.now, timeoutMs = 10000, cookieTtlMs = 20 * 60 * 1000,
  roomCookies = defaultRoomCookies, onDiagnostic = record => console.error('[room-diagnostic]', JSON.stringify(record)),
} = {}) {
  const cookieCaches = new Map();
  const lastReports = new Map();
  async function request(url, cookie, step) {
    const signal = AbortSignal.timeout(timeoutMs);
    let response;
    try {
      response = await fetchImpl(url, {
        headers: { 'User-Agent': UA, Referer: ORIGIN + '/', ...(cookie ? { Cookie: cookie } : {}) },
        signal, redirect: 'error',
      });
    } catch (error) {
      const code = error?.cause?.code ?? error?.code;
      step.result = signal.aborted ? 'timeout' : 'network_error';
      if (networkCodes.has(code)) step.networkCode = code;
      throw new Error('抖音房间资料请求失败或超时');
    }
    step.http = response.status;
    const mime = response.headers?.get('content-type')?.split(';')[0].trim().toLowerCase();
    step.contentType = mime ? (knownMimeTypes.has(mime) ? mime : 'other') : 'missing';
    if (!response.ok) {
      step.result = 'http_error';
      await response.body?.cancel().catch(() => {});
      throw new Error(`抖音房间资料请求返回 HTTP ${response.status}`);
    }
    let body;
    try { body = await response.text(); }
    catch {
      step.result = signal.aborted ? 'timeout' : 'read_error';
      throw new Error('抖音房间资料响应读取失败或超时');
    }
    step.bodyBytes = Buffer.byteLength(body);
    const firstChar = body.trimStart()[0];
    if (firstChar === '{' || firstChar === '[') step.bodyShape = 'json_like';
    else if (firstChar === '<') step.bodyShape = 'markup_like';
    else step.bodyShape = firstChar ? 'other' : 'empty';
    if (!body || body.length > 8 * 1024 * 1024) {
      step.result = body ? 'body_too_large' : 'empty_body';
      throw new Error('抖音房间资料响应为空或超限');
    }
    return { body, response };
  }
  async function readStage(step, url, cookie, parse) {
    const started = now();
    try {
      const { body, response } = await request(url, cookie, step);
      const result = parse(body, response);
      step.result = result == null ? 'no_match' : 'ok';
      return result;
    } catch (error) {
      step.result ??= error instanceof SyntaxError ? 'invalid_json' : 'invalid_metadata';
      throw error;
    } finally {
      step.elapsedMs = Math.max(0, now() - started);
    }
  }
  async function getCookie(liveId, steps) {
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
      cache.step = { stage: 'visitor_home' };
      cache.pending = readStage(cache.step, ORIGIN + '/', configured, (_body, response) => {
        const setCookie = response.headers.getSetCookie().find(value => value.startsWith('ttwid='));
        const cookie = setCookie?.split(';')[0];
        if (!cookie || cookie === 'ttwid=') {
          cache.step.result = 'visitor_cookie_missing';
          throw new Error('抖音未返回直播访问凭据，请稍后重试');
        }
        cache.cookie = mergeCookieHeaders(cookie, configured); cache.expires = now() + cookieTtlMs;
        return cache.cookie;
      }).finally(() => { cache.pending = undefined; });
    }
    steps.push(cache.step);
    return cache.pending;
  }
  async function fetchByWebRid(liveId, cookie, steps) {
    const params = new URLSearchParams({ aid: '6383', app_name: 'douyin_web', live_id: '1', device_platform: 'web', web_rid: liveId, enter_from: 'web_live', cookie_enabled: 'true' });
    const api = { stage: 'room_api' };
    steps.push(api);
    try {
      return await readStage(api, ORIGIN + '/webcast/room/web/enter/?' + params, cookie,
        body => parseRoomApi(body, liveId, now(), api));
    } catch {
      const page = { stage: 'room_page' };
      steps.push(page);
      return readStage(page, ORIGIN + '/' + liveId, cookie, body => parseRoomPage(body, liveId, now(), page));
    }
  }
  async function lookup(liveId, steps) {
    const cookie = await getCookie(liveId, steps);
    let metadata;
    try {
      metadata = await fetchByWebRid(liveId, cookie, steps);
    } catch (roomError) {
      // An anchor_id link can also be stored as a numeric monitoring ID.
      const params = new URLSearchParams({ aid: '6383', user_id: liveId });
      const step = { stage: 'anchor_profile' };
      steps.push(step);
      let profile;
      try {
        profile = await readStage(step, ORIGIN + '/aweme/v1/web/user/profile/other/?' + params, cookie,
          body => parseAnchorProfile(body, liveId, now(), step));
      } catch (error) {
        if (step.result !== 'invalid_metadata') throw roomError;
        throw new Error(error.message + '（原直播间查询失败：' + roomError.message + '）', { cause: roomError });
      }
      if (!profile) throw roomError;
      metadata = profile.metadata;
      if (profile.webRid) {
        try {
          const details = await fetchByWebRid(profile.webRid, cookie, steps);
          if (details.room_id === metadata.room_id && details.anchor.id === liveId && details.live_status === 'live') {
            metadata = details;
          }
        } catch { /* Verified profile data is sufficient to connect. */ }
      }
    }
    return { roomId: metadata.room_id, cookie, metadata };
  }
  async function fetchRoomInfo(liveId) {
    if (typeof liveId !== 'string' || !roomAddressId.test(liveId)) throw new Error('直播间 ID 必须为数字字符串');
    const steps = [];
    try {
      const room = await lookup(liveId, steps);
      lastReports.delete(liveId);
      return room;
    } catch (error) {
      const time = now();
      const previous = lastReports.get(liveId);
      if (!previous || time - previous.time >= 60000) {
        if (!steps.length) steps.push({ stage: 'local_credentials', result: 'read_error' });
        onDiagnostic({ time: new Date(time).toISOString(), liveId, suppressed: previous?.suppressed ?? 0, steps });
        lastReports.set(liveId, { time, suppressed: 0 });
      } else {
        previous.suppressed++;
      }
      throw error;
    }
  }
  fetchRoomInfo.clearCookieCache = liveId => {
    lastReports.delete(liveId);
    cookieCaches.delete(liveId);
  };
  return fetchRoomInfo;
}

const fetchRoomInfo = createRoomInfoClient();
module.exports = { fetchRoomInfo, createRoomInfoClient, parseRoomApi, parseRoomPage };
