const fs = require('node:fs');

// Windows cannot open directories through fs.openSync. File contents are flushed
// before the same-directory rename on both platforms; Unix also flushes the dir.
function syncDirectory(directory) {
  if (process.platform === 'win32') return;
  const fd = fs.openSync(directory, 'r');
  try { fs.fsyncSync(fd); } finally { fs.closeSync(fd); }
}
module.exports = { syncDirectory };
