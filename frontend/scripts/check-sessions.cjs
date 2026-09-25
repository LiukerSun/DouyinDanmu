const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {createRequire} = require('node:module');
const ts = require('typescript');

function loadTs(relativePath) {
  const sourcePath = path.resolve(__dirname, '../src', relativePath);
  const compiled = ts.transpileModule(fs.readFileSync(sourcePath, 'utf8'), {
    compilerOptions: {target: ts.ScriptTarget.ES2020, module: ts.ModuleKind.CommonJS, jsx: ts.JsxEmit.ReactJSX, esModuleInterop: true},
  }).outputText;
  const component = {exports: {}};
  const localRequire = createRequire(sourcePath);
  const componentRequire = name => {
    if (name.startsWith('.')) {
      for (const extension of ['.ts', '.tsx']) {
        const candidate = path.resolve(path.dirname(sourcePath), name + extension);
        if (fs.existsSync(candidate)) return loadTs(candidate);
      }
    }
    return localRequire(name);
  };
  new Function('require', 'module', 'exports', compiled)(componentRequire, component, component.exports);
  return component.exports;
}

const domainPath = path.resolve(__dirname, '../src/pipeline/sessions.ts');
assert.ok(fs.existsSync(domainPath), 'pipeline/sessions.ts must exist');
const domainSource = fs.readFileSync(domainPath, 'utf8');
for (const name of ['LiveSession', 'SessionStats', 'SessionListResult', 'sessionsQuery']) {
  assert.match(domainSource, new RegExp('export (type|function) ' + name + '\\b'), 'sessions.ts must export ' + name);
}

const {sessionsQuery, sessionEventsQuery, sessionRankingQuery} = loadTs('pipeline/sessions.ts');
const ended = {id: 5, room_id: '7xxx', started_at_ms: 1700000000000, ended_at_ms: 1700003600000, status: 'ended', start_source: 'activity', end_source: 'control_message',
  stats: {chat_count: '741', gift_events: '3', gift_quantity: '10', known_gift_value: '55621', unknown_price_quantity: '0', value_complete: true, like_count: '1304', enter_count: '0', peak_online: 177}};
const live = {...ended, ended_at_ms: null, status: 'live', end_source: null};

const list = new URLSearchParams(sessionsQuery('123', 40));
assert.equal(list.get('limit'), '20');
assert.equal(list.get('offset'), '40');
assert.throws(() => sessionsQuery('not-a-room', 0), /直播间/, 'Invalid rooms must be rejected before requesting');
console.log('PASS: sessionsQuery validates the room and builds limit/offset pagination');

const chat = new URLSearchParams(sessionEventsQuery('123', ended, 'chat', '9007199254740993'));
assert.equal(chat.get('room'), '123');
assert.equal(chat.get('type'), 'chat');
assert.equal(chat.get('view'), 'events', 'Chat history must use the events view');
assert.equal(chat.get('from_ms'), '1700000000000');
assert.equal(chat.get('to_ms'), '1700003600000');
assert.equal(chat.get('before'), '9007199254740993');
const gift = new URLSearchParams(sessionEventsQuery('123', ended, 'gift', ''));
assert.equal(gift.get('type'), 'gift');
assert.equal(gift.get('view'), null, 'Gift history must use the default merged combo view');
assert.equal(gift.get('before'), null);
const liveChat = new URLSearchParams(sessionEventsQuery('123', live, 'chat', ''));
assert.equal(liveChat.get('to_ms'), null, 'Live sessions must query without an end bound');
assert.equal(liveChat.get('from_ms'), '1700000000000');
console.log('PASS: sessionEventsQuery scopes chat/gift history to the session window and cursor');

const ranking = new URLSearchParams(sessionRankingQuery('123', ended, 20, '9876'));
assert.equal(ranking.get('room'), '123');
assert.equal(ranking.get('sort'), 'value');
assert.equal(ranking.get('offset'), '20');
assert.equal(ranking.get('from_ms'), '1700000000000');
assert.equal(ranking.get('to_ms'), '1700003600000');
assert.equal(ranking.get('as_of_seq'), '9876', 'Ranking pages must reuse the snapshot');
const liveRanking = new URLSearchParams(sessionRankingQuery('123', live, 0));
assert.equal(liveRanking.get('to_ms'), null);
assert.equal(liveRanking.get('as_of_seq'), null);
console.log('PASS: sessionRankingQuery scopes the gift ranking to the session window with snapshot pagination');

const componentSource = fs.readFileSync(path.resolve(__dirname, '../src/components/RoomSessions.tsx'), 'utf8');
assert.ok(componentSource.includes('/sessions'), 'RoomSessions must query the sessions endpoint');
assert.ok(componentSource.includes('/messages/search'), 'RoomSessions must query message history');
assert.ok(componentSource.includes('/analytics/gift-ranking'), 'RoomSessions must query the gift ranking');
for (const name of ['StudioFilters', 'InlineFeedback', 'PipelineMessage', 'MessageDetails']) {
  assert.ok(componentSource.includes(`from './${name}'`), 'RoomSessions must reuse ' + name);
}
assert.ok(fs.existsSync(path.resolve(__dirname, '../src/components/RoomSessions.css')), 'RoomSessions.css must exist');
console.log('PASS: RoomSessions wires the sessions, message history and gift ranking endpoints with shared components');

const pipelineSource = fs.readFileSync(path.resolve(__dirname, '../src/pages/Pipeline.tsx'), 'utf8');
assert.ok(pipelineSource.includes("roomContent === 'sessions'"), 'Pipeline must branch on the sessions room content');
assert.ok(pipelineSource.includes('<RoomSessions'), 'Pipeline must mount RoomSessions');
assert.ok(pipelineSource.includes("id: 'sessions'"), 'Pipeline must offer the sessions tab');
console.log('PASS: Pipeline exposes the sessions tab and mounts RoomSessions');
