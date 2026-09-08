// Recover missing fan fields from the exact archived message, never from another
// viewer, room or time. Dry-run by default. Existing values and counters are kept.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const { DatabaseSync } = require('node:sqlite');
const protobuf = require('protobufjs');

const methods = { WebcastChatMessage: 'ChatMessage', WebcastMemberMessage: 'MemberMessage', WebcastGiftMessage: 'GiftMessage', WebcastLikeMessage: 'LikeMessage', WebcastSocialMessage: 'SocialMessage' };
const own = (object, field) => object != null && Object.hasOwn(object, field);
function missingFans(body, supplied) {
  const fans = { ...(body.fans_club || {}) };
  const fields = ['name', 'level', 'status', 'anchor_id', 'member'];
  if (fields.some(field => fans[field] != null && fans[field] !== '' && supplied[field] != null && fans[field] !== supplied[field])) return null;
  let changed = false;
  for (const field of fields) {
    if (supplied[field] == null || (fans[field] != null && fans[field] !== '')) continue;
    fans[field] = supplied[field]; changed = true;
    if (field === 'member') fans.member_inferred = true;
  }
  return changed ? { ...body, fans_club: fans } : null;
}

async function repair({ database, capture, proto, apply = false }) {
  const root = await protobuf.load([path.join(proto, 'douyin.proto'), path.join(proto, 'ingest.proto')]);
  const Envelope = root.lookupType('pipeline.RawFrameEnvelope'), Push = root.lookupType('PushFrame'), Response = root.lookupType('Response');
  const fanField = root.lookupType('User').fieldsById[24].name;
  const db = new DatabaseSync(database, { readOnly: !apply });
  db.exec('PRAGMA busy_timeout=5000');
  const result = { frames: 0, decodeErrors: 0, events: 0, views: 0, users: [], backup: null, applied: apply };
  const candidates = new Map();
  try {
    for (const file of fs.readdirSync(capture).filter(file => file.endsWith('.bin')).sort()) {
      try {
        const frame = Envelope.decode(fs.readFileSync(path.join(capture, file)));
        if (frame.kind !== 'upstream_frame') continue;
        let bytes = Push.decode(frame.payload).payload;
        if (!bytes?.length) continue;
        if (bytes[0] === 31 && bytes[1] === 139) bytes = zlib.gunzipSync(bytes);
        const response = Response.decode(bytes); result.frames++;
        for (const message of response.messagesList || []) {
          if (!methods[message.method] || String(message.msgId) === '0') continue;
          const user = root.lookupType(methods[message.method]).decode(message.payload).user;
          const data = user?.[fanField]?.data;
          if (!data) continue;
          const status = own(data, 'userFansClubStatus') ? data.userFansClubStatus : null;
          const fans = { name: data.clubName || null, level: own(data, 'level') && data.level >= 0 ? data.level : null, status,
            member: status === 1 ? true : status === 0 ? false : null,
            anchor_id: data.anchorId && String(data.anchorId) !== '0' ? String(data.anchorId) : null };
          const id = `${frame.roomId}:${message.method}:${message.msgId}:${message.wrdsVersion}`;
          const uid = String(user.idStr || user.id || '');
          const key = JSON.stringify([frame.liveId, id, uid]);
          // Different retransmissions of one event must agree before recovery.
          const previous = candidates.get(key);
          if (previous && JSON.stringify(previous.fans) !== JSON.stringify(fans)) previous.conflict = true;
          else if (!previous) candidates.set(key, { id, live: frame.liveId, uid, fans });
        }
      } catch { result.decodeErrors++; }
    }
    if (apply && result.decodeErrors) throw new Error('Archive decode errors; refusing partial repair');
    if (apply) {
      result.backup = `${database}.before-fans-${Date.now()}.bak`;
      db.prepare('VACUUM INTO ?').run(result.backup);
      db.exec('BEGIN IMMEDIATE');
    }
    const viewers = new Set();
    for (const candidate of candidates.values()) {
      if (candidate.conflict) continue;
      const row = db.prepare('SELECT id,body FROM events WHERE event_id=? AND live_id=?').get(candidate.id, candidate.live);
      if (!row) continue;
      const body = JSON.parse(row.body);
      if (String(body.user_id) !== candidate.uid) continue;
      const repaired = missingFans(body, candidate.fans);
      if (repaired) {
        result.events++; viewers.add(body.user_name);
        if (apply) db.prepare('UPDATE events SET body=? WHERE id=?').run(JSON.stringify(repaired), row.id);
      }
      // A gift view may represent a later message: only repair the exact sequence.
      for (const view of db.prepare('SELECT display_id,body FROM message_views WHERE live_id=? AND last_seq=?').all(candidate.live, row.id)) {
        const viewBody = JSON.parse(view.body);
        if (String(viewBody.user_id) !== candidate.uid) continue;
        const patched = missingFans(viewBody, candidate.fans);
        if (!patched) continue;
        result.views++; viewers.add(body.user_name);
        if (apply) db.prepare('UPDATE message_views SET body=? WHERE live_id=? AND display_id=?').run(JSON.stringify(patched), candidate.live, view.display_id);
      }
    }
    if (apply) db.exec('COMMIT');
    result.users = [...viewers];
    return result;
  } catch (error) { if (apply && db.isTransaction) db.exec('ROLLBACK'); throw error; }
  finally { db.close(); }
}

module.exports = { repair };
if (require.main === module) {
  const [database, capture, proto] = process.argv.slice(2);
  if (!database || !capture || !proto) throw new Error('Usage: node repair-fans-profiles.js database capture-directory proto-directory [--apply]');
  repair({ database, capture, proto, apply: process.argv.includes('--apply') }).then(result => console.log(JSON.stringify(result, null, 2))).catch(error => { console.error(error.message); process.exitCode = 1; });
}
