const fs = require('node:fs');
const path = require('node:path');

function readVersion(root) {
  const value = fs.readFileSync(path.join(root, 'VERSION'), 'utf8').trim();
  if (!/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(value)
      || value.split('.').some(part => Number(part) > 65535))
    throw new Error('VERSION 必须是 0–65535 范围内的 主版本.次版本.修订版本，例如 2.5.0');
  return value;
}

function changeVersion(root, requested, date = new Intl.DateTimeFormat('en-CA', { timeZone: 'Asia/Shanghai' }).format(new Date())) {
  const current = readVersion(root);
  const parts = current.split('.').map(Number);
  const index = { major: 0, minor: 1, patch: 2 }[requested];
  if (index !== undefined) { parts[index]++; parts.fill(0, index + 1); }
  const next = index === undefined ? requested : parts.join('.');
  if (!/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(next)
      || next.split('.').some(part => Number(part) > 65535)) throw new Error('新版本号无效');
  if (next === current) return current;
  const compared = next.split('.').map(Number).map((part, i) => part - current.split('.').map(Number)[i]).find(value => value !== 0);
  if (compared < 0) throw new Error('版本号只能递增');
  const changelogPath = path.join(root, 'CHANGELOG.md');
  const changelog = fs.readFileSync(changelogPath, 'utf8');
  if (new RegExp(`^## v${next.replaceAll('.', '\\.')}[（(\\s]`, 'm').test(changelog))
    throw new Error('CHANGELOG 已有此版本，请先核对记录');
  const headingEnd = changelog.indexOf('\n');
  if (!changelog.startsWith('# ') || headingEnd < 0) throw new Error('CHANGELOG.md 缺少文档标题');
  const updated = changelog.slice(0, headingEnd).trimEnd() + `\n\n## v${next}（${date}）\n\n- 待补充更新说明。\n\n` + changelog.slice(headingEnd).trimStart();
  fs.writeFileSync(changelogPath, updated, 'utf8');
  try { fs.writeFileSync(path.join(root, 'VERSION'), next + '\n', 'utf8'); }
  catch (error) { fs.writeFileSync(changelogPath, changelog, 'utf8'); throw error; }
  return next;
}

module.exports = { readVersion, changeVersion };
if (require.main === module) {
  try {
    const root = path.resolve(__dirname, '..'), command = process.argv[2] || 'check';
    if (process.argv.length > 3) throw new Error('用法：node portable/version.cjs [check|patch|minor|major|2.6.0]');
    console.log(command === 'check' ? readVersion(root) : changeVersion(root, command));
  } catch (error) { console.error(error.message); process.exitCode = 1; }
}
