const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { readVersion, changeVersion } = require('./version.cjs');

test('version changes keep one source and start a dated changelog entry without losing history', t => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'douyin-version-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  fs.writeFileSync(path.join(root, 'VERSION'), '2.5.0\n');
  const history = '# 更新记录\n\n## v2.5.0（2026-10-27）\n\n- 已有功能。\n';
  fs.writeFileSync(path.join(root, 'CHANGELOG.md'), history);
  assert.equal(readVersion(root), '2.5.0');
  assert.equal(changeVersion(root, 'patch', '2026-10-28'), '2.5.1');
  assert.match(fs.readFileSync(path.join(root, 'CHANGELOG.md'), 'utf8'), /## v2\.5\.1（2026-10-28）[\s\S]*已有功能/);
  assert.equal(changeVersion(root, 'minor'), '2.6.0');
  assert.equal(changeVersion(root, 'major'), '3.0.0');
  assert.equal(changeVersion(root, '3.1.2'), '3.1.2');
  assert.throws(() => changeVersion(root, '3.0.0'), /递增/);
  for (const invalid of ['v4.0.0', '4.0', '04.0.0', '65536.0.0', '4.0.0-beta', '../VERSION'])
    assert.throws(() => changeVersion(root, invalid), /无效/);
  assert.equal(readVersion(root), '3.1.2');
});
