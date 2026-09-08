// Read-only quarantine inspection. Run in a disposable collector image with the
// backend volume mounted :ro; the optional output directory is separate.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const { DatabaseSync } = require('node:sqlite');
const protobuf = require('protobufjs');

function varint(buffer, offset) {
  let value = 0n, shift = 0n, end = offset;
  do {
    if (end >= buffer.length || shift > 63n) throw new Error('Invalid wire varint');
    const byte = buffer[end++]; value |= BigInt(byte & 127) << shift;
    if (!(byte & 128)) return { value, end };
    shift += 7n;
  } while (true);
}
function fields(buffer) {
  const list = []; let offset = 0;
  while (offset < buffer.length) {
    const start = offset, tag = varint(buffer, offset); offset = tag.end;
    const id = Number(tag.value >> 3n), wire = Number(tag.value & 7n);
    let value = null, data = null;
    if (wire === 0) { const decoded = varint(buffer, offset); value = decoded.value.toString(); offset = decoded.end; }
    else if (wire === 1) offset += 8;
    else if (wire === 2) { const length = varint(buffer, offset); offset = length.end; data = buffer.subarray(offset, offset + Number(length.value)); offset += Number(length.value); }
    else if (wire === 5) offset += 4;
    else throw new Error(`Unsupported wire type ${wire} for field ${id}`);
    if (offset > buffer.length) throw new Error('Truncated wire field');
    list.push({ id, wire, value, data, bytes: buffer.subarray(start, offset) });
  }
  return list;
}
function encodeVarint(value) {
  let n = BigInt(value), bytes = [];
  do { let byte = Number(n & 127n); n >>= 7n; if (n) byte |= 128; bytes.push(byte); } while (n);
  return Buffer.from(bytes);
}
function nested(id, data) { return Buffer.concat([encodeVarint(id * 8 + 2), encodeVarint(data.length), data]); }
function expectedWire(field) {
  if (field.resolvedType instanceof protobuf.Type || ['string', 'bytes'].includes(field.type)) return 2;
  if (['fixed64', 'sfixed64', 'double'].includes(field.type)) return 1;
  if (['fixed32', 'sfixed32', 'float'].includes(field.type)) return 5;
  return 0;
}
function userLevelSummary(root, method, payload, summaries) {
  const typeName = { WebcastChatMessage: 'ChatMessage', WebcastMemberMessage: 'MemberMessage' }[method];
  if (!typeName) return;
  const summary = summaries[method] ||= {
    messages: 0, decode_errors: 0, users: 0, pay_grade_present: 0,
    pay_grade_level_explicit: 0, pay_grade_levels: {}, fans_club_present: 0,
    fans_club_data_present: 0, fans_club_level_explicit: 0, fans_club_status_explicit: 0,
    fans_club_levels: {}, fans_club_statuses: {}, fans_club_status_levels: {}
  };
  summary.messages++;
  let user;
  try { user = root.lookupType(typeName).decode(payload).user; }
  catch { summary.decode_errors++; return; }
  if (!user) return;
  summary.users++;
  const User = root.lookupType('User');
  const pay = user[User.fieldsById[23].name], fans = user[User.fieldsById[24].name];
  const add = (histogram, value) => { const key = String(value); histogram[key] = (histogram[key] || 0) + 1; };
  if (pay) {
    summary.pay_grade_present++;
    if (Object.hasOwn(pay, 'level')) summary.pay_grade_level_explicit++;
    add(summary.pay_grade_levels, pay.level);
  }
  if (fans) {
    summary.fans_club_present++;
    if (fans.data) {
      summary.fans_club_data_present++;
      if (Object.hasOwn(fans.data, 'level')) summary.fans_club_level_explicit++;
      if (Object.hasOwn(fans.data, 'userFansClubStatus')) summary.fans_club_status_explicit++;
      add(summary.fans_club_levels, fans.data.level);
      add(summary.fans_club_statuses, fans.data.userFansClubStatus);
      add(summary.fans_club_status_levels, `${fans.data.userFansClubStatus}/${fans.data.level}`);
    }
  }
}

(async () => {
  const databasePath = process.argv[2] || '/data/pipeline.db';
  const outputDirectory = process.argv[3];
  const protoDirectory = process.env.PROTO_DIR || '/app/proto';
  const root = await protobuf.load([path.join(protoDirectory, 'douyin.proto'), path.join(protoDirectory, 'ingest.proto')]);
  const Envelope = root.lookupType('pipeline.RawFrameEnvelope'), Push = root.lookupType('PushFrame');
  const Response = root.lookupType('Response'), Gift = root.lookupType('GiftMessage'), User = root.lookupType('User');
  root.resolveAll();
  const db = new DatabaseSync(databasePath, { readOnly: true });
  const summary = db.prepare("SELECT reason, COUNT(*) count, MIN(created_at) first_at, MAX(created_at) last_at FROM quarantine GROUP BY reason ORDER BY count DESC").all();
  const eventSummary = db.prepare("SELECT live_id,json_extract(body,'$.type') type,COUNT(*) count,MAX(json_extract(body,'$.received_at_ms')) last_received_at FROM events GROUP BY live_id,type").all();
  let rows = db.prepare("SELECT frame_id,reason,raw,created_at FROM quarantine ORDER BY created_at DESC LIMIT 1000").all();
  if (process.env.RAW_CAPTURE_PATH) {
    rows = fs.readdirSync(process.env.RAW_CAPTURE_PATH).filter(file => file.endsWith('.bin')).map(file => {
      const raw = fs.readFileSync(path.join(process.env.RAW_CAPTURE_PATH, file));
      const envelope = Envelope.decode(raw);
      const errors = db.prepare('SELECT reason FROM quarantine WHERE frame_id=?').all(envelope.frameId);
      return { frame_id: envelope.frameId, raw, created_at: Number(envelope.receivedAtMs), reason: errors.map(error => error.reason).join('; ') };
    }).sort((a, b) => b.created_at - a.created_at);
  }
  const aggregate = { summary, eventSummary, examined_quarantine_rows: rows.length,
    scan_source: process.env.RAW_CAPTURE_PATH ? 'raw-capture' : 'quarantine',
    first_frame_received_at_ms: rows.length ? Math.min(...rows.map(row => row.created_at)) : null,
    last_frame_received_at_ms: rows.length ? Math.max(...rows.map(row => row.created_at)) : null };
  console.log(JSON.stringify(aggregate, null, 2));
  const methods = {}, roomMethods = {}, userLevels = {}, textMatches = [], reported = new Set();
  let saved = false, decodedFrames = 0, giftMessages = 0, receipts = 0;
  for (const row of rows) {
    if (reported.has(row.frame_id)) continue;
    reported.add(row.frame_id);
    let envelope, push;
    try {
      envelope = Envelope.decode(row.raw);
      if (envelope.kind !== 'upstream_frame') continue;
      push = Push.decode(envelope.payload);
    } catch { continue; }
    const payload = Buffer.from(push.payload);
    const response = Response.decode(payload[0] === 0x1f && payload[1] === 0x8b ? zlib.gunzipSync(payload) : payload);
    decodedFrames++;
    if (db.prepare('SELECT frame_id FROM receipts WHERE frame_id=?').get(row.frame_id)) receipts++;
    for (const message of response.messagesList || []) {
      methods[message.method] = (methods[message.method] || 0) + 1;
      const roomKey = `${envelope.liveId}/${envelope.roomId}`;
      roomMethods[roomKey] ||= {};
      roomMethods[roomKey][message.method] = (roomMethods[roomKey][message.method] || 0) + 1;
      userLevelSummary(root, message.method, message.payload, userLevels);
      const payloadBytes = Buffer.from(message.payload);
      const matched = ['星光真爱', '粉丝灯牌', '红烧猪咪咪', '送出了'].filter(term => payloadBytes.includes(Buffer.from(term)));
      if (matched.length) {
        const match = { frame_id: row.frame_id, created_at_ms: row.created_at, live_id: envelope.liveId,
          room_id: envelope.roomId, method: message.method, matched_terms: matched,
          payload_bytes: payloadBytes.length,
          has_receipt: !!db.prepare('SELECT frame_id FROM receipts WHERE frame_id=?').get(row.frame_id),
          quarantine_reason: row.reason };
        textMatches.push(match);
        if (outputDirectory && textMatches.length <= 3 && matched.some(term => term !== '红烧猪咪咪')) {
          fs.mkdirSync(outputDirectory, { recursive: true });
          fs.writeFileSync(path.join(outputDirectory, `gift-text-match-${textMatches.length}.bin`), payloadBytes);
        }
      }
      if (message.method !== 'WebcastGiftMessage') continue;
      giftMessages++;
      const wireFields = fields(Buffer.from(message.payload));
      let decodeResult;
      try { const decoded = Gift.decode(message.payload); decodeResult = { ok: true, comboCount: decoded.comboCount, repeatCount: decoded.repeatCount, giftName: decoded.gift && decoded.gift.name }; }
      catch (error) { decodeResult = { ok: false, error: error.message }; }
      const report = {
        frame_id: row.frame_id, created_at_ms: row.created_at, live_id: envelope.liveId,
        room_id: envelope.roomId, method: message.method, quarantine_reason: row.reason,
        has_receipt: !!db.prepare('SELECT frame_id FROM receipts WHERE frame_id=?').get(row.frame_id),
        payload_bytes: message.payload.length, protobufjs: decodeResult,
        fields: wireFields.map(field => {
          const schema = Gift.fieldsById[field.id];
          return { id: field.id, wire: field.wire, schema_name: schema && schema.name, schema_type: schema && schema.type,
            mismatch: schema ? field.wire !== expectedWire(schema) : undefined,
            scalar_value: field.value, bytes: field.data && field.data.length };
        })
      };
      console.log(JSON.stringify(report, null, 2));
      if (outputDirectory && !saved) {
        fs.mkdirSync(outputDirectory, { recursive: true });
        fs.writeFileSync(path.join(outputDirectory, 'gift-message-original.bin'), message.payload);
        // Retain observed count encodings and safe gift identity only. Omit
        // Common/text/effect records and replace both users to remove identity.
        const keep = new Set([2, 3, 4, 5, 6, 9, 11, 12, 13, 17, 20, 25, 28, 29, 30, 33, 34, 36, 41]);
        const anonymized = wireFields.filter(field => keep.has(field.id)).map(field => field.bytes);
        const user = User.encode(User.fromObject({ id: '9007199254740993', nickName: '测试送礼用户' })).finish();
        anonymized.push(nested(7, user));
        for (const gift of wireFields.filter(field => field.id === 15 && field.wire === 2)) {
          const safeGift = fields(gift.data).filter(field => [5, 12, 16].includes(field.id)).map(field => field.bytes);
          anonymized.push(nested(15, Buffer.concat(safeGift)));
        }
        const fixture = Buffer.concat(anonymized);
        let fixtureResult;
        try { Gift.decode(fixture); fixtureResult = { ok: true }; }
        catch (error) { fixtureResult = { ok: false, error: error.message }; }
        report.anonymized_fixture = { bytes: fixture.length, protobufjs: fixtureResult };
        fs.writeFileSync(path.join(outputDirectory, 'gift-message-anonymized.bin'), fixture);
        fs.writeFileSync(path.join(outputDirectory, 'gift-evidence.json'), JSON.stringify(report, null, 2));
        saved = true;
      }
    }
  }
  aggregate.scan = { decoded_frames: decodedFrames, frames_with_receipts: receipts,
    gift_messages: giftMessages, methods, room_methods: roomMethods, user_levels: userLevels, gift_text_matches: textMatches };
  console.log(JSON.stringify({ scan: aggregate.scan }, null, 2));
  if (outputDirectory) {
    fs.mkdirSync(outputDirectory, { recursive: true });
    fs.writeFileSync(path.join(outputDirectory, 'quarantine-summary.json'), JSON.stringify(aggregate, null, 2));
  }
  db.close();
})().catch(error => { console.error(error.stack); process.exitCode = 1; });
