'use strict';
const { gzipSync, gunzipSync } = require('node:zlib');

// Match the backend's body storage contract. SQLite exposes BLOBs as Uint8Array,
// while legacy/small bodies remain TEXT; queryable projections remain TEXT too.
function decodeBody(value) {
  if (value === null || typeof value === 'string') return value;
  if (!(value instanceof Uint8Array) || value.length < 2 || value[0] !== 0x1f || value[1] !== 0x8b)
    throw new Error('undecodable body');
  try { return gunzipSync(value).toString('utf8'); }
  catch (cause) { throw new Error('undecodable body', { cause }); }
}

function encodeBody(plain) {
  if (plain === null) return null;
  if (typeof plain !== 'string') throw new TypeError('Body must be JSON text');
  return process.env.COMPRESSION === 'off' || Buffer.byteLength(plain, 'utf8') < 64
    ? plain : gzipSync(plain, { level: 6 });
}

module.exports = { decodeBody, encodeBody };
