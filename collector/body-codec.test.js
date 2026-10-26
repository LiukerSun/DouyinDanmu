'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { gzipSync } = require('node:zlib');
const { decodeBody, encodeBody } = require('./body-codec');

test('body codec follows byte threshold and compression switch without limiting existing large bodies', () => {
  const previousCompression = process.env.COMPRESSION;
  try {
    process.env.COMPRESSION = 'on';
    assert.equal(encodeBody('x'.repeat(63)), 'x'.repeat(63));
    for (const plain of ['x'.repeat(64), '测'.repeat(22), JSON.stringify({ content: 'x'.repeat(17 * 1024 * 1024) })]) {
      const stored = encodeBody(plain);
      assert.ok(stored instanceof Uint8Array);
      assert.equal(decodeBody(new Uint8Array(stored)), plain);
    }
    process.env.COMPRESSION = 'off';
    assert.equal(encodeBody('x'.repeat(128)), 'x'.repeat(128));
    assert.equal(decodeBody(gzipSync('existing body')), 'existing body');
    assert.equal(decodeBody('legacy text'), 'legacy text');
    assert.equal(decodeBody(null), null);
    for (const stored of [Buffer.from([1, 2, 3]), Buffer.from([31, 139, 0]), gzipSync('body').subarray(0, 12)])
      assert.throws(() => decodeBody(stored), /undecodable body/);
  } finally {
    if (previousCompression === undefined) delete process.env.COMPRESSION; else process.env.COMPRESSION = previousCompression;
  }
});
