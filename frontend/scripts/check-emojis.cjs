const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {createRequire} = require('node:module');
const ts = require('typescript');
const React = require('react');
const {renderToStaticMarkup} = require('react-dom/server');
const sourcePath = path.resolve(__dirname,'../src/components/ChatContent.tsx');
const source = fs.readFileSync(sourcePath,'utf8');
const compiled = ts.transpileModule(source,{compilerOptions:{module:ts.ModuleKind.CommonJS,jsx:ts.JsxEmit.ReactJSX,esModuleInterop:true}}).outputText;
const component = {exports:{}};
new Function('require','module','exports',compiled)(createRequire(sourcePath),component,component.exports);
const render = content => renderToStaticMarkup(React.createElement(component.exports.default,{content}));
const catalog = require('../src/data/douyin-emojis.json');
const actual = render('[色]');
assert.match(actual,/<img\b/);
assert.ok(actual.includes('alt="[色]"'));
assert.ok(actual.includes(catalog['[色]']));
const mixed = render('你好[色][微笑][色]🙂[未收录表情]<script>alert(1)</script>');
assert.equal((mixed.match(/<img\b/g)||[]).length,3);
assert.ok(mixed.includes('你好') && mixed.includes('🙂[未收录表情]'));
assert.ok(mixed.includes('&lt;script&gt;') && !mixed.includes('<script>'));
for (const asset of Object.values(catalog)) {
  assert.match(asset,/^\/emojis\/[a-f0-9]+\.(png|gif|jpg|webp)$/);
  assert.ok(fs.statSync(path.resolve(__dirname,'../public' + asset)).size>0,asset);
}
console.log(`PASS: real [色], mixed/repeated emoji, Unicode, unknown codes, escaped text, ${Object.keys(catalog).length} local assets`);
