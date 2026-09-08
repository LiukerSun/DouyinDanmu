const test = require('node:test');
const assert = require('node:assert/strict');
const { createCookieSource, mergeCookieHeaders, hasTtwid } = require('./auth-cookie');

test('configured Cookie wins on collisions and preserves equals signs inside values', () => {
  const merged = mergeCookieHeaders('ttwid=anonymous; first=1', '\uFEFFCookie: ttwid=account; sessionid=a=b=c; first=2;\r\n');
  assert.equal(merged, 'ttwid=account; first=2; sessionid=a=b=c');
  assert.equal(hasTtwid(merged), true);
  assert.equal(hasTtwid('sessionid=fixture'), false);
});

test('missing and empty files stay anonymous without exposing the secret path', () => {
  for (const readFile of [() => '', () => ' \r\n', () => { throw Object.assign(new Error('fixture secret path'), { code: 'ENOENT' }); }]) {
    const source = createCookieSource({ readFile });
    assert.equal(source.read(), '');
    assert.deepEqual(source.status(), { auth_mode: 'anonymous', auth_status: 'anonymous' });
  }
});

test('status reports configuration presence, not verified login, and never reveals Cookie', () => {
  let content = 'sessionid=test-secret-value';
  const source = createCookieSource({ readFile: () => content });
  assert.deepEqual(source.status(), { auth_mode: 'authenticated', auth_status: 'configured' });
  assert.equal(JSON.stringify(source.status()).includes('test-secret-value'), false);
  content = '';
  assert.equal(source.status().auth_mode, 'anonymous');
});

test('malformed values and read failures produce only sanitized diagnostics', () => {
  for (const content of ['sessionid=secret\r\nX-Header: secret', 'secret-without-equals', 'sessionid=' + 'x'.repeat(32768), 'bad key=secret']) {
    const source = createCookieSource({ readFile: () => content });
    assert.throws(() => source.read(), error => !error.message.includes('secret') && /配置格式/.test(error.message));
    assert.deepEqual(source.status(), { auth_mode: 'anonymous', auth_status: 'configuration_error' });
  }
  const source = createCookieSource({ readFile: () => { throw Object.assign(new Error('private path and credential'), { code: 'EACCES' }); } });
  assert.throws(() => source.read(), error => error.message === '无法读取抖音 Cookie 配置文件');
  assert.equal(source.status().auth_status, 'configuration_error');
});
