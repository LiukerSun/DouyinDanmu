const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const net = require('node:net');
const crypto = require('node:crypto');
const release = path.resolve(process.argv[2]);
const testRoot = fs.mkdtempSync(path.join(os.tmpdir(), '直播台 便携验收-'));
const app = path.join(testRoot, '解压目录 with spaces');
fs.cpSync(release, app, { recursive: true, filter: source => !source.startsWith(path.join(release, 'data')) && !source.startsWith(path.join(release, 'logs')) });
const node = path.join(app, 'runtime', 'node.exe');
const WebSocket = require(path.join(app, 'app/collector/node_modules/ws'));
const protobuf = require(path.join(app, 'app/collector/node_modules/protobufjs'));
const children = [];
const env = { ...process.env, PATH: path.join(process.env.SystemRoot, 'System32') };
for (const key of ['NODE_PATH','NODE_OPTIONS','AMQP_URL','BACKEND_URL','INTERNAL_TOKEN','DATABASE_PATH','SPOOL_DIR','AUTH_DATA_DIR']) delete env[key];
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(fn, label) { for(let i=0;i<100;i++){try{const result=await fn();if(result)return result;}catch{}await wait(200);}throw new Error('Timed out: '+label); }
async function availablePort() { const s=net.createServer();await new Promise(r=>s.listen(0,'127.0.0.1',r));const p=s.address().port;await new Promise(r=>s.close(r));return p; }
function run(file,args=[]) { const child=spawn(file,args,{cwd:app,env,windowsHide:true,stdio:['ignore','pipe','pipe']});children.push(child);let output='';child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);child.output=()=>output;return child; }
(async()=>{
  const port=await availablePort(),base=`http://127.0.0.1:${port}`;
  let cookie='';
  const request=async(endpoint,method='GET',body)=>{
    const r=await fetch(base+endpoint,{method,headers:{Cookie:cookie,Origin:base,'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(5000)});
    if(r.headers.get('set-cookie'))cookie=r.headers.get('set-cookie').split(';')[0];
    return {status:r.status,data:await r.json()};
  };
  const launch=async debug=>{const child=run(path.join(app,'DouyinDanmu.exe'),['--port',String(port),'--no-browser',...(debug?['--debug']:[])]);await until(async()=>{if(child.exitCode!==null)throw Error(child.output());return(await fetch(base+'/health/live')).ok;},'start');return child;};
  const stop=async child=>{const task=run(process.env.ComSpec,['/d','/c','stop.cmd']);await until(()=>task.exitCode!==null,'stop command');assert.equal(task.exitCode,0,task.output());await until(()=>child.exitCode!==null,'process tree shutdown');assert.equal(child.exitCode,0,child.output());assert(!fs.existsSync(path.join(app,'data/.runtime/instance.json')));};
  let child=await launch(false);
  assert.equal((await request('/api/rooms')).status,401);
  assert.equal((await request('/api/rooms/demo/remove','POST')).status,401);
  assert.equal((await request('/internal/targets')).status,404);
  const page=await fetch(base+'/');assert.equal(page.status,200);assert((await page.text()).includes('root'));
  assert.equal((await request('/api/auth/setup','POST',{username:'portable-check',displayName:'便携验收',password:'Portable-test-2026!'})).status,200);
  await until(async()=>(await request('/api/health')).data.collector.online,'collector heartbeat');
  assert.equal((await request('/api/health')).data.transport,'local');
  const duplicate=run(path.join(app,'DouyinDanmu.exe'),['--port',String(port),'--no-browser']);await until(()=>duplicate.exitCode!==null,'duplicate launch');assert.notEqual(duplicate.exitCode,0);
  assert.equal((await request('/api/rooms','POST',{live_id:'demo'})).status,202);
  const batch=await new Promise((resolve,reject)=>{const ws=new WebSocket(base.replace('http:','ws:')+'/ws',{headers:{Cookie:cookie,Origin:base}});const timeout=setTimeout(()=>{ws.close();reject(Error('WebSocket timeout'));},15000);ws.on('open',()=>ws.send(JSON.stringify({action:'subscribe',live_id:'demo',after_seq:'0'})));ws.on('message',raw=>{const value=JSON.parse(raw);if(value.type==='event_batch'&&value.events.length){clearTimeout(timeout);ws.close();resolve(value);}});ws.on('error',reject);});
  assert(batch.events.every(e=>e.live_id==='demo'));const event=batch.events[0];
  assert.equal((await request('/api/messages/detail?event_id='+encodeURIComponent(event.event_id))).data.debug_mode,false);
  // Stop capture, then ensure every fsynced record has received its SQLite receipt.
  await request('/api/rooms/demo','DELETE');
  await until(()=>fs.readdirSync(path.join(app,'data/spool')).filter(n=>n.endsWith('.bin')).length===0,'durable receipts');
  const before=(await request('/api/rooms/demo/snapshot')).data;
  assert.equal((await request('/api/rooms')).data.some(room=>room.live_id==='demo'),true,'Pause keeps the room listed');
  assert.equal((await request('/api/rooms/demo/remove','POST')).status,200);
  assert.equal((await request('/api/rooms/demo/remove','POST')).status,200,'Repeated removal is idempotent');
  assert.equal((await request('/api/rooms')).data.some(room=>room.live_id==='demo'),false,'Removal hides the room');
  await until(async()=>(await request('/api/health')).data.collector.sessions===0,'removed collector session stopped');
  await stop(child);
  // A recovered spool record must be decoded once, stored, and removed after commit.
  const proto=await protobuf.load([path.join(app,'app/collector/proto/douyin.proto'),path.join(app,'app/collector/proto/ingest.proto')]);
  const payload=Buffer.from(JSON.stringify({status:'collecting',detail:'recovered fixture'}));
  const id=crypto.randomUUID(),Envelope=proto.lookupType('pipeline.RawFrameEnvelope');
  const frame=Buffer.from(Envelope.encode(Envelope.fromObject({schemaVersion:1,frameId:id,liveId:'demo',roomId:'demo',source:'demo',sessionId:'portable-test',sessionSeq:'1',receivedAtMs:String(Date.now()),desiredVersion:'1',kind:'collector_status',payload,payloadSha256:crypto.createHash('sha256').update(payload).digest('hex')})).finish());
  const spool=path.join(app,'data/spool',id+'.bin');fs.writeFileSync(spool,frame);
  child=await launch(true);cookie='';
  assert.equal((await request('/api/auth/session')).data.setupRequired,false);
  assert.equal((await request('/api/auth/login','POST',{username:'portable-check',password:'Portable-test-2026!'})).status,200);
  assert.equal((await request('/api/rooms')).data.some(room=>room.live_id==='demo'),false,'Removed room stays hidden after restart');
  assert.equal((await request('/api/messages/detail?event_id='+encodeURIComponent(event.event_id))).data.debug_mode,true);
  assert(BigInt((await request('/api/rooms/demo/snapshot')).data.through_seq)>=BigInt(before.through_seq));
  await until(()=>!fs.existsSync(spool),'recovered spool committed');
  const {DatabaseSync}=require('node:sqlite');const db=new DatabaseSync(path.join(app,'data/pipeline.db'),{readOnly:true});
  assert.equal(db.prepare('SELECT count(*) AS n FROM receipts WHERE frame_id=?').get(id).n,1);db.close();
  assert.equal((await request('/api/rooms')).data.some(room=>room.live_id==='demo'),false,'Recovered spool cannot restore a removed room');
  assert.equal((await request('/api/rooms','POST',{live_id:'demo'})).status,202);
  await until(async()=>(await request('/api/health')).data.collector.sessions===1,'re-added collector session started');
  await until(async()=>BigInt((await request('/api/rooms/demo/snapshot')).data.through_seq)>BigInt(before.through_seq),'re-added room continues stored history');
  assert.equal((await request('/api/messages/detail?event_id='+encodeURIComponent(event.event_id))).status,200,'Re-add retains original details');
  assert.equal((await request('/api/rooms/demo/remove','POST')).status,200);
  await until(async()=>(await request('/api/health')).data.collector.sessions===0,'enabled room removal stops collection');
  // Closing the native host must also terminate the backend and collector.
  child.kill();await until(()=>child.exitCode!==null || child.signalCode!==null,'host terminated');
  await until(async()=>{try{await fetch(base+'/health/live',{signal:AbortSignal.timeout(500)});return false;}catch{return true;}},'job cleanup');
  child=run(process.env.ComSpec,['/d','/c',`start-debug.cmd --port ${port} --no-browser`]);
  await until(async()=>(await fetch(base+'/health/live')).ok,'debug startup script');
  cookie='';await request('/api/auth/login','POST',{username:'portable-check',password:'Portable-test-2026!'});
  await until(async()=>(await request('/api/health')).data.debug_mode===true,'debug script parameter forwarding');
  await stop(child);
  console.log('PASS: native host, clean PATH, Chinese/spaced extraction path, first account, protected APIs, collector, SQLite, per-room WebSocket, debug restart, persistent history, pause/removal distinction, persistent removal, collector shutdown/re-add, spool recovery without resurrection, duplicate prevention, stop script and job cleanup.');
  console.log('Isolated validation files: '+app);
})().catch(error=>{console.error(error);for(const child of children)if(child.exitCode===null)child.kill();console.error('Validation files: '+app);process.exitCode=1;});
