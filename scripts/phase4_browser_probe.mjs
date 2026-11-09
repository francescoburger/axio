// Fixture presentation QA or --runtime: real C++ runtime with fake hardware.
import {spawn} from 'node:child_process';
import {mkdir,writeFile,readFile} from 'node:fs/promises';
import {createServer} from 'node:http';
import {resolve} from 'node:path';
const root=resolve(import.meta.dirname,'..');
const runtimeMode=process.argv.includes('--runtime');
const matrixMode=process.argv.includes('--matrix');
const apMode=process.argv.includes('--ap');
const widthIndex=process.argv.indexOf('--width');
const artifactTagIndex=process.argv.indexOf('--artifact-tag');
const artifactTag=artifactTagIndex<0?'':process.argv[artifactTagIndex+1];
if(artifactTagIndex>=0&&!/^[a-z0-9-]+$/.test(artifactTag))throw Error('Invalid artifact tag');
const viewportWidth=widthIndex<0?430:Number(process.argv[widthIndex+1]);
if(!Number.isInteger(viewportWidth)||viewportWidth<280||viewportWidth>3000)throw Error('Invalid viewport width');
const dialogs=[];
const originIndex=process.argv.indexOf('--origin');
const liveOrigin=originIndex<0?null:process.argv[originIndex+1];
const artifactDir=(liveOrigin?'phase4-device-ui':runtimeMode?'phase4-runtime-ui':'phase4-ui')+(apMode?'-ap':'')+(widthIndex<0?'':'-'+viewportWidth)+(artifactTag?'-'+artifactTag:'');
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
 backend=spawn(resolve(root,'.build/tests/dashboard_runtime'),apMode?['--ap']:[],{stdio:['ignore','pipe','pipe']});
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
 const capture=async name=>{const m=await cdp('Page.getLayoutMetrics');const r=await cdp('Page.captureScreenshot',{format:'png',captureBeyondViewport:true,clip:{x:0,y:0,width:viewportWidth,height:m.cssContentSize.height,scale:1}});await writeFile(resolve(root,'captures',artifactDir,name+'.png'),Buffer.from(r.data,'base64'));};
 await cdp('Page.enable');await cdp('Runtime.enable');await cdp('Emulation.setDeviceMetricsOverride',{width:viewportWidth,height:860,deviceScaleFactor:2,mobile:false});
 const fixture={device:{id:'axio-001',mac:'A4:C1:38:7F:2E:9B',ip:'192.168.1.42',hostname:'axio-a4c1387f2e9b.local',ap_ssid:'Axio-axio-001',osc_status:'Sending',version:'Phase 3'},status:{wifi_connected:true,imu:'Active'},peers:[{id:'axio-002',ip:'192.168.1.43'},{id:'axio-003',ip:'192.168.1.44'}],osc:{schema:2,id:'axio-001',destinations:[{ip:'192.168.1.50',port:9000},{ip:'192.168.1.51',port:9001}],listen_port:16889,calibration_address:'/calibration/{id}',ack:{enabled:false,address:'/calibrated/{id}',destination:0},editor:{main_address:'/motion/{id}',suffixes:['','real','world','gyro','quat'],selected:[7,7,7,7,15],interval_ms:20}},network:{connected:true,ssid:'Studio WiFi',ip:'192.168.1.42',gateway:'192.168.1.1',subnet:'255.255.255.0',rssi:-58,entry_point:'axio.local',ap_ip:'192.168.4.1',revision:1,networks:[{ssid:'Studio WiFi',original:0},{ssid:'Mobile Hotspot',original:1}]},diagnostics:{uptime:'02h 34m 18s',heap_free:'112.4 KB',heap_min:'32.5 KB',flash_used:'1.10 MB',imu_hz:99.95,max_interval_us:78000,osc_hz:100,destinations:1,udp_errors:0,wifi_mode:'STA',mdns:'Active',samples:1000,datagrams:1000,reconnects:0}};
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
  if(page==='motion'){
   await evaluate(`document.querySelector('#show-3d').click()`);
   let drawn=false;
   for(let i=0;i<50;++i){
    drawn=await evaluate(`(()=>{const c=document.querySelector('#orientation'),ctx=c?.getContext('2d');return !!ctx&&ctx.getImageData(0,0,c.width,c.height).data.some((n,i)=>i%4===3&&n>0);})()`);
    if(drawn)break;await new Promise(r=>setTimeout(r,100));
   }
   if(!drawn)throw Error('Orientation canvas remained empty');
  }
  await evaluate('new Promise(r=>setTimeout(r,200))');
  const metrics=await cdp('Page.getLayoutMetrics');
  const shot=await cdp('Page.captureScreenshot',{format:'png',captureBeyondViewport:true,clip:{x:0,y:0,width:viewportWidth,height:metrics.cssContentSize.height,scale:1}});
  await writeFile(resolve(root,`captures/${artifactDir}/${page}.png`),Buffer.from(shot.data,'base64'));
  const check=await evaluate(`(()=>{const mast=document.querySelector('.mast').getBoundingClientRect(),nav=document.querySelector('.nav').getBoundingClientRect(),title=document.querySelector('.mast h1').getBoundingClientRect(),battery=document.querySelector('.battery').getBoundingClientRect();return {cards:[...document.querySelectorAll('.card h2')].map(e=>e.textContent),battery:document.querySelector('#battery').textContent,overflow:document.documentElement.scrollWidth>${viewportWidth},headerFull:Math.abs(mast.left)<1&&Math.abs(mast.width-${viewportWidth})<1,navWidth:nav.width,headerOverlap:title.right>battery.left,badge:!!document.querySelector('.statusline'),topIsCard:document.querySelector('main').firstElementChild?.classList.contains('card')};})()`);
  if(check.overflow)throw Error(page+' horizontal overflow');
  if(!check.headerFull||check.headerOverlap||check.badge||!check.topIsCard)throw Error(page+' header/status layout '+JSON.stringify(check));
  if(check.navWidth!==viewportWidth)throw Error(page+' navigation is not full width');
  if(await evaluate(`document.querySelector('.mast-inner').getBoundingClientRect().width`)!==check.navWidth)throw Error(page+' header contents no longer align with navigation');
  const layout=await evaluate(`(()=>{const shell=document.querySelector('.shell').getBoundingClientRect(),inner=document.querySelector('.mast-inner'),style=getComputedStyle(inner),title=document.querySelector('.mast h1').getBoundingClientRect(),battery=document.querySelector('.battery').getBoundingClientRect();return {mainWidth:shell.width,titleLeft:title.left,paddingLeft:parseFloat(style.paddingLeft),batteryRight:battery.right,paddingRight:parseFloat(style.paddingRight)};})()`);
  if(layout.mainWidth!==Math.min(viewportWidth,viewportWidth>=800?780:460)||Math.abs(layout.titleLeft-layout.paddingLeft)>1||Math.abs(viewportWidth-layout.batteryRight-layout.paddingRight)>1)throw Error('Header/content layout '+JSON.stringify(layout));
  const brand=await evaluate(`({text:document.querySelector('.mast h1').textContent.trim(),logo:!!document.querySelector('.brand-mark path'),oldPageTitle:!!document.querySelector('#pagetitle'),navActive:document.querySelector('.nav a.active')?.dataset.page})`);
  if(brand.text!=='Axio'||!brand.logo||brand.oldPageTitle||brand.navActive!==page)throw Error('Axio brand/navigation '+JSON.stringify(brand));
  if(page==='overview'){
   const oscIcon=await evaluate(`(()=>{const icon=document.querySelector('.osc-icon');return {color:icon?getComputedStyle(icon).color:null,stroke:icon?.getAttribute('stroke'),label:icon?.closest('.stat').querySelector('strong').textContent};})()`);
   if(oscIcon.color!=='rgb(100, 32, 172)'||oscIcon.stroke!=='currentColor'||oscIcon.label!=='OSC')throw Error('OSC icon '+JSON.stringify(oscIcon));
  }
  if(page==='motion'&&!liveOrigin){
   await evaluate(`(async()=>{paused=true;clearTimeout(timer);for(let i=0;i<100&&inFlight;i++)await new Promise(r=>setTimeout(r,25));if(inFlight)throw Error('Telemetry still in flight');})()`);
   const geometry=await evaluate(`(()=>{const axes=[[1,0,0],[0,1,0],[0,0,1]].map(projectIsometric),lengths=axes.map(([x,y])=>Math.hypot(x,y)),cosines=axes.map((a,i)=>{const b=axes[(i+1)%3];return (a[0]*b[0]+a[1]*b[1])/(lengths[i]*lengths[(i+1)%3]);}),q={q0:1,q1:0,q2:0,q3:0},original=JSON.stringify(q),canvas=document.querySelector('#orientation'),ctx=canvas.getContext('2d'),fillText=ctx.fillText,labels=[];ctx.fillText=function(text,...args){labels.push(text);return fillText.call(this,text,...args);};try{render3d(q);}finally{ctx.fillText=fillText;}const base=orientationVertices(q)[6],v=Math.SQRT1_2,poses=[{q0:v,q1:v,q2:0,q3:0},{q0:v,q1:0,q2:0,q3:v},{q0:v,q1:0,q2:-v,q3:0}],turns=poses.map(orientationVertices),expected=[[base[0],-base[2],base[1]],[base[2],base[1],-base[0]],[-base[1],base[0],base[2]]],rotations=turns.every((vertices,i)=>vertices[6].every((n,j)=>Math.abs(n-expected[i][j])<1e-9));const bodyAxes=poses.map(p=>orientationGeometry(p).axes),expectedAxes=[[[1,0,0],[0,0,1],[0,-1,0]],[[0,0,-1],[0,1,0],[1,0,0]],[[0,1,0],[-1,0,0],[0,0,1]]],solidAxes=bodyAxes.every((axes,i)=>axes.every((axis,j)=>axis.every((n,k)=>Math.abs(n-expectedAxes[i][j][k])<1e-9))),coupled=poses.every(p=>{const g=orientationGeometry(p);return g.vertices[6].every((n,i)=>Math.abs(n-.72*g.axes.reduce((sum,axis)=>sum+axis[i],0))<1e-9);});return {lengths,cosines,labels,rotations,bodyAxes,solidAxes,coupled,unchanged:JSON.stringify(q)===original,empty:orientationVertices({q0:0,q1:0,q2:0,q3:0})===null,invalid:orientationVertices({q0:null,q1:0,q2:0,q3:0})===null};})()`);
   if(geometry.lengths.some(n=>Math.abs(n-Math.sqrt(2/3))>1e-9)||geometry.cosines.some(n=>Math.abs(n+.5)>1e-9)||geometry.labels.join('')!=='XYZ'||!geometry.rotations||!geometry.solidAxes||!geometry.coupled||!geometry.unchanged||!geometry.empty||!geometry.invalid)throw Error('Isometric geometry '+JSON.stringify(geometry));
   for(const [name,q] of [['neutral',{q0:1,q1:0,q2:0,q3:0}],['pitch-x90',{q0:Math.SQRT1_2,q1:Math.SQRT1_2,q2:0,q3:0}],['yaw-y90',{q0:Math.SQRT1_2,q1:0,q2:0,q3:Math.SQRT1_2}],['roll-z90',{q0:Math.SQRT1_2,q1:0,q2:-Math.SQRT1_2,q3:0}]]){
    await evaluate(`render3d(${JSON.stringify(q)})`);await capture('motion-'+name);
   }
   const recovery=await evaluate(`(()=>{const canvas=document.querySelector('#orientation'),ctx=canvas.getContext('2d');render3d({q0:0,q1:0,q2:0,q3:0});const invalid=!!document.querySelector('#render-status').textContent,cleared=ctx.getImageData(0,0,canvas.width,canvas.height).data.every((n,i)=>i%4!==3||n===0),getContext=canvas.getContext;canvas.getContext=()=>null;render3d({q0:1,q1:0,q2:0,q3:0});const missing=document.querySelector('#render-status').textContent.includes('3D rendering unavailable');canvas.getContext=getContext;render3d({q0:1,q1:0,q2:0,q3:0});return {invalid,cleared,missing,recovered:document.querySelector('#render-status').textContent===''};})()`);
   if(Object.values(recovery).some(v=>!v))throw Error('Canvas recovery '+JSON.stringify(recovery));
   const pausedToggle=await evaluate(`(()=>{const toggle=document.querySelector('#show-3d');toggle.click();render3d({q0:0,q1:0,q2:0,q3:0});toggle.click();const c=document.querySelector('#orientation');return paused&&!document.querySelector('#advanced').hidden&&c.getContext('2d').getImageData(0,0,c.width,c.height).data.some((n,i)=>i%4===3&&n>0)&&document.querySelector('#render-status').textContent==='';})()`);
   if(!pausedToggle)throw Error('Paused visualization did not draw the last valid sample');
   check.isometric={...geometry,...recovery,pausedToggle};await evaluate(`paused=false;pollSample()`);
  }
  if(page==='motion') {const live=await evaluate(`document.querySelector('#stream-status').textContent==='Live'&&document.querySelector('#stream-status').closest('.card')!==null`);if(!live)throw Error('Sensor stream state missing');const hidden=await evaluate(`document.querySelector('#show-3d').click();document.querySelector('#advanced').hidden`);if(!hidden)throw Error('Quaternion/3D visibility');}
  if(page==='osc'){
   const count=await evaluate(`document.querySelector('#ack-enabled').checked=true;document.querySelector('#ack-enabled').dispatchEvent(new Event('change'));document.querySelector('#ack-settings').hidden`);if(count)throw Error('Acknowledgement visibility');
   const editor=await evaluate(`(()=>{const initial=state.osc,field=document.querySelector('#calibration-address'),previewStyle=getComputedStyle(document.querySelector('.message-preview')),transfer=document.querySelector('.config-transfer'),ack=document.querySelector('#ack-settings');return {address:field.value,expected:initial.calibration_address,preview:document.querySelector('.preview-lines').textContent,expectedPreview:editorMessages(initial.editor,state.device.id).map(m=>m.address+'  '+m.fields.join(', ')).join('\\n'),left:previewStyle.borderLeftWidth,right:previewStyle.borderRightWidth,radius:previewStyle.borderRadius,numberedMessage:!!document.querySelector('.message-heading'),transferTitle:document.querySelector('#transfer-title')?.textContent,transferGap:transfer.getBoundingClientRect().top-ack.getBoundingClientRect().bottom,abbreviatedAcknowledgement:/\\bACK\\b/.test(document.querySelector('main').textContent)};})()`);
   if(editor.address!==editor.expected||editor.preview!==editor.expectedPreview||editor.left!==editor.right||editor.radius==='0px'||editor.numberedMessage||!editor.transferTitle||editor.transferGap<16||editor.abbreviatedAcknowledgement)throw Error('OSC editor presentation '+JSON.stringify(editor));
   const dynamic=await evaluate(`(()=>{const expected=structuredClone(state.osc.editor),chip=document.querySelector('[data-group="0"][data-bit="0"]');expected.selected[0]^=1;chip.click();const actual=document.querySelector('.preview-lines').textContent,wanted=editorMessages(expected,state.device.id).map(m=>m.address+'  '+m.fields.join(', ')).join('\\n');chip.click();return actual===wanted;})()`);
   if(!dynamic)throw Error('Dynamic OSC preview changed');
   if(runtimeMode){
    const saved=await evaluate(`(async()=>{document.querySelector('#calibration-address').value='/cal';document.querySelector('#ack-address').value='/calok';await document.querySelector('#apply').onclick();const active=(await api('snapshot')).osc;return {address:active.calibration_address,enabled:active.ack.enabled,acknowledgement:active.ack.address,notice:document.querySelector('#notice').textContent};})()`);
    if(saved.address!=='/cal'||!saved.enabled||saved.acknowledgement!=='/calok')throw Error('OSC configuration save '+JSON.stringify(saved));check.pureDataConfigurationSaved=true;
   }
  }
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
 await writeFile(resolve(root,'captures',artifactDir,'report.json'),JSON.stringify({scope:liveOrigin?'Physical Axio HTTP dashboard':runtimeMode?'Real C++ HTTP/runtime; hardware simulated':'Browser fixture QA only',viewport:{width:viewportWidth,height:860,dpr:2},apMode,matrix:matrixMode,dialogsDismissed:dialogs,results},null,2));
 await writeFile(resolve(root,'captures',artifactDir,'comparison.html'),'<html lang="en"><meta charset="utf-8"><title>Axio presentation comparison</title><style>body{font-family:Arial}section{display:flex;gap:12px;align-items:flex-start}img{width:430px}h2{margin-top:30px}</style><h1>'+ (liveOrigin?'Physical Axio dashboard':runtimeMode?'Host runtime with simulated hardware':'Presentation QA — fixture data') +'</h1><p>Reference left, implementation right. Visual acceptance remains pending.</p>'+['overview','motion','osc','network','diagnostics'].map(p=>`<h2>${p}</h2><section><img src="../../axio_codex_ui/screenshots/axio_${p}_mobile.png"><img src="${p}.png"></section>`).join(''));
 console.log('PASS: five screenshots and conditional controls; mode='+artifactDir+(liveOrigin?'. Physical HTTP.':'. Hardware NOT RUN.'));
 await send('Browser.close');
} finally {if(ws)ws.close();browser.kill();server.close();if(backend)backend.kill();}
