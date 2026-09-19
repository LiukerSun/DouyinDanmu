const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const crypto = require('node:crypto');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const protobuf = require('protobufjs');

const CAPACITY = 64 * 1024 * 1024;
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(predicate, message, timeout = 10000) {
  const end = Date.now() + timeout;
  while (!predicate()) {
    assert.ok(Date.now() < end, message);
    await pause(25);
  }
}
function padMessage(body, size) {
  const varint = value => {
    const bytes = [];
    do { const byte = value & 127; value >>>= 7; bytes.push(value ? byte | 128 : byte); } while (value);
    return Buffer.from(bytes);
  };
  let length = size - body.length - 5;
  for (let i = 0; i < 3; i++) length = size - body.length - 1 - varint(length).length;
  return Buffer.concat([body, Buffer.from([0x7a]), varint(length), Buffer.alloc(length)]);
}

async function fixture(t) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'collector-spool-test-'));
  const runtime = path.join(directory, 'runtime'), spool = path.join(directory, 'spool');
  fs.mkdirSync(path.join(runtime, 'proto'), { recursive: true });
  fs.mkdirSync(spool);
  for (const name of fs.readdirSync(__dirname).filter(name => name.endsWith('.js'))) {
    fs.copyFileSync(path.join(__dirname, name), path.join(runtime, name));
  }
  for (const name of ['douyin.proto', 'ingest.proto']) {
    fs.copyFileSync(path.join(__dirname, '../backend/proto', name), path.join(runtime, 'proto', name));
  }
  const root = await protobuf.load(['douyin.proto', 'ingest.proto'].map(name => path.join(runtime, 'proto', name)));
  const Envelope = root.lookupType('pipeline.RawFrameEnvelope');
  const encode = (type, value) => { const message = root.lookupType(type); return Buffer.from(message.encode(message.fromObject(value)).finish()); };
  const envelope = (payload = encode('PushFrame', { payloadType: 'hb' })) => {
    const id = crypto.randomUUID();
    return { id, body: encode('pipeline.RawFrameEnvelope', {
      schemaVersion: 1, frameId: id, liveId: 'demo', roomId: '9000000001', sessionId: 'recovery-fixture',
      sessionSeq: '1', receivedAtMs: String(Date.now()), payload,
      payloadSha256: crypto.createHash('sha256').update(payload).digest('hex'), source: 'demo', kind: 'upstream_frame', desiredVersion: '7',
    }) };
  };
  const committed = new Map(), heartbeats = [];
  let blocked = false, child, stderr = '';
  async function stop() {
    if (child?.exitCode === null) { const exited = once(child, 'exit'); child.kill('SIGKILL'); await exited; }
    child = undefined;
  }
  const server = http.createServer(async (req, res) => {
    const chunks = []; for await (const chunk of req) chunks.push(chunk);
    const body = Buffer.concat(chunks);
    res.setHeader('Content-Type', 'application/json');
    if (blocked && ['/internal/receipts', '/internal/ingest'].includes(req.url)) {
      res.writeHead(503); res.end('{}'); return;
    }
    if (req.url === '/internal/targets') res.end(JSON.stringify([{ live_id: 'demo', source: 'demo', version: 7, enabled: true }]));
    else if (req.url === '/internal/heartbeat') { heartbeats.push(JSON.parse(body)); res.end('{"ok":true}'); }
    else if (req.url === '/internal/receipts') res.end(JSON.stringify(JSON.parse(body).filter(id => committed.has(id))));
    else if (req.url === '/internal/ingest') { const frame = Envelope.decode(body); committed.set(frame.frameId, frame); res.end('{"committed":true}'); }
    else { res.writeHead(404); res.end('{}'); }
  });
  server.listen(0, '127.0.0.1'); await once(server, 'listening');
  t.after(async () => {
    await stop();
    server.closeAllConnections(); await new Promise(resolve => server.close(resolve));
    assert.equal(path.dirname(directory), path.resolve(os.tmpdir()));
    fs.rmSync(directory, { recursive: true, force: true });
  });
  return {
    spool, committed, heartbeats, encode, envelope, stop,
    blockDelivery(value) { blocked = value; },
    async start() {
      const priorHeartbeats = heartbeats.length;
      child = spawn(process.execPath, [path.join(runtime, 'index.js')], {
        windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'],
        env: { ...process.env, NODE_PATH: path.join(__dirname, 'node_modules'), INGEST_TRANSPORT: 'local',
          BACKEND_URL: `http://127.0.0.1:${server.address().port}`, INTERNAL_TOKEN: 'isolated-test-token',
          SPOOL_DIR: spool, SETTINGS_PORT: '0', BIND_HOST: '127.0.0.1', DEMO_INTERVAL_MS: '25', RAW_CAPTURE_DIR: '',
          DOUYIN_ROOM_COOKIE_DIR: path.join(directory, 'unused-cookies'), AUTH_ORIGINS: 'http://127.0.0.1:3000',
        },
      });
      child.stdout.resume(); child.stderr.on('data', data => { stderr += data; });
      await until(() => heartbeats.length > priorHeartbeats, 'collector failed to start: ' + stderr);
    },
  };
}

test('main quarantines interrupted writes outside active capacity and recovers complete temporary records', { timeout: 20000 }, async t => {
  const f = await fixture(t);
  const payload = padMessage(f.encode('PushFrame', { payloadType: 'hb' }), 8 * 1024 * 1024);
  const partial = f.envelope(payload).body.subarray(0, 8 * 1024 * 1024 - 1);
  for (let i = 0; i < 8; i++) fs.writeFileSync(path.join(f.spool, `interrupted-${i}.tmp`), partial);
  const complete = f.envelope();
  fs.writeFileSync(path.join(f.spool, complete.id + '.tmp'), complete.body);
  await f.start();
  await until(() => [...f.committed.values()].some(frame => frame.sessionId !== 'recovery-fixture' && frame.kind === 'upstream_frame'), 'retained incomplete records still prevent new messages');
  assert.ok(f.committed.has(complete.id), 'complete fsynced temporary record must still be delivered');
  const quarantine = path.join(f.spool, 'quarantine');
  const files = fs.readdirSync(quarantine);
  assert.equal(files.length, 8);
  for (const file of files) assert.equal(fs.statSync(path.join(quarantine, file)).size, partial.length, 'original partial bytes must be retained');
  assert.deepEqual(fs.readFileSync(path.join(quarantine, files[0])), partial);
  assert.deepEqual(f.heartbeats.at(-1).spool_quarantine, { files: 8, bytes: CAPACITY - 8 });
  assert.ok(f.heartbeats.at(-1).spool_bytes < CAPACITY / 2, 'quarantine must not consume active spool capacity');
  await f.stop();
  await f.start();
  assert.deepEqual(f.heartbeats.at(-1).spool_quarantine, { files: 8, bytes: CAPACITY - 8 });
  assert.ok(f.heartbeats.at(-1).spool_bytes < CAPACITY / 2, 'retained quarantine must stay outside capacity after another restart');
});

test('main reports full-spool backpressure over heartbeat and clears it after actual delivery recovery', { timeout: 20000 }, async t => {
  const f = await fixture(t);
  for (let bytes = 0; bytes < CAPACITY;) {
    const size = Math.min(7 * 1024 * 1024, CAPACITY - bytes), frame = f.envelope();
    fs.writeFileSync(path.join(f.spool, frame.id + '.bin'), padMessage(frame.body, size)); bytes += size;
  }
  f.blockDelivery(true);
  await f.start();
  const full = f.heartbeats.at(-1);
  assert.equal(full.spool_bytes, CAPACITY);
  assert.equal(full.room_states?.length, 1, 'room state must be reported even when its spool write fails');
  const state = full.room_states[0];
  assert.equal(state.status, 'backpressured');
  assert.equal(state.live_id, 'demo');
  assert.equal(state.desired_version, 7);
  assert.ok(state.session_id);
  assert.match(state.session_seq, /^\d+$/);
  assert.ok(state.observed_at_ms > 0);
  f.blockDelivery(false);
  await until(() => f.heartbeats.some(heartbeat => heartbeat.room_states?.some(next => next.status === 'collecting' && next.session_id === state.session_id && BigInt(next.session_seq) > BigInt(state.session_seq))), 'backpressure did not recover after spool delivery resumed');
  const recovered = f.heartbeats.find(heartbeat => heartbeat.room_states?.[0]?.status === 'collecting').room_states[0];
  await until(() => [...f.committed.values()].some(frame => frame.kind === 'upstream_frame' && frame.sessionId === recovered.session_id && String(frame.sessionSeq) === recovered.session_seq), 'recovery watermark must correspond to a real persisted upstream frame');
});
