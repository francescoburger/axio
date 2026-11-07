// Fixture presentation QA or --runtime: real C++ runtime with fake hardware.
import {spawn} from 'node:child_process';
import {mkdir,writeFile,readFile} from 'node:fs/promises';
import {createServer} from 'node:http';
import {resolve} from 'node:path';
const root=resolve(import.meta.dirname,'..');
const runtimeMode=process.argv.includes('--runtime');
const matrixMode=process.argv.includes('--matrix');
const dialogs=[];
const originIndex=process.argv.indexOf('--origin');
const liveOrigin=originIndex<0?null:process.argv[originIndex+1];
const artifactDir=liveOrigin?'phase4-device-ui':runtimeMode?'phase4-runtime-ui':'phase4-ui';
let backend;
await mkdir(resolve(root,'.build/phase4-browser'),{recursive:true});
await mkdir(resolve(root,'captures',artifactDir),{recursive:true});
let fixtureData,sampleData;
const server=createServer(async(req,res)=>{
 const path=new URL(req.url,'http://localhost').pathname;
 if(path.startsWith('/api/')) {
  res.setHeader('Content-Type','application/json');
  const value=path==='/api/session'?{token:'qa'}:path==='/api/snapshot'?fixtureData:path==='/api/sample'?sampleData:path==='/api/battery'?{percent:68}:{ok:true};
  res.end(JSON.stringify(value??{}));return;
 }
 try {const name={'/':'index.html','/style.css':'style.css','/app.js':'app.js'}[path];if(!name){res.writeHead(404).end();return;}
  const data=await readFile(resolve(root,'firmware/Axio/web',name));res.setHeader('Content-Type',name.endsWith('.js')?'application/javascript':name.endsWith('.css')?'text/css':'text/html');res.end(data);
 }catch{res.writeHead(500).end();}
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
let origin='http://127.0.0.1:'+server.address().port;
if(runtimeMode){
 backend=spawn(resolve(root,'.build/tests/dashboard_runtime'),[],{stdio:['ignore','pipe','pipe']});
 await new Promise((resolve,reject)=>{backend.stdout.once('data',resolve);backend.once('error',reject);backend.once('exit',code=>reject(Error('Backend exited '+code)));});
 origin='http://127.0.0.1:17884';
}
if(liveOrigin)origin=liveOrigin;
const browser=spawn('/Applications/Brave Browser.app/Contents/MacOS/Brave Browser',['--headless=new','--disable-gpu','--no-first-run','--no-default-browser-check','--remote-debugging-port=0','--user-data-dir='+resolve(root,'.build/phase4-browser'),'about:blank'],{stdio:['ignore','ignore','pipe']});
let ws;
try {
 const url=await new Promise((resolve,reject)=>{let log='';const timeout=setTimeout(()=>reject(Error('Browser startup timeout')),15000);browser.stderr.on('data',b=>{log+=b;const m=log.match(/DevTools listening on (ws:\/\/[^\s]+)/);if(m){clearTimeout(timeout);resolve(m[1]);}});browser.on('error',reject);});
 ws=new WebSocket(url);await new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;});
 let next=0;const pending=new Map();let dismissDialog;ws.onmessage=event=>{const row=JSON.parse(event.data);if(row.method==='Page.javascriptDialogOpening'&&dismissDialog)dismissDialog(row.params);if(row.id){const p=pending.get(row.id);if(p){pending.delete(row.id);row.error?p.reject(Error(JSON.stringify(row.error))):p.resolve(row.result);}}};
 const send=(method,params={},sessionId)=>new Promise((resolve,reject)=>{const id=++next;pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params,...(sessionId?{sessionId}:{})}));});
 const target=await send('Target.createTarget',{url:'about:blank'}),attached=await send('Target.attachToTarget',{targetId:target.targetId,flatten:true}),session=attached.sessionId;
 const cdp=(method,params)=>send(method,params,session);
 const evaluate=async expression=>{const r=await cdp('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
 dismissDialog=params=>{dialogs.push(params.message);cdp('Page.handleJavaScriptDialog',{accept:false}).catch(()=>{});};
 const capture=async name=>{const m=await cdp('Page.getLayoutMetrics');const r=await cdp('Page.captureScreenshot',{format:'png',captureBeyondViewport:true,clip:{x:0,y:0,width:430,height:m.cssContentSize.height,scale:1}});await writeFile(resolve(root,'captures',artifactDir,name+'.png'),Buffer.from(r.data,'base64'));};
 await cdp('Page.enable');await cdp('Runtime.enable');await cdp('Emulation.setDeviceMetricsOverride',{width:430,height:860,deviceScaleFactor:2,mobile:false});
 const fixture={device:{id:'axio-001',mac:'A4:C1:38:7F:2E:9B',ip:'192.168.1.42',hostname:'axio-a4c1387f2e9b.local',ap_ssid:'Axio-axio-001',osc_status:'Sending',version:'Phase 3'},status:{wifi_connected:true,imu:'Active'},peers:[{id:'axio-002',ip:'192.168.1.43'},{id:'axio-003',ip:'192.168.1.44'}],osc:{schema:2,id:'axio-001',destinations:[{ip:'192.168.1.50',port:9000},{ip:'192.168.1.51',port:9001}],listen_port:16889,tare_address:'/tare/{id}',ack:{enabled:false,address:'/tared/{id}',destination:0},editor:{main_address:'/motion/{id}',suffixes:['','real','world','gyro','quat'],selected:[7,7,7,7,15],interval_ms:20}},network:{connected:true,ssid:'Studio WiFi',ip:'192.168.1.42',gateway:'192.168.1.1',subnet:'255.255.255.0',rssi:-58,entry_point:'axio.local',ap_ip:'192.168.4.1',revision:1,networks:[{ssid:'Studio WiFi',original:0},{ssid:'Mobile Hotspot',original:1}]},diagnostics:{uptime:'02h 34m 18s',heap_free:'112.4 KB',heap_min:'32.5 KB',flash_used:'1.10 MB',imu_hz:99.95,max_interval_us:78000,osc_hz:100,destinations:1,udp_errors:0,wifi_mode:'STA',mdns:'Active',samples:1000,datagrams:1000,reconnects:0}};
 const sample={valid:true,yaw:.624,pitch:-.173,roll:.286,axReal:.047,ayReal:-.328,azReal:.983,axWorld:.011,ayWorld:.065,azWorld:-.997,gx:-.211,gy:.137,gz:-.049,q0:.923879,q1:.080438,q2:-.356195,q3:.133306};
 fixtureData=fixture;sampleData=sample;
 const results=[];
 for(const page of ['overview','motion','osc','network','diagnostics']) {
  await cdp('Page.navigate',{url:origin+'/?page='+page});
  for(let i=0;i<100;++i){try{if(await evaluate('typeof boot === "function"'))break;}catch{}await new Promise(r=>setTimeout(r,25));}
  if(!runtimeMode&&!liveOrigin)await evaluate(`api=async(path,body)=>{if(path==='session')return {token:'qa'};if(path==='snapshot')return ${JSON.stringify(fixture)};if(path==='battery')return {percent:68};if(path==='sample')return ${JSON.stringify(sample)};return {ok:true};};boot()`);
  for(let i=0;i<100;++i){if(await evaluate('document.querySelectorAll(".card").length>0'))break;await new Promise(r=>setTimeout(r,25));}
  if(runtimeMode||liveOrigin){
   // Allow the first 16 ADC conversions; no demo battery fallback on a device.
   for(let i=0;i<20;++i){if(await evaluate(`document.querySelector('#battery').textContent!=='—'`))break;await new Promise(r=>setTimeout(r,100));}
   const measured=await evaluate(`(async()=>{const b=await api('battery');return {voltage:b.voltage,text:document.querySelector('#battery').textContent};})()`);
   if(Number.isFinite(measured.voltage)&&measured.text!==measured.voltage.toFixed(2)+' V')throw Error(page+' measured voltage missing');
  }
  if(page==='motion')await evaluate(`document.querySelector('#show-3d').click()`);
  await evaluate('new Promise(r=>setTimeout(r,200))');
  const metrics=await cdp('Page.getLayoutMetrics');
  const shot=await cdp('Page.captureScreenshot',{format:'png',captureBeyondViewport:true,clip:{x:0,y:0,width:430,height:metrics.cssContentSize.height,scale:1}});
  await writeFile(resolve(root,`captures/${artifactDir}/${page}.png`),Buffer.from(shot.data,'base64'));
  const check=await evaluate(`({cards:[...document.querySelectorAll('.card h2')].map(e=>e.textContent),battery:document.querySelector('#battery').textContent,overflow:document.documentElement.scrollWidth>430})`);
  if(check.overflow)throw Error(page+' horizontal overflow');
  if(page==='motion') {const liveDot=await evaluate(`!document.querySelector('#stream-status').previousElementSibling.classList.contains('offline')`);if(!liveDot)throw Error('Live indicator still offline');const hidden=await evaluate(`document.querySelector('#show-3d').click();document.querySelector('#advanced').hidden`);if(!hidden)throw Error('Quaternion/3D visibility');}
  if(page==='osc'){const count=await evaluate(`document.querySelector('#ack-enabled').click();document.querySelector('#ack-settings').hidden`);if(count)throw Error('ACK visibility');}
  if(matrixMode){
   if(page==='overview'){
    await evaluate(`document.querySelector('#tracker-id').value='draft-example';document.querySelector('#tracker-id').focus()`);await capture('overview-editing');
    await evaluate(`document.querySelector('#restart').click()`);await evaluate(`document.querySelector('#sleep').click()`);
   }
   if(page==='motion'){
    await capture('motion-quaternion-off');await evaluate(`document.querySelector('#pause').click()`);await capture('motion-paused');
   }
   if(page==='osc'){
    await capture('osc-ack-on');
    await evaluate(`for(let i=0;i<8;i++)document.querySelector('#add-destination').click()`);await capture('osc-eight-destinations');
    const bounded=await evaluate(`document.querySelector('#add-destination').disabled&&document.querySelectorAll('[data-remove]').length===8`);if(!bounded)throw Error('Destination bound');
   }
   if(page==='network'){
    await evaluate(`while(document.querySelector('[data-delete]'))document.querySelector('[data-delete]').click()`);await capture('network-empty');
    await evaluate(`for(let i=0;i<8;i++)document.querySelector('#add-network').click()`);await capture('network-eight');
    const bounded=await evaluate(`document.querySelector('#add-network').disabled`);if(!bounded)throw Error('Network bound');
    await evaluate(`document.querySelector('#ap-eye').click()`);if(await evaluate(`document.querySelector('#ap-password').type`)!=='text')throw Error('Password visibility');
   }
   if(page==='diagnostics'){await evaluate(`document.querySelector('#details-toggle').click()`);await capture('diagnostics-details');}
   if(!liveOrigin&&!runtimeMode){
    for(const [label,data,text,level] of [['68',{percent:68},'68%','normal'],['15',{percent:15},'15%','low'],['5',{percent:5},'5%','critical'],['voltage',{percent:null,voltage:3.506},'3.51 V','voltage'],['unavailable',{percent:null,voltage:null},'—','unavailable']]){
     await evaluate(`(async()=>{const previous=api;api=async(path,body)=>path==='battery'?${JSON.stringify(data)}:previous(path,body);await battery();api=previous;})()`);
     const rendered=await evaluate(`({text:document.querySelector('#battery').textContent,level:document.querySelector('.battery').dataset.level,bars:document.querySelector('.batt').children.length})`);
     if(rendered.text!==text||rendered.level!==level||((level==='voltage'||level==='unavailable')&&rendered.bars!==0))throw Error(page+' battery '+label+' '+JSON.stringify(rendered));
     await capture(page+'-battery-'+label);
    }
    // A failed read must clear the previous value, including icon and tooltip.
    const failed=await evaluate(`(async()=>{const previous=api;showBattery({percent:68});api=async()=>{throw Error('test offline')};await battery();api=previous;return {text:document.querySelector('#battery').textContent,bars:document.querySelector('.batt').children.length,title:document.querySelector('.battery').title};})()`);
    if(failed.text!=='—'||failed.bars!==0||failed.title!=='Battery measurement unavailable')throw Error('Stale battery after error');
    if(page==='overview'){
     // Real timers: initially unavailable ADC becomes visible after the 1 s retry.
     const retry=await evaluate(`(async()=>{const previous=api;let reads=0;api=async(path,body)=>path==='battery'?(++reads===1?{voltage:null,percent:null}:{voltage:3.506,percent:null}):previous(path,body);await battery();await new Promise(r=>setTimeout(r,1200));api=previous;return {reads,text:document.querySelector('#battery').textContent};})()`);
     if(retry.reads!==2||retry.text!=='3.51 V')throw Error('Battery startup retry '+JSON.stringify(retry));
    }
   }
  }
  results.push({page,...check});
 }
 await writeFile(resolve(root,'captures',artifactDir,'report.json'),JSON.stringify({scope:liveOrigin?'Physical Axio HTTP dashboard':runtimeMode?'Real C++ HTTP/runtime; hardware simulated':'Browser fixture QA only',viewport:{width:430,height:860,dpr:2},matrix:matrixMode,dialogsDismissed:dialogs,results},null,2));
 await writeFile(resolve(root,'captures',artifactDir,'comparison.html'),'<html lang="en"><meta charset="utf-8"><title>Axio presentation comparison</title><style>body{font-family:Arial}section{display:flex;gap:12px;align-items:flex-start}img{width:430px}h2{margin-top:30px}</style><h1>'+ (liveOrigin?'Physical Axio dashboard':runtimeMode?'Host runtime with simulated hardware':'Presentation QA — fixture data') +'</h1><p>Reference left, implementation right. Visual acceptance remains pending.</p>'+['overview','motion','osc','network','diagnostics'].map(p=>`<h2>${p}</h2><section><img src="../../axio_codex_ui/screenshots/axio_${p}_mobile.png"><img src="${p}.png"></section>`).join(''));
 console.log('PASS: five screenshots and conditional controls; mode='+artifactDir+(liveOrigin?'. Physical HTTP.':'. Hardware NOT RUN.'));
 await send('Browser.close');
} finally {if(ws)ws.close();browser.kill();server.close();if(backend)backend.kill();}
