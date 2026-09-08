const test = require('node:test');
const assert = require('node:assert/strict');
const { createRoomInfoClient: createClient, parseRoomApi, parseRoomPage } = require('./room-info');
const { createCookieSource } = require('./auth-cookie');
// Injected cookie sources keep unit tests independent of runtime credentials.
const createRoomInfoClient = ({ cookieSource = createCookieSource({ readFile: () => '' }), ...options } = {}) =>
  createClient({ roomCookies: { forRoom: () => cookieSource }, ...options });

const LIVE_ID = '100000000001', ROOM_ID = '7000000000000000001', USER_ID = '9000000000000000001';
const checkedAt = 1700000000123;
// Synthetic SSR room metadata fixtures.
const offline = {
  room: { id_str: ROOM_ID, status: 4, status_str: '4', title: '测试直播间 A' },
  roomId: ROOM_ID, web_rid: LIVE_ID,
  anchor: { id_str: USER_ID, nickname: '测试主播 A', avatar_thumb: { url_list: ['https://p11.douyinpic.com/example-avatar.jpeg'] } },
};
function page(info, extra = {}) {
  const flight = 'd:' + JSON.stringify(['$', '$L12', null, { state: { roomStore: { roomInfo: info }, ...extra } }]) + '\n';
  const cut = Math.floor(flight.length / 2);
  return [flight.slice(0, cut), flight.slice(cut)].map(chunk => '<script nonce="fixture">self.__pace_f.push(' + JSON.stringify([1, chunk]) + ')</script>').join('');
}
function api(room, user) { return JSON.stringify({ status_code: 0, data: { data: [room], user } }); }
function response(body, status = 200, cookie = false) {
  return new Response(body, { status, headers: cookie ? { 'Set-Cookie': 'ttwid=test-fixture; Path=/; HttpOnly' } : {} });
}

test('SSR shape: explicit status 4 means offline and missing optional fields stay empty', () => {
  const info = parseRoomPage(page(offline), LIVE_ID, checkedAt);
  assert.equal(info.live_status, 'offline');
  assert.equal(info.room_id, ROOM_ID);
  assert.equal(info.title, offline.room.title);
  assert.equal(info.anchor.id, USER_ID);
  assert.equal(info.anchor.nickname, offline.anchor.nickname);
  assert.equal(info.anchor.follower_count, null);
  assert.equal(info.live_started_at_ms, null);
  assert.equal(info.checked_at_ms, checkedAt);
});

test('live SSR accepts byte-length Flight text records immediately followed by room data', () => {
  const liveId = '100000000002';
  const roomInfo = {
    room: { id_str: '7000000000000000002', status: 2, title: '测试直播间 B' },
    roomId: '7000000000000000002', web_rid: liveId,
    anchor: { id_str: '9000000000000000002', nickname: '测试主播 B' },
  };
  // Adjacent Flight text records use UTF-8 byte lengths.
  // Include Chinese and fake record syntax in the text to catch character counting
  // or newline scanning that accidentally treats arbitrary text as room metadata.
  const streamText = JSON.stringify({ stream: '中文直播数据', raw: '\nd:["roomStore"]' });
  const textRecord = '13:T' + Buffer.byteLength(streamText).toString(16) + ',' + streamText;
  const secondText = '中文';
  const flight = textRecord + '14:T' + Buffer.byteLength(secondText).toString(16) + ',' + secondText
    + 'd:' + JSON.stringify(['$', '$L12', null, { state: { roomStore: { roomInfo } } }]) + '\n';
  const split = Math.floor(flight.length / 2);
  const html = [flight.slice(0, split), flight.slice(split)].map(chunk => '<script>self.__pace_f.push(' + JSON.stringify([1, chunk]) + ')</script>').join('');
  const info = parseRoomPage(html, liveId, checkedAt);
  assert.equal(info.room_id, roomInfo.roomId);
  assert.equal(info.live_status, 'live');
  assert.equal(info.anchor.nickname, roomInfo.anchor.nickname);
  assert.throws(() => parseRoomPage(html, LIVE_ID), /该直播间/);
});

test('live JSON room preserves bare 64-bit IDs and supported anchor fields', () => {
  const raw = '{"status_code":0,"data":{"data":[{"id":7000000000000000001,"status":2,"title":"直播中","start_time":1700000000,"owner":{"id":9000000000000000001,"nickname":"主播","sec_uid":"sec-example","signature":"你好","display_id":"host123","follow_info":{"follower_count":1234},"avatar_medium":{"url_list":["https://example.com/avatar.png"]}}}]}}';
  const info = parseRoomApi(raw, LIVE_ID, checkedAt);
  assert.equal(info.live_status, 'live');
  assert.equal(info.room_id, ROOM_ID);
  assert.equal(info.anchor.id, USER_ID);
  assert.equal(info.anchor.display_id, 'host123');
  assert.equal(info.anchor.signature, '你好');
  assert.equal(info.anchor.follower_count, 1234);
  assert.equal(info.live_started_at_ms, 1700000000000);
});

test('matches the requested room store and rejects recommendation-only pages', () => {
  const recommendation = { ...offline, web_rid: '12345', room: { ...offline.room, status: 2, title: '其他房间' } };
  const info = parseRoomPage(page(offline, { recommendations: [{ roomStore: { roomInfo: recommendation } }] }), LIVE_ID, checkedAt);
  assert.equal(info.title, offline.room.title);
  assert.equal(info.live_status, 'offline');
  assert.throws(() => parseRoomPage(page(recommendation), LIVE_ID), /该直播间/);
});

test('unrecognized or absent status is unknown, never inferred from stream or online counts', () => {
  for (const status of [undefined, 0, 1, 3, 99]) {
    const info = parseRoomApi(api({ id_str: ROOM_ID, status, user_count: 0, stream_url: {} }), LIVE_ID, checkedAt);
    assert.equal(info.live_status, 'unknown');
  }
  assert.throws(() => parseRoomApi('{"status_code":0,"data":{"data":[]}}', LIVE_ID), /唯一房间/);
  assert.throws(() => parseRoomApi(api({ id_str: ROOM_ID, web_rid: '12345' }), LIVE_ID), /不匹配/);
});

test('public API empty response falls back to SSR and reuses cookie within TTL', async () => {
  const urls = [];
  const fetchRoomInfo = createRoomInfoClient({ now: () => checkedAt, fetchImpl: async (url, options) => {
    urls.push(url);
    if (url === 'https://live.douyin.com/') return response('<html>Home</html>', 200, true);
    assert.equal(options.headers.Cookie, 'ttwid=test-fixture');
    return response(url.includes('/webcast/') ? '' : page(offline));
  } });
  for (let n = 0; n < 2; n++) {
    const result = await fetchRoomInfo(LIVE_ID);
    assert.equal(result.roomId, ROOM_ID);
    assert.equal(result.metadata.live_status, 'offline');
    assert.equal(result.metadata.checked_at_ms, checkedAt);
    assert.equal(JSON.stringify(result.metadata).includes('ttwid'), false);
  }
  assert.equal(urls.filter(url => url === 'https://live.douyin.com/').length, 1);
});

test('network failure, HTTP error, and invalid pages reject instead of returning offline', async () => {
  for (const failure of ['network', 'http', 'page']) {
    const fetchRoomInfo = createRoomInfoClient({ fetchImpl: async url => {
      if (url === 'https://live.douyin.com/') return response('<html>Home</html>', 200, true);
      if (failure === 'network') throw new Error('example credential must not leak');
      if (failure === 'http') return response('', 503);
      return response('<html>Verification required</html>');
    } });
    await assert.rejects(fetchRoomInfo(LIVE_ID), failure === 'network' ? /请求失败或超时/ : failure === 'http' ? /HTTP 503/ : /可验证资料/);
  }
});

test('bootstrap failure retries on the next call and invalid IDs never make requests', async () => {
  let calls = 0;
  const fetchRoomInfo = createRoomInfoClient({ fetchImpl: async url => {
    calls++;
    if (calls === 1) return response('');
    if (url === 'https://live.douyin.com/') return response('<html>Home</html>', 200, true);
    return response(api({ id_str: ROOM_ID, status: 2 }));
  } });
  await assert.rejects(fetchRoomInfo('https://example.com'), /数字字符串/);
  assert.equal(calls, 0);
  await assert.rejects(fetchRoomInfo(LIVE_ID), /响应为空/);
  assert.equal((await fetchRoomInfo(LIVE_ID)).metadata.live_status, 'live');
});

test('configured complete Cookie reaches HTTP and the returned WS credential, never metadata', async () => {
  const fixtureCookie = 'ttwid=account-fixture; sessionid=session-fixture; token=encoded=value';
  const cookieSource = createCookieSource({ readFile: () => fixtureCookie });
  const urls = [];
  const fetchRoomInfo = createRoomInfoClient({ cookieSource, fetchImpl: async (url, options) => {
    urls.push(url);
    assert.equal(options.headers.Cookie, fixtureCookie);
    assert.equal(options.redirect, 'error');
    return response(url.includes('/webcast/') ? '' : page(offline));
  } });
  const result = await fetchRoomInfo(LIVE_ID);
  assert.equal(result.cookie, fixtureCookie);
  assert.equal(urls.length, 2);
  assert.equal(urls.includes('https://live.douyin.com/'), false);
  assert.equal(JSON.stringify(result.metadata).includes('session-fixture'), false);
  assert.deepEqual(cookieSource.status(), { auth_mode: 'authenticated', auth_status: 'configured' });
});

test('Cookie without ttwid merges the homepage visitor token and removal restores anonymous access', async () => {
  let fixtureCookie = 'sessionid=account-one', homeCount = 0;
  const cookieSource = createCookieSource({ readFile: () => fixtureCookie });
  const fetchRoomInfo = createRoomInfoClient({ cookieSource, fetchImpl: async (url, options) => {
    if (url === 'https://live.douyin.com/') {
      homeCount++;
      assert.equal(options.headers.Cookie || '', fixtureCookie);
      return response('<html>Home</html>', 200, true);
    }
    assert.equal(options.headers.Cookie, 'ttwid=test-fixture' + (fixtureCookie ? '; ' + fixtureCookie : ''));
    return response(api({ id_str: ROOM_ID, status: 2 }));
  } });
  assert.equal((await fetchRoomInfo(LIVE_ID)).cookie, 'ttwid=test-fixture; sessionid=account-one');
  fixtureCookie = 'sessionid=account-two';
  assert.equal((await fetchRoomInfo(LIVE_ID)).cookie, 'ttwid=test-fixture; sessionid=account-two');
  fixtureCookie = '';
  assert.equal((await fetchRoomInfo(LIVE_ID)).cookie, 'ttwid=test-fixture');
  assert.equal(homeCount, 3);
  assert.equal(cookieSource.status().auth_status, 'anonymous');
});

test('configured-cookie transport errors are sanitized and invalid secrets never leave the process', async () => {
  const secret = 'sessionid=private-test-fixture; ttwid=account-fixture';
  const fetchRoomInfo = createRoomInfoClient({ cookieSource: createCookieSource({ readFile: () => secret }), fetchImpl: async () => {
    throw new Error('transport error with ' + secret);
  } });
  await assert.rejects(fetchRoomInfo(LIVE_ID), error => {
    assert.equal(error.message.includes('private-test-fixture'), false);
    return /请求失败或超时/.test(error.message);
  });
  const brokenBody = createRoomInfoClient({ cookieSource: createCookieSource({ readFile: () => secret }), fetchImpl: async () => ({
    ok: true, text: async () => { throw new Error('body reader ' + secret); },
  }) });
  await assert.rejects(brokenBody(LIVE_ID), error => !error.message.includes('private-test-fixture') && /响应读取失败/.test(error.message));
  let calls = 0;
  const invalid = createRoomInfoClient({ cookieSource: createCookieSource({ readFile: () => secret + '\r\nX-Extra: injected' }), fetchImpl: async () => { calls++; } });
  await assert.rejects(invalid(LIVE_ID), /Cookie 配置格式不正确/);
  assert.equal(calls, 0);
});

test('simultaneous room HTTP and WS credentials and visitor bootstrap caches are isolated', async () => {
  const values = new Map([['111', 'sessionid=account-a'], ['222', 'sessionid=account-b'], ['333', '']]);
  const counts = new Map();
  const roomCookies = { forRoom: id => ({ read: () => values.get(id) || '' }) };
  const fetchRoomInfo = createClient({ roomCookies, fetchImpl: async (url, options) => {
    if (url === 'https://live.douyin.com/') {
      const configured = options.headers.Cookie || '';
      const count = (counts.get(configured) || 0) + 1;
      counts.set(configured, count);
      // Yield so concurrent rooms exercise independent pending bootstrap requests.
      await new Promise(resolve => setImmediate(resolve));
      return new Response('<html>home</html>', { headers: { 'Set-Cookie': `ttwid=visitor-${configured.slice(10) || 'guest'}-${count}; Path=/` } });
    }
    const id = new URL(url).searchParams.get('web_rid');
    const configured = values.get(id);
    assert.equal(options.headers.Cookie.includes('sessionid='), Boolean(configured));
    if (configured) assert.ok(options.headers.Cookie.endsWith(configured));
    return response(api({ id_str: id + '0000', web_rid: id, status: 2 }));
  } });
  const ids = ['111', '222', '333'];
  const first = await Promise.all(ids.map(fetchRoomInfo));
  assert.equal(new Set(first.map(result => result.cookie)).size, 3);
  assert.equal(first[0].cookie, 'ttwid=visitor-account-a-1; sessionid=account-a');
  assert.equal(first[1].cookie, 'ttwid=visitor-account-b-1; sessionid=account-b');
  assert.equal(first[2].cookie, 'ttwid=visitor-guest-1');
  const second = await Promise.all(ids.map(fetchRoomInfo));
  assert.deepEqual(second.map(r => r.cookie), first.map(r => r.cookie));
  assert.equal([...counts.values()].reduce((a, b) => a + b), 3);
  values.set('111', ''); fetchRoomInfo.clearCookieCache('111');
  const cleared = await Promise.all(ids.map(fetchRoomInfo));
  assert.equal(cleared[0].cookie, 'ttwid=visitor-guest-2');
  assert.equal(cleared[1].cookie, first[1].cookie);
  assert.equal(cleared[2].cookie, first[2].cookie);
  // Even two anonymous rooms must retain different visitor credentials.
  assert.notEqual(cleared[0].cookie, cleared[2].cookie);
});
