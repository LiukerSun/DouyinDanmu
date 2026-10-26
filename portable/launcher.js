const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const http = require('node:http');
const crypto = require('node:crypto');
const { spawn } = require('node:child_process');
const { createAuthServer } = require('../auth/server');
const { createUpdateService } = require('../auth/update-service');
const { createPortableInstaller } = require('./update-package');

const appRoot = path.resolve(__dirname, '../..');
const root = process.env.DOUYIN_UPDATE_PROTOCOL === '1' && process.env.DOUYIN_INSTALL_ROOT
  ? path.resolve(process.env.DOUYIN_INSTALL_ROOT) : appRoot;
const data = path.join(root, 'data'), runtime = path.join(data, '.runtime');
const stateFile = path.join(runtime, 'instance.json');
const args = process.argv.slice(2);
const option = name => { const i = args.indexOf(name); return i < 0 ? undefined : args[i + 1]; };
const port = Number(option('--port') || 3000);
const debug = args.includes('--debug');
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
async function freePort() {
  const server = net.createServer();
  await new Promise((resolve, reject) => { server.once('error', reject); server.listen(0, '127.0.0.1', resolve); });
  const value = server.address().port; await new Promise(resolve => server.close(resolve)); return value;
}
async function stopExisting() {
  if (!fs.existsSync(stateFile)) { console.log('程序未运行。'); return; }
  const state = JSON.parse(fs.readFileSync(stateFile, 'utf8'));
  const response = await fetch(`http://127.0.0.1:${state.controlPort}/stop`, { method: 'POST', headers: { 'X-Control-Token': state.controlToken }, signal: AbortSignal.timeout(5000) });
  if (!response.ok) throw new Error('停止请求未通过校验，请在启动窗口按 Ctrl+C。');
  console.log('已请求停止，账号和采集数据保留。');
}
async function start() {
  if (!Number.isInteger(port) || port < 1024 || port > 65535) throw new Error('--port 需要是 1024–65535 的端口号');
  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--port') { i++; continue; }
    if (!['--debug', '--no-browser'].includes(args[i])) throw new Error('未知参数：' + args[i]);
  }
  fs.mkdirSync(runtime, { recursive: true }); fs.mkdirSync(path.join(root, 'logs'), { recursive: true });
  if (fs.existsSync(stateFile)) {
    let previous; try { previous = JSON.parse(fs.readFileSync(stateFile, 'utf8')); } catch { throw new Error('运行锁损坏，请确认程序已停止后删除 data/.runtime/instance.json。'); }
    let alive = true; try { process.kill(previous.pid, 0); } catch (error) { if (error.code === 'ESRCH') alive = false; }
    if (alive) throw new Error('本目录的程序已启动，请先运行 stop.cmd。');
    fs.unlinkSync(stateFile);
  }
  const controlToken = crypto.randomBytes(32).toString('hex');
  const record = { pid: process.pid, controlToken, controlPort: null, port, debug };
  fs.writeFileSync(stateFile, JSON.stringify(record), { flag: 'wx', mode: 0o600 });
  const children = []; let web, control, ending = false, failed = false;
  const shutdown = async code => {
    if (ending) return; ending = true; process.exitCode = code;
    web?.close(); web?.closeAllConnections(); control?.close();
    for (const child of children) if (child.exitCode === null) child.kill();
    await Promise.race([Promise.all(children.map(child => child.exitCode !== null ? Promise.resolve() : new Promise(resolve => child.once('exit', resolve)))), sleep(5000)]);
    for (const child of children) if (child.exitCode === null) child.kill('SIGKILL');
    try { if (JSON.parse(fs.readFileSync(stateFile, 'utf8')).controlToken === controlToken) fs.unlinkSync(stateFile); } catch {}
    console.log(code === 75 ? '更新已准备完成，正在重启，账号和采集数据保留。' : code ? '启动/运行失败，详情见 logs 目录。' : '服务已停止，数据已保留。');
    // All owned children are terminated before the supervisor exits.
    process.exit(code);
  };
  for (const signal of ['SIGINT', 'SIGTERM', 'SIGHUP']) process.on(signal, () => void shutdown(0));
  process.on('uncaughtException', error => { console.error(error.message); void shutdown(1); });
  process.on('unhandledRejection', error => { console.error(String(error)); void shutdown(1); });
  try {
    // A window close after staging must not strand a verified pending update.
    // The instance lock above ensures a second launch cannot switch a live app.
    if (process.env.DOUYIN_UPDATE_PROTOCOL === '1' && fs.existsSync(path.join(root, '.updates', 'pending-version'))) {
      await shutdown(75);return;
    }
    const backendPort = await freePort(), wsPort = await freePort(), settingsPort = await freePort();
    const origins = [`http://localhost:${port}`, `http://127.0.0.1:${port}`];
    const currentVersion = fs.readFileSync(path.join(appRoot, 'VERSION'), 'utf8').trim();
    const installRelease = process.platform === 'win32' && process.env.DOUYIN_UPDATE_PROTOCOL === '1'
      ? createPortableInstaller({ installRoot: root, appRoot, currentVersion, onReady: () => { setTimeout(() => void shutdown(75), 500); } }) : null;
    const updateService = createUpdateService({ currentVersion, installRelease });
    web = await createAuthServer({ dataDir: path.join(data, 'auth'), origins, wsHost: '127.0.0.1', wsPort, backendPort, settingsPort, webRoot: path.join(appRoot, 'web'), updateService });
    await new Promise((resolve, reject) => { web.once('error', reject); web.listen(port, '127.0.0.1', resolve); });
    control = http.createServer((req, res) => {
      if (req.headers['x-control-token'] !== controlToken) { res.writeHead(403); res.end(); return; }
      if (req.method !== 'POST' || req.url !== '/stop') { res.writeHead(404); res.end(); return; }
      res.end('stopping'); setImmediate(() => void shutdown(0));
    });
    await new Promise((resolve, reject) => { control.once('error', reject); control.listen(0, '127.0.0.1', resolve); });
    record.controlPort = control.address().port; fs.writeFileSync(stateFile, JSON.stringify(record), { mode: 0o600 });
    const env = { ...process.env, BIND_HOST: '127.0.0.1', HTTP_PORT: String(backendPort), WS_PORT: String(wsPort), SETTINGS_PORT: String(settingsPort),
      DATABASE_PATH: path.join(data, 'pipeline.db'), INTERNAL_TOKEN: crypto.randomBytes(32).toString('hex'),
      BACKEND_URL: `http://127.0.0.1:${backendPort}`, INGEST_TRANSPORT: 'local', AUTH_ORIGINS: origins.join(','),
      SPOOL_DIR: path.join(data, 'spool'), DOUYIN_ROOM_COOKIE_DIR: path.join(data, 'config', 'rooms') };
    delete env.RAW_CAPTURE_DIR; delete env.AMQP_URL;
    const launch = (name, file, params) => {
      const log = fs.openSync(path.join(root, 'logs', name + '.log'), 'a');
      let child;
      try { child = spawn(file, params, { cwd: root, env, windowsHide: true, stdio: ['ignore', log, log] }); } finally { fs.closeSync(log); }
      children.push(child);
      child.on('error', error => { failed = true; console.error(name + ': ' + error.message); void shutdown(1); });
      child.on('exit', code => { if (!ending) { failed = true; console.error(name + ' 已退出：' + code); void shutdown(1); } });
      return child;
    };
    launch('backend', path.join(appRoot, 'bin', 'douyin-pipeline.exe'), debug ? ['--debug'] : []);
    let healthy = false, maintenanceNotice = false, deadline = Date.now() + 15000;
    while (!failed && Date.now() < deadline) {
      let pause = 150;
      try {
        const response = await fetch(env.BACKEND_URL + '/health/live', { signal: AbortSignal.timeout(500) });
        if (response.ok) {
          const health = await response.json();
          healthy = health.status === 'ok';
          if (health.status === 'maintenance') {
            // A responsive VACUUM may take minutes. Keep waiting only while
            // the backend confirms it is alive and doing startup maintenance.
            deadline = Date.now() + 15000;
            pause = 1000;
            if (!maintenanceNotice) {
              console.log('正在整理数据库，完成后将自动启动采集，请保持程序运行。');
              maintenanceNotice = true;
            }
          }
        }
      } catch {}
      if (healthy) break; await sleep(pause);
    }
    if (!healthy) throw new Error('后端未能启动，请查看 logs/backend.log');
    launch('collector', process.execPath, [path.join(appRoot, 'app', 'collector', 'index.js')]);
    console.log(`直播台已启动：http://localhost:${port}`);
    console.log(debug ? 'Debug 模式：协议诊断已开启。' : '普通模式：协议诊断已隐藏。');
    console.log('首次访问创建管理员账号。停止请按 Ctrl+C 或运行 stop.cmd。');
    if (!args.includes('--no-browser')) spawn('rundll32.exe', ['url.dll,FileProtocolHandler', `http://localhost:${port}`], { windowsHide: true, stdio: 'ignore' }).on('error', () => {});
  } catch (error) { console.error(error.code === 'EADDRINUSE' ? `端口 ${port} 已占用，可运行 start.cmd --port 3001` : error.message); await shutdown(1); }
}
(args.includes('--stop') ? stopExisting() : start()).catch(error => { console.error(error.message); process.exitCode = 1; });
