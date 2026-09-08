const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {createRequire} = require('node:module');
const ts = require('typescript');

async function main() {
const heroUi = await import('@heroui/react');
function loadTs(relativePath) {
  const sourcePath = path.resolve(__dirname, '../src', relativePath);
  const compiled = ts.transpileModule(fs.readFileSync(sourcePath, 'utf8'), {
    compilerOptions: {target: ts.ScriptTarget.ES2020, module: ts.ModuleKind.CommonJS, jsx: ts.JsxEmit.ReactJSX, esModuleInterop: true},
  }).outputText;
  const component = {exports: {}};
  const localRequire = createRequire(sourcePath);
  const componentRequire = name => {
    if (name === '@heroui/react') return heroUi;
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

const {mergeMessages, advanceCursor} = loadTs('pipeline/messages.ts');
const event = overrides => ({
  seq: '1', event_id: 'event-1', display_id: 'gift-combo-1', live_id: 'room-a',
  source: 'douyin', type: 'gift', timestamp: 1700000000000,
  user_id: '9007199254740993', user_name: '观众', user_level: null,
  fans_club: {member: null, level: null, name: '', status: null},
  content: '小心心', gift_count: 1, gift_final: false,
  persisted_at_ms: 1700000000050, received_at_ms: 1700000000000,
  ...overrides,
});

const initial = mergeMessages([], [event({})], 'room-a');
const updated = mergeMessages(initial, [event({seq: '3', event_id: 'event-3', gift_count: 3})], 'room-a');
assert.equal(updated.length, 1, '同一连送应保留一行');
assert.equal(updated[0].gift_count, 3, '礼物数量应更新为累计 3 个');
console.log('PASS: a gift combo updates one row from 小心心 × 1 to 小心心 × 3');

const latest = event({seq: '9007199254740993', event_id: 'event-final', gift_count: 3, gift_final: true});
const replayed = mergeMessages([latest], [
  event({seq: '9007199254740992', event_id: 'event-older', gift_count: 2}),
  latest,
  event({seq: '9007199254740993', event_id: 'event-final', gift_count: 1, gift_final: false}),
], 'room-a');
assert.equal(replayed.length, 1, '快照和重放不得重复追加连送');
assert.equal(replayed[0].gift_count, 3, '旧序号和相同序号不得覆盖最新数量');
assert.equal(replayed[0].gift_final, true, '旧进度不得覆盖连送已完成状态');
assert.equal(latest.gift_count, 3, '合并不得修改暂停展示持有的旧对象');
console.log('PASS: duplicate replay and out-of-order 64-bit sequences preserve the latest combo');

const chat = event({type: 'chat', event_id: 'chat-1', display_id: undefined, seq: '4', content: '你好[色]'});
const switched = mergeMessages(updated, [
  event({live_id: 'room-b', gift_count: 2, seq: '5'}),
  chat,
  event({live_id: 'room-b', type: 'online_count', display_id: 'online-6', event_id: 'online-6', seq: '6'}),
], 'room-b');
assert.equal(switched.length, 2, '切换房间必须清除旧房间行，保留在线人数事件');
assert.equal(switched[0].live_id, 'room-b');
assert.equal(switched[0].gift_count, 2);
const mixed = mergeMessages(updated, [chat, chat, event({display_id: 'gift-combo-2', event_id: 'gift-2', seq: '5'})], 'room-a');
assert.deepEqual(mixed.map(row => row.display_id || row.event_id), ['gift-combo-1', 'chat-1', 'gift-combo-2']);
const capped = mergeMessages([], Array.from({length: 510}, (_, i) => event({type: 'chat', display_id: undefined, event_id: `chat-${i}`, seq: String(i + 1)})), 'room-a');
assert.equal(capped.length, 500);
assert.equal(capped[0].event_id, 'chat-10');
const expanded = mergeMessages([], ['online_count', 'room_rank', 'commerce', 'unknown', 'parse_error'].map((type, i) => event({ type, display_id: undefined, event_id: 'full-' + i, seq: String(i + 1) })), 'room-a');
assert.equal(expanded.length, 5, '全量监控不得过滤统计、榜单、电商、未知或错误消息');
console.log('PASS: room changes, all message categories and 500-row retention');

const React = require('react');
const {renderToStaticMarkup} = require('react-dom/server');
const PipelineMessage = loadTs('components/PipelineMessage.tsx').default;
const render = value => renderToStaticMarkup(React.createElement(PipelineMessage, {event: value}));
const unknown = render(event({type: 'chat', content: '你好[色]'}));
assert.ok(unknown.includes('9007199254740993'), '完整 UID 不得截断或丢精度');
assert.ok(unknown.includes('财富等级未知'), '缺失财富等级不得展示 0 级');
assert.ok(unknown.includes('未显示灯牌'), '缺失粉丝团资料不得展示未加入');
assert.ok(!unknown.includes('财富 Lv.0') && !unknown.includes('未加入粉丝团') && !unknown.includes('未点亮灯牌'));
assert.ok(!unknown.includes('粉丝团未知') && !unknown.includes('粉丝团 Lv.0'));
assert.ok(unknown.includes('alt="[色]"'), '用户资料展示必须保留表情渲染');
console.log('PASS: full UID and explicit unknown profile fields, retaining chat emoji');

const member = render(event({user_level: 26, fans_club: {member: true, level: 8, name: '星光团', status: 1}, gift_count: 3, gift_final: true}));
assert.ok(member.includes('财富 Lv.26') && member.includes('星光团') && member.includes('星光团-8级'));
assert.ok(member.includes('小心心') && member.includes('× 3') && member.includes('连送完成'));
assert.ok(member.includes('复制 UID 9007199254740993'));
assert.ok(member.includes('>星光团-8级</span>'), '有灯牌名称和等级时必须直接展示名字-等级');
const namedOnly = render(event({fans_club: {member: null, level: null, name: '原来阿', status: null}}));
assert.ok(namedOnly.includes('>原来阿-等级待补全</span>') && !namedOnly.includes('未显示灯牌'), '有名称不能被缺失状态或等级隐藏');
const namedUnknown = render(event({fans_club: {member: null, level: 10, name: '原来阿', status: 2}}));
assert.ok(namedUnknown.includes('>原来阿-10级</span>'), '未知成员状态不得隐藏已知名称和等级');
assert.ok(namedUnknown.includes('未确认灯牌点亮状态') && !namedUnknown.includes('VIP') && !namedUnknown.includes('星守护'));
const missing = render(event({user_id: '', user_level: undefined, fans_club: undefined}));
assert.ok(missing.includes('未提供') && missing.includes('财富等级未知') && missing.includes('未显示灯牌'));
const nonmember = render(event({user_level: 0, fans_club: {member: false, level: null, name: '', status: 0}}));
assert.ok(nonmember.includes('财富 Lv.0') && nonmember.includes('未加入粉丝团'));
console.log('PASS: supplied wealth/fan levels, explicit non-membership, copy UID and final gift quantity');

assert.equal(advanceCursor('9007199254740993', '9007199254740992'), '9007199254740993', '回放旧批次不得令重连游标倒退');
assert.equal(advanceCursor('9007199254740992', '9007199254740993'), '9007199254740993');
console.log('PASS: recovery cursor never moves backwards during replay');

const system = render(event({type: 'system', user_id: '', content: '直播已结束'}));
assert.ok(!system.includes('message-profile') && !system.includes('财富等级未知'), '系统通知不应显示观众资料');
const extra = render(event({type: 'chat', method: 'WebcastChatMessage', parse_status: 'partial', user_id: '', content: '<script>alert(1)</script>'}));
assert.ok(extra.includes('弹幕') && !extra.includes('WebcastChatMessage') && !extra.includes('部分字段待解释'), '消息卡片只展示业务类型，不展示协议诊断信息');
assert.ok(!extra.includes('<script>'), '原始文本不得执行脚本');
const online = render(event({type: 'online_count', user_id: '', content: '在线人数更新'}));
assert.ok(!online.includes('message-profile'), '在线人数通知不应显示观众资料');
const singleGift = render(event({gift_combo: false, gift_final: true}));
assert.ok(!singleGift.includes('连送完成') && !singleGift.includes('连送中'), '单次礼物不应显示连送状态');
const comboGift = render(event({gift_combo: true, gift_final: true}));
assert.ok(comboGift.includes('连送完成'));
console.log('PASS: only user events show user profiles and only combo gifts show combo progress');

const edge = mergeMessages([], [event({}), ...Array.from({length: 499}, (_, i) => event({type: 'chat', event_id: `edge-chat-${i}`, display_id: undefined, seq: String(i + 2)}))], 'room-a');
const freshAtEdge = mergeMessages(edge, [event({event_id: 'edge-update', seq: '501', gift_count: 3}), event({type: 'chat', event_id: 'edge-latest-chat', display_id: undefined, seq: '502'})], 'room-a');
assert.equal(freshAtEdge.length, 500);
assert.equal(freshAtEdge[freshAtEdge.length - 2].display_id, 'gift-combo-1', '更新中的旧连送必须按最新序号保留在列表底部');
assert.equal(freshAtEdge[freshAtEdge.length - 2].gift_count, 3);
assert.equal(freshAtEdge[0].event_id, 'edge-chat-1', '500 行边界应淘汰最早的消息而非最新连送');
console.log('PASS: an updated combo survives the 500-row boundary and follows latest sequence ordering');

const CollectorIdentity = loadTs('components/CollectorIdentity.tsx').default;
const identity = status => renderToStaticMarkup(React.createElement(CollectorIdentity, {status}));
assert.ok(identity('anonymous').includes('本直播间使用游客身份采集；礼物是否下发以实际采集为准。'));
assert.ok(identity('configured').includes('已配置登录凭据') && !identity('configured').includes('已登录'));
assert.ok(identity('configuration_error').includes('登录凭据配置错误'));
assert.ok(identity(undefined).includes('身份状态待确认'));
console.log('PASS: collector identity distinguishes anonymous, configured, invalid and unknown credentials');

const {levelBand, giftTone} = loadTs('pipeline/message-colors.ts');
for (const [kind, ranges] of [
  ['wealth', [[0, 9, 'slate'], [10, 19, 'teal'], [20, 29, 'blue'], [30, 39, 'violet'], [40, 49, 'amber'], [50, 100, 'rose']]],
  ['fans', [[0, 4, 'teal'], [5, 9, 'blue'], [10, 14, 'violet'], [15, 19, 'amber'], [20, 100, 'rose']]],
]) {
  for (const [min, max, tone] of ranges) for (const level of [min, max]) assert.equal(levelBand(level, kind).tone, tone);
  for (const level of [null, undefined, -1, NaN, Infinity, 1.5]) assert.equal(levelBand(level, kind).label, '等级未知');
}
assert.notEqual(giftTone('粉丝团灯牌'), giftTone('为你闪耀'));
assert.equal(giftTone(' 新礼物 '), giftTone('新礼物'));
assert.equal(typeof giftTone('__proto__'), 'string');
assert.equal(typeof giftTone('constructor'), 'string');
const levelColor = render(event({user_level: 31, fans_club: {member: null, level: 2, name: '', status: null}}));
assert.match(levelColor, /<span(?=[^>]*user-level-badge)(?=[^>]*data-tone="violet")[^>]*>/);
assert.match(levelColor, /<span(?=[^>]*fans-badge)(?=[^>]*data-tone="teal")[^>]*>/);
assert.ok(levelColor.includes('粉丝团-2级') && levelColor.includes('未确认灯牌点亮状态'), 'Known fan level must not invent membership');
const nonMemberColor = render(event({fans_club: {member: false, level: 20, name: '', status: null}}));
assert.match(nonMemberColor, /<span(?=[^>]*fans-badge)(?=[^>]*data-tone="slate")[^>]*>/);
console.log('PASS: level boundaries, missing levels, fan membership and stable gift colors');

}
main().catch(error => { console.error(error); process.exitCode = 1; });
