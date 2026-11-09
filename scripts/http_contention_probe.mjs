// Real-device A/B/A comparison. Read-only browser: no simulated data or POSTs.
import {spawn} from 'node:child_process';
import {mkdir, readFile, writeFile, appendFile} from 'node:fs/promises';
import {resolve, basename} from 'node:path';

const root=resolve(import.meta.dirname,'..');
const option=(name,fallback)=>{const i=process.argv.indexOf(name);return i<0?fallback:process.argv[i+1];};
const runDir=resolve(root,option('--run-dir','captures/phase4-http-contention-01'));
if(!runDir.startsWith(resolve(root,'captures')+'/'))throw Error('Output must be under captures/');
const seconds=Number(option('--seconds','60'));
if(!Number.isInteger(seconds)||seconds<60||seconds>180)throw Error('Phase duration must be 60..180 seconds');
const primary=option('--primary','/dev/cu.usbmodem2101'),secondary=option('--secondary','/dev/cu.usbmodem1101');
const oscBind=option('--osc-bind','192.168.1.4');
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const report={started_utc:new Date().toISOString(),firmware_changed:false,operator_other_dashboards_closed:true,phases:[]};
await mkdir(runDir); // Do not overwrite an earlier comparison.
await mkdir(resolve(root,'.build/http-contention'),{recursive:true});

async function waitUntil(fn,milliseconds){
 const end=Date.now()+milliseconds;
 while(Date.now()<end){try{const value=await fn();if(value)return value;}catch{}await sleep(100);}
 throw Error('Readiness timeout');
}

async function browserLoad(origin,directory){
 await mkdir(directory);
 const profile=resolve(root,'.build/http-contention',basename(runDir));
 const child=spawn('/Applications/Brave Browser.app/Contents/MacOS/Brave Browser',[
  '--headless=new','--disable-gpu','--no-first-run','--no-default-browser-check',
  '--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'],{stdio:['ignore','ignore','pipe']});
 const result={origin,started_utc:new Date().toISOString(),states:[],network:{requests:0,sample_requests:0,failed:0,unexpected_methods:0}};
 const pending=new Map();let ws,next=0;
 const networkFile=resolve(directory,'browser-network.jsonl');await writeFile(networkFile,'',{flag:'wx'});
 let writes=Promise.resolve();
 const log=value=>{writes=writes.then(()=>appendFile(networkFile,JSON.stringify({utc:new Date().toISOString(),...value})+'\n'));};
 try{
  const url=await new Promise((resolve,reject)=>{
   let log='';const timer=setTimeout(()=>reject(Error('Browser startup timeout')),12000);
   child.stderr.on('data',b=>{log=(log+b).slice(-8192);const m=log.match(/DevTools listening on (ws:\/\/[^\s]+)/);if(m){clearTimeout(timer);resolve(m[1]);}});
   child.once('error',e=>{clearTimeout(timer);reject(e);});child.once('exit',c=>{clearTimeout(timer);reject(Error('Browser exited '+c));});
  });
  ws=new WebSocket(url);await new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;});
  const send=(method,params={},sessionId)=>new Promise((resolve,reject)=>{
   const id=++next,timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout '+method));},8000);
   pending.set(id,{resolve,reject,timer});ws.send(JSON.stringify({id,method,params,...(sessionId?{sessionId}:{})}));
  });
  ws.onmessage=event=>{
   const row=JSON.parse(event.data),p=pending.get(row.id);
   if(p){pending.delete(row.id);clearTimeout(p.timer);row.error?p.reject(Error(JSON.stringify(row.error))):p.resolve(row.result);return;}
   const e=row.params;
   if(row.method==='Fetch.requestPaused'){
    const allowed=['GET','HEAD'].includes(e.request.method);
    if(!allowed){result.network.unexpected_methods++;log({event:'blocked_method',method:e.request.method,url:e.request.url});}
    send(allowed?'Fetch.continueRequest':'Fetch.failRequest',{requestId:e.requestId,...(allowed?{}:{errorReason:'Aborted'})},row.sessionId).catch(()=>{});
   }
   if(row.method==='Network.requestWillBeSent'){
    result.network.requests++;if(e.request.url.includes('/api/sample'))result.network.sample_requests++;
    log({event:'request',session:row.sessionId,id:e.requestId,method:e.request.method,url:e.request.url,timestamp:e.timestamp});
   }
   if(row.method==='Network.responseReceived')log({event:'response',session:row.sessionId,id:e.requestId,url:e.response.url,status:e.response.status,timestamp:e.timestamp,timing:e.response.timing});
   if(row.method==='Network.loadingFinished')log({event:'finished',session:row.sessionId,id:e.requestId,timestamp:e.timestamp,bytes:e.encodedDataLength});
   if(row.method==='Network.loadingFailed'){result.network.failed++;log({event:'failed',session:row.sessionId,id:e.requestId,error:e.errorText,canceled:e.canceled,timestamp:e.timestamp});}
  };
  const started=Date.now();let target,session;
  const evaluate=async expression=>{const r=await send('Runtime.evaluate',{expression,returnByValue:true},session);if(r.exceptionDetails)throw Error('Browser JS exception');return r.result.value;};
  const open=async()=>{
   target=(await send('Target.createTarget',{url:'about:blank'})).targetId;
   session=(await send('Target.attachToTarget',{targetId:target,flatten:true})).sessionId;
   for(const method of ['Page.enable','Runtime.enable','Network.enable'])await send(method,{},session);
   await send('Fetch.enable',{patterns:[{urlPattern:origin+'/*',requestStage:'Request'}]},session);
   await send('Target.activateTarget',{targetId:target});await send('Page.bringToFront',{},session);
  };
  const navigate=async page=>{
   log({event:'navigation',page});await send('Page.navigate',{url:origin+'/?page='+page},session);
   await waitUntil(async()=>evaluate(`document.querySelectorAll('.card').length>0&&new URL(location.href).searchParams.get('page')===${JSON.stringify(page)}`),8000);
   if(page==='motion')await waitUntil(async()=>evaluate(`document.querySelector('#stream-status')?.textContent==='Live'`),8000);
   result.states.push({utc:new Date().toISOString(),page,...await evaluate(`({visibility:document.visibilityState,cards:document.querySelectorAll('.card').length,stream:document.querySelector('#stream-status')?.textContent??null})`)});
  };
  for(const [at,action] of [[0,'open'],[8,'motion'],[25,'close'],[30,'open'],[38,'motion'],[55,'close']]){
   await sleep(Math.max(0,started+at*1000-Date.now()));
   if(action==='open'){await open();await navigate('overview');}
   else if(action==='motion')await navigate('motion');
   else{await send('Target.closeTarget',{targetId:target});target=null;session=null;}
  }
  result.result=result.states.length===4&&result.network.sample_requests>0&&!result.network.unexpected_methods?'PASS':'FAIL';
 }catch(error){result.result='FAIL';result.error=String(error);}
 finally{
  result.finished_utc=new Date().toISOString();
  if(ws)ws.close();for(const p of pending.values()){clearTimeout(p.timer);p.reject(Error('Browser closed'));}pending.clear();
  child.kill('SIGTERM');
  if(child.exitCode===null)await Promise.race([new Promise(r=>child.once('exit',r)),sleep(3000)]);
  if(child.exitCode===null)child.kill('SIGKILL');
  await writes;await writeFile(resolve(directory,'browser-report.json'),JSON.stringify(result,null,2)+'\n');
 }
 return result;
}

for(const [index,name] of ['A-probe-only','B-probe-browser','A2-recovery'].entries()){
 const phase={name,started_utc:new Date().toISOString()};report.phases.push(phase);
 await writeFile(resolve(runDir,'status.json'),JSON.stringify({state:'running',phase:name,utc:phase.started_utc},null,2));
 const directory=resolve(runDir,name),logfile=resolve(root,'.build/http-contention',basename(runDir)+'-'+index+'.log');
 const fs=await import('node:fs');const output=fs.openSync(logfile,'wx');
 const child=spawn(resolve(root,'.venv/bin/python'),['-B','tools/endurance_test.py','--primary',primary,'--secondary',secondary,
  '--osc-bind',oscBind,'--osc-port','16888','--seconds',String(seconds),'--run-dir',directory,'--trace-http'],{cwd:root,stdio:['ignore',output,output]});
 fs.closeSync(output);
 const ended=new Promise((resolve,reject)=>{child.once('exit',code=>resolve(code));child.once('error',reject);});
 try{
  const status=await waitUntil(async()=>{const s=JSON.parse(await readFile(resolve(directory,'status.json'),'utf8'));if(s.state==='finished')throw Error('Probe ended before readiness');return s.state==='running'?s:null;},12000);
  if(index===1){
   const initial=JSON.parse(await readFile(resolve(directory,'initial.json'),'utf8'));
   phase.browser=await browserLoad('http://'+initial.primary.STATUS.connectivity.sta_ip,resolve(directory,'browser'));
  }
  phase.exit_code=await ended;
 }catch(error){phase.error=String(error);child.kill('SIGTERM');await Promise.race([ended,sleep(6000)]);if(child.exitCode===null)child.kill('SIGKILL');}
 try{
  const r=JSON.parse(await readFile(resolve(directory,'report.json'),'utf8'));
  phase.result=r.result;phase.completed=r.completed;phase.elapsed_seconds=r.elapsed_seconds;
  phase.statistics=r.statistics;phase.imu_window=r.windows.primary;phase.http_hz=r.observed_http_hz;
  phase.tests=r.tests;
 }catch(error){phase.result='FAIL';phase.read_error=String(error);}
 phase.finished_utc=new Date().toISOString();
 await writeFile(resolve(runDir,'report.json'),JSON.stringify(report,null,2)+'\n');
 console.log(JSON.stringify({phase:name,result:phase.result,counts:phase.statistics?.counts,browser:phase.browser?.result}));
 if(index===1)await sleep(6000); // Native HTTP has an absolute 5 s client timeout.
}
report.finished_utc=new Date().toISOString();
report.note='A short comparison can reproduce errors but cannot prove absence of rare failures. No firmware/settings changes.';
await writeFile(resolve(runDir,'report.json'),JSON.stringify(report,null,2)+'\n');
await writeFile(resolve(runDir,'status.json'),JSON.stringify({state:'finished',finished_utc:report.finished_utc},null,2));
const rows=report.phases.map(p=>`| ${p.name} | ${p.result} | ${p.statistics?.counts.http_valid??0}/${p.statistics?.counts.http_requests??0} | ${p.statistics?.counts.error_http??0} | ${p.http_hz?.toFixed(3)??'N/D'} |`);
await writeFile(resolve(runDir,'summary.md'),['# HTTP concurrency comparison','','| Phase | Result | Valid HTTP / requests | HTTP errors | Valid HTTP Hz |','|---|---|---|---|---|',...rows,'',report.note,''].join('\n'));
