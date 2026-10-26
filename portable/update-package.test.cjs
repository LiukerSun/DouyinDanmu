const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const crypto = require('node:crypto');
const { execFile } = require('node:child_process');
const { promisify } = require('node:util');
const { createPortableInstaller } = require('./update-package');
const execute = promisify(execFile);
const powershell = path.join(process.env.SystemRoot || 'C:\\Windows', 'System32/WindowsPowerShell/v1.0/powershell.exe');
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const nativeHost = path.resolve(__dirname, '../backend/build_portable/DouyinDanmu.exe');
const write = async (file, body) => { await fs.mkdir(path.dirname(file), { recursive: true });await fs.writeFile(file, body); };

async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'douyin-update-中文 空格-'));
  t.after(async () => { await fs.rm(root, { recursive: true, force: true }); });
  const app = path.join(root, 'installed');
  await write(path.join(app, 'app/portable/extract-update.ps1'), await fs.readFile(path.join(__dirname, 'extract-update.ps1')));
  await write(path.join(app, 'VERSION'), '2.5.0\n');
  await write(path.join(app, 'data/pipeline.db'), Buffer.from([0,1,2,3,255]));
  await write(path.join(app, 'data/pipeline.db-wal'), 'uncheckpointed transactions');
  await write(path.join(app, 'data/auth/account.json'), 'private account');
  await write(path.join(app, 'data/config/rooms/123.json'), 'private cookie');
  await write(path.join(app, 'logs/backend.log'), 'existing logs');
  const generator = path.join(root, 'zip.ps1');
  await write(generator, `param([string]$InputFile,[string]$ArchiveFile)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$entries = ConvertFrom-Json -InputObject ([IO.File]::ReadAllText($InputFile))
$file=[IO.File]::Open($ArchiveFile,[IO.FileMode]::CreateNew)
try { $zip=[IO.Compression.ZipArchive]::new($file,[IO.Compression.ZipArchiveMode]::Create,$true)
try { foreach($item in $entries) { $entry=$zip.CreateEntry($item.name);if($null -ne $item.attributes){$entry.ExternalAttributes=$item.attributes};$stream=$entry.Open();try{$bytes=[Convert]::FromBase64String($item.bytes);$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()} } }finally{$zip.Dispose()} }finally{$file.Dispose()}
`);
  let sequence = 0;
  async function zip(options = {}) {
    const files = { 'DouyinDanmu.exe':'fixture host', 'runtime/node.exe':'fixture runtime', 'bin/douyin-pipeline.exe':'fixture pipeline',
      'app/portable/launcher.js':'fixture launcher', 'app/portable/extract-update.ps1':'fixture extractor', 'app/portable/update-package.js':'fixture installer',
      'app/auth/server.js':'fixture auth', 'app/auth/update-service.js':'fixture updates', 'web/index.html':'fixture UI', 'VERSION':'2.6.0\n',
      'update.json': JSON.stringify({format:1,version:'2.6.0',min_host_protocol:options.protocol || 1}), ...options.files };
    const manifest = Object.entries(files).map(([name, value]) => ({ path:name, sha256:hash(value) }));
    if (options.badManifest) manifest[0].sha256='0'.repeat(64);
    files['manifest.json']=JSON.stringify(manifest);
    const entries = Object.entries(files).map(([name,value]) => ({ name:'Package/'+name, bytes:Buffer.from(value).toString('base64') }));
    entries.push(...(options.extraEntries || []).map(entry => ({bytes:Buffer.from('bad').toString('base64'),...entry})));
    const source=path.join(root, 'entries'+sequence+'.json'), archive=path.join(root, 'package'+sequence+++'.zip');
    await fs.writeFile(source,JSON.stringify(entries));
    await execute(powershell,['-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',generator,'-InputFile',source,'-ArchiveFile',archive],{windowsHide:true});
    return fs.readFile(archive);
  }
  const release = bytes => ({version:'2.6.0',tag:'v2.6.0',url:'https://github.com/LiukerSun/DouyinDanmu/releases/tag/v2.6.0',asset:{name:'DouyinDanmu-win-x64-portable.zip',url:'https://github.com/LiukerSun/DouyinDanmu/releases/download/v2.6.0/DouyinDanmu-win-x64-portable.zip',size:bytes.length,sha256:hash(bytes)}});
  return {root,app,zip,release};
}

test('verified ZIP becomes an isolated version while data, WAL, account, cookie and logs stay unchanged', async t => {
  const f=await fixture(t),bytes=await f.zip(),progress=[];let ready=0;
  const install=createPortableInstaller({installRoot:f.app,appRoot:f.app,currentVersion:'2.5.0',fetchImpl:async()=>new Response(new ReadableStream({start(controller){controller.enqueue(bytes.subarray(0,37));controller.enqueue(bytes.subarray(37));controller.close()}})),onReady:()=>{ready++}});
  await install(f.release(bytes),state=>progress.push(state.phase));
  const id=(await fs.readFile(path.join(f.app,'.updates/pending-version'),'utf8')).trim();
  assert.match(id,/^[a-f0-9]{32}$/);
  assert.equal(await fs.readFile(path.join(f.app,'.versions',id,'VERSION'),'utf8'),'2.6.0\n');
  assert.equal(await fs.readFile(path.join(f.app,'VERSION'),'utf8'),'2.5.0\n');
  assert.deepEqual(await fs.readFile(path.join(f.app,'data/pipeline.db')),Buffer.from([0,1,2,3,255]));
  assert.equal(await fs.readFile(path.join(f.app,'data/pipeline.db-wal'),'utf8'),'uncheckpointed transactions');
  assert.equal(await fs.readFile(path.join(f.app,'data/auth/account.json'),'utf8'),'private account');
  assert.equal(await fs.readFile(path.join(f.app,'data/config/rooms/123.json'),'utf8'),'private cookie');
  assert.equal(await fs.readFile(path.join(f.app,'logs/backend.log'),'utf8'),'existing logs');
  assert.equal(ready,1);assert.ok(progress.includes('verifying')&&progress.includes('staging'));assert.equal(progress.at(-1),'restarting');
  await assert.rejects(install(f.release(bytes)),/等待重启/);
});

test('download hash mismatch and untrusted redirect never publish a pending version', async t => {
  const f=await fixture(t),bytes=await f.zip();
  const args={installRoot:f.app,appRoot:f.app,currentVersion:'2.5.0',onReady:()=>assert.fail('Unexpected restart')};
  const release=f.release(bytes);release.asset.sha256='0'.repeat(64);
  await assert.rejects(createPortableInstaller({...args,fetchImpl:async()=>new Response(bytes)})(release),/校验失败/);
  await assert.rejects(createPortableInstaller({...args,fetchImpl:async()=>new Response(null,{status:302,headers:{location:'https://evil.example/update.zip'}})})(f.release(bytes)),/不受信任/);
  await assert.rejects(fs.access(path.join(f.app,'.updates/pending-version')),{code:'ENOENT'});
});

test('installer rejects downgrade and oversized release metadata before downloading', async t => {
  const f=await fixture(t),bytes=await f.zip();let calls=0;
  const installer=createPortableInstaller({installRoot:f.app,appRoot:f.app,currentVersion:'2.7.0',fetchImpl:async()=>{calls++;return new Response(bytes)},onReady:()=>assert.fail('Unexpected restart')});
  await assert.rejects(installer(f.release(bytes)),/更新的版本/);
  const huge=f.release(bytes);huge.version='2.8.0';huge.tag='v2.8.0';huge.asset.url=huge.asset.url.replace('v2.6.0','v2.8.0');huge.asset.size=513*1024*1024;
  await assert.rejects(installer(huge),/大小/);assert.equal(calls,0);
});

test('package contents and protocol are checked before any program is switched', async t => {
  const f=await fixture(t);
  for(const options of [
    {badManifest:true}, {protocol:2}, {files:{'VERSION':'9.0.0\n'}},
    {extraEntries:[{name:'Package/../outside.txt'}]}, {extraEntries:[{name:'Package/data/pipeline.db'}]},
    {extraEntries:[{name:'Package/web/INDEX.html'}]}, {extraEntries:[{name:'Package/web/link',attributes:-1577123840}]},
    {extraEntries:[{name:'Other/web/evil.js'}]}, {extraEntries:[{name:'Package/web\\outside.js'}]},
  ]) {
    const bytes=await f.zip(options);
    const install=createPortableInstaller({installRoot:f.app,appRoot:f.app,currentVersion:'2.5.0',fetchImpl:async()=>new Response(bytes),onReady:()=>assert.fail('Unexpected restart')});
    await assert.rejects(install(f.release(bytes)),/更新包校验失败/);
    await assert.rejects(fs.access(path.join(f.app,'.updates/pending-version')),{code:'ENOENT'});
  }
  assert.equal(await fs.readFile(path.join(f.app,'data/auth/account.json'),'utf8'),'private account');
});

test('updates cannot stage through a junction into another directory', async t => {
  const f=await fixture(t),bytes=await f.zip(),outside=path.join(f.root,'outside');await fs.mkdir(outside);
  await fs.symlink(outside,path.join(f.app,'.updates'),'junction');
  const install=createPortableInstaller({installRoot:f.app,appRoot:f.app,currentVersion:'2.5.0',fetchImpl:async()=>new Response(bytes),onReady:()=>assert.fail('Unexpected restart')});
  await assert.rejects(install(f.release(bytes)),/链接|重解析/);assert.deepEqual(await fs.readdir(outside),[]);
});

test('native bootstrap kills the old job, atomically switches, and preserves root data and arguments', async t => {
  await fs.access(nativeHost);
  const f=await fixture(t),id='b'.repeat(32),version=path.join(f.app,'.versions',id);
  await fs.copyFile(nativeHost,path.join(f.app,'DouyinDanmu.exe'));
  for(const root of [f.app,version]) {
    await fs.mkdir(path.join(root,'runtime'),{recursive:true});await fs.copyFile(process.execPath,path.join(root,'runtime/node.exe'));
  }
  for(const relative of ['bin/douyin-pipeline.exe','web/index.html','VERSION','update.json'])await write(path.join(version,relative),'fixture');
  await write(path.join(f.app,'app/portable/launcher.js'),`const fs=require('node:fs'),path=require('node:path'),{spawn}=require('node:child_process');const root=process.env.DOUYIN_INSTALL_ROOT;fs.mkdirSync(path.join(root,'.updates'),{recursive:true});fs.writeFileSync(path.join(root,'.updates/pending-version'),${JSON.stringify(id)});spawn(process.execPath,['-e',"setTimeout(()=>require('node:fs').writeFileSync(process.argv[1],'escaped'),2000)",path.join(root,'escaped-child')],{stdio:'ignore',windowsHide:true});setTimeout(()=>process.exit(75),100);`);
  await write(path.join(version,'app/portable/launcher.js'),`require('node:fs').writeFileSync(require('node:path').join(process.env.DOUYIN_INSTALL_ROOT,'result.json'),JSON.stringify({root:process.env.DOUYIN_INSTALL_ROOT,protocol:process.env.DOUYIN_UPDATE_PROTOCOL,script:__filename,args:process.argv.slice(2)}));`);
  await execute(path.join(f.app,'DouyinDanmu.exe'),['--port','43123','--no-browser'],{windowsHide:true,timeout:15000});
  const result=JSON.parse(await fs.readFile(path.join(f.app,'result.json'),'utf8'));
  assert.equal(result.root,f.app);assert.equal(result.protocol,'1');assert.deepEqual(result.args,['--port','43123','--no-browser']);assert.equal(result.script,path.join(version,'app/portable/launcher.js'));
  assert.equal((await fs.readFile(path.join(f.app,'.updates/active-version'),'utf8')).trim(),id);
  await new Promise(resolve=>setTimeout(resolve,2200));await assert.rejects(fs.access(path.join(f.app,'escaped-child')),{code:'ENOENT'});
  assert.equal(await fs.readFile(path.join(f.app,'data/auth/account.json'),'utf8'),'private account');
});

test('native bootstrap refuses invalid pending pointers without changing the active program', async t => {
  const f=await fixture(t);await fs.copyFile(nativeHost,path.join(f.app,'DouyinDanmu.exe'));
  await fs.mkdir(path.join(f.app,'runtime'));await fs.copyFile(process.execPath,path.join(f.app,'runtime/node.exe'));
  await write(path.join(f.app,'.updates/pending-version'),'../outside');
  await write(path.join(f.app,'app/portable/launcher.js'),'process.exit(75);');
  await assert.rejects(execute(path.join(f.app,'DouyinDanmu.exe'),[],{windowsHide:true,timeout:5000}),error=>error.code===1);
  await assert.rejects(fs.access(path.join(f.app,'.updates/active-version')),{code:'ENOENT'});
  assert.equal(await fs.readFile(path.join(f.app,'VERSION'),'utf8'),'2.5.0\n');
});
