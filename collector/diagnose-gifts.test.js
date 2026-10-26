'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs'), os = require('node:os'), path = require('node:path');
const { spawnSync } = require('node:child_process');
const { gzipSync } = require('node:zlib');
const { DatabaseSync } = require('node:sqlite');

test('gift diagnostic reads mixed TEXT/gzip bodies without changing the database', () => {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'gift-diagnostic-'));
  const database = path.join(temp, 'test.db'), output = path.join(temp, 'report');
  const db = new DatabaseSync(database);
  try {
    db.exec('CREATE TABLE events(live_id TEXT,body TEXT); CREATE TABLE quarantine(frame_id TEXT,reason TEXT,raw BLOB,created_at INTEGER);');
    const insert = db.prepare('INSERT INTO events VALUES(?,?)');
    insert.run('111', JSON.stringify({ type: 'chat', received_at_ms: 123 }));
    insert.run('111', gzipSync(JSON.stringify({ type: 'chat', received_at_ms: 456 })));
    insert.run('111', gzipSync(JSON.stringify({ type: 'gift', received_at_ms: 789 })));
    const before = db.prepare('SELECT * FROM events').all();
    const run = () => spawnSync(process.execPath, [path.join(__dirname, 'diagnose-gifts.js'), database, output], {
      env: { ...process.env, PROTO_DIR: path.resolve(__dirname, '../backend/proto'), RAW_CAPTURE_PATH: '' }, encoding: 'utf8', windowsHide: true,
    });
    const result = run();
    assert.equal(result.status, 0, result.stderr);
    const report = JSON.parse(fs.readFileSync(path.join(output, 'quarantine-summary.json'), 'utf8'));
    assert.deepEqual(report.eventSummary.sort((a, b) => a.type.localeCompare(b.type)), [
      { live_id: '111', type: 'chat', count: 2, last_received_at: 456 },
      { live_id: '111', type: 'gift', count: 1, last_received_at: 789 },
    ]);
    assert.deepEqual(db.prepare('SELECT * FROM events').all(), before);
    insert.run('111', Buffer.from([1, 2, 3, 4]));
    const corrupt = run();
    assert.notEqual(corrupt.status, 0);
    assert.match(corrupt.stderr, /undecodable body/i);
  } finally { db.close(); fs.rmSync(temp, { recursive: true, force: true }); }
});
