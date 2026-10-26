const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { EventEmitter } = require('node:events');
const { test } = require('node:test');

const source = fs.readFileSync(path.join(__dirname, 'launcher.js'), 'utf8');

// Run the actual supervisor with virtual time and owned, in-memory processes,
// files and sockets. No user data or running service is touched.
async function runLauncher(health, options = {}) {
  let elapsed = 0, nextPort = 12000, finish;
  const finished = new Promise(resolve => { finish = resolve; });
  const files = new Map(), messages = [], launches = [], kills = [];
  if (options.pending) files.set(path.join(options.env.DOUYIN_INSTALL_ROOT, '.updates', 'pending-version'), 'a'.repeat(32));
  const server = () => Object.assign(new EventEmitter(), {
    listen(...args) { args.at(-1)(); },
    address() { return { port: ++nextPort }; },
    close(callback) { callback?.(); },
    closeAllConnections() {},
  });
  const fakeFs = {
    existsSync: file => files.has(file), mkdirSync() {},
    writeFileSync: (file, value) => files.set(file, value),
    readFileSync: file => file.endsWith('VERSION') ? '2.5.0\n' : files.get(file), unlinkSync: file => files.delete(file),
    openSync: () => 1, closeSync() {},
  };
  const fakeProcess = {
    argv: ['node', 'launcher.js', '--no-browser'], env: options.env || {}, platform: 'win32', pid: 1, execPath: 'node.exe',
    on() {}, kill() {}, exit: code => finish({ exitCode: code }),
  };
  const spawn = (file, args) => {
    const name = file.endsWith('douyin-pipeline.exe') ? 'backend' : 'collector';
    launches.push({ name, file, args, at: elapsed });
    const child = new EventEmitter();
    child.exitCode = null;
    child.kill = () => {
      kills.push({ name, at: elapsed }); child.exitCode = 1; child.emit('exit', 1);
    };
    if (name === 'collector') finish({ started: true });
    return child;
  };
  let authOptions, installerOptions, serviceOptions;
  const modules = {
    'node:fs': fakeFs, 'node:path': path, 'node:net': { createServer: server },
    'node:http': { createServer: server }, 'node:crypto': { randomBytes: () => Buffer.alloc(32) },
    'node:child_process': { spawn }, '../auth/server': { createAuthServer: async options => { authOptions = options; return server(); } },
    '../auth/update-service': { createUpdateService: options => { serviceOptions = options; return { updateService: true }; } },
    './update-package': { createPortableInstaller: options => { installerOptions = options; return async () => {}; } },
  };
  vm.runInNewContext(source, {
    require: name => { assert.ok(modules[name], `Unexpected import: ${name}`); return modules[name]; },
    __dirname: options.appDirectory || __dirname, process: fakeProcess, Buffer, AbortSignal,
    Date: { now: () => elapsed },
    console: { log: message => messages.push(message), error: message => messages.push(message) },
    fetch: async () => {
      assert.ok(elapsed < 180000, 'Startup must finish or time out');
      const status = health(elapsed);
      if (status instanceof Error) throw status;
      return { ok: true, json: async () => ({ status }) };
    },
    setImmediate,
    setTimeout: (callback, milliseconds) => { elapsed += milliseconds; queueMicrotask(callback); },
  });
  const result = await finished;
  await new Promise(resolve => setImmediate(resolve));
  return { ...result, messages, launches, kills, authOptions, installerOptions, serviceOptions };
}

test('waits through minutes of responsive maintenance before starting collection', async () => {
  const result = await runLauncher(elapsed => elapsed < 120000 ? 'maintenance' : 'ok');
  assert.equal(result.started, true);
  assert.ok(result.launches.find(child => child.name === 'collector').at >= 120000);
  assert.deepEqual(result.kills, []);
  assert.equal(result.messages.filter(message => message.includes('正在整理数据库')).length, 1);
});

test('an unavailable backend still times out', async () => {
  const result = await runLauncher(() => new Error('ECONNREFUSED'));
  assert.equal(result.exitCode, 1);
  assert.deepEqual(result.launches.map(child => child.name), ['backend']);
  assert.ok(result.kills[0].at >= 15000 && result.kills[0].at <= 16000);
});

test('maintenance does not disable the timeout after the backend stops responding', async () => {
  const result = await runLauncher(elapsed => elapsed < 30000 ? 'maintenance' : new Error('ECONNREFUSED'));
  assert.equal(result.exitCode, 1);
  assert.deepEqual(result.launches.map(child => child.name), ['backend']);
  assert.ok(result.kills[0].at >= 44000 && result.kills[0].at <= 46000);
});

test('HTTP 200 with a starting status is not readiness', async () => {
  const result = await runLauncher(() => 'starting');
  assert.equal(result.exitCode, 1);
  assert.deepEqual(result.launches.map(child => child.name), ['backend']);
});

test('the existing ok health response starts collection immediately', async () => {
  const result = await runLauncher(() => 'ok');
  assert.equal(result.started, true);
  assert.equal(result.launches.find(child => child.name === 'collector').at, 0);
  assert.deepEqual(result.kills, []);
});

test('an activated version uses new program files and the original user data root', async () => {
  const installRoot = path.resolve('fixture-install');
  const appRoot = path.join(installRoot, '.versions', 'a'.repeat(32));
  const result = await runLauncher(() => 'ok', { appDirectory: path.join(appRoot, 'app', 'portable'),
    env: { DOUYIN_INSTALL_ROOT: installRoot, DOUYIN_UPDATE_PROTOCOL: '1' } });
  assert.equal(result.authOptions.dataDir, path.join(installRoot, 'data', 'auth'));
  assert.equal(result.authOptions.webRoot, path.join(appRoot, 'web'));
  assert.equal(result.launches[0].file, path.join(appRoot, 'bin', 'douyin-pipeline.exe'));
  assert.equal(result.launches[1].args[0], path.join(appRoot, 'app', 'collector', 'index.js'));
  assert.equal(result.installerOptions.installRoot, installRoot);
  assert.equal(result.installerOptions.appRoot, appRoot);
  assert.equal(result.serviceOptions.currentVersion, '2.5.0');
  assert.equal(typeof result.serviceOptions.installRelease, 'function');
});

test('a verified update interrupted before restart resumes without starting old services', async () => {
  const result = await runLauncher(() => 'ok', { pending: true,
    env: { DOUYIN_INSTALL_ROOT: path.resolve('fixture-install'), DOUYIN_UPDATE_PROTOCOL: '1' } });
  assert.equal(result.exitCode, 75);
  assert.deepEqual(result.launches, []);
  assert.ok(result.messages.some(message => message.includes('更新已准备完成')));
});
