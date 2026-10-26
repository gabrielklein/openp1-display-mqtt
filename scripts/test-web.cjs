// Exercise the exact embedded scripts without a browser or npm dependencies.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('web/pages.hpp','utf8')+fs.readFileSync('web/settings.hpp','utf8')+fs.readFileSync('web/access.hpp','utf8')+fs.readFileSync('web/diagnostics.hpp','utf8');
const scripts = [...source.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m=>m[1]);
function element() {
  return {textContent:'',className:'',value:'',hidden:false,disabled:false,dataset:{},children:[],attributes:{},
    append(...items){this.children.push(...items)},replaceChildren(){this.children=[]},
    setAttribute(k,v){this.attributes[k]=v},removeAttribute(k){delete this.attributes[k]},focus(){this.focused=true}};
}
function context(fetch) {
  const elements=Object.fromEntries([...source.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],element()]));
  const sandbox={document:{getElementById:id=>elements[id],createElement:()=>element()},fetch,
    location:{replace(url){this.redirect=url}},AbortController,TextEncoder,setTimeout:(fn,ms)=>{if(ms===1000)queueMicrotask(fn);return 1},clearTimeout:()=>{}};
  vm.createContext(sandbox);return {elements,sandbox};
}
const settle=()=>new Promise(resolve=>setImmediate(resolve));
const ok=data=>({ok:true,status:200,json:async()=>data});
const submit={preventDefault(){}};
function wizard(state='success') {
  const calls=[];
  const ctx=context(async(url,options)=>{
    calls.push([url,options]);
    if(url==='/setup/token')return ok({token:'test-token'});
    if(url==='/setup/wifi')return ok({});
    if(url==='/setup/status')return ok({state,ip:'192.168.1.42'});
    return ok({state:'ready',networks:[]});
  });
  vm.runInContext(scripts[1],ctx.sandbox);return {...ctx,calls};
}
(async()=>{
  let online=true;
  const a=context(async()=>{if(!online)throw Error('offline');return ok({simulation:true,connected:true,errors:0,values:[{name:'<script>',obis:'1.0.1.7.0.255',value:950,unit:'W',age_ms:16000,stale:true}]})});
  vm.runInContext(scripts[0],a.sandbox);await settle();
  assert.match(a.elements.state.textContent,/Simulation/);
  assert.match(a.elements.accessState.textContent,/Press and release S2/);
  assert.equal(a.elements.settings.hidden,true);
  assert.equal(a.elements.values.children[0].className,'stale');
  assert.equal(a.elements.values.children[0].children[0].textContent,'<script>');
  assert.equal(a.elements.values.children[0].children[1].textContent,'1-0:1.7.0');
  assert.equal(vm.runInContext("formatObis('1.1.1.8.1.255')",a.sandbox),'1-1:1.8.1');
  assert.equal(vm.runInContext("formatObis('1.1.1.8.1.0')",a.sandbox),'1-1:1.8.1*0');
  a.sandbox.fetch=async()=>ok({connected:true,errors:0,values:[{name:'Serial number',obis:'0.0.96.1.0.255',obis_display:'0-0:96.1.0',value:'00001234',unit:'',age_ms:100,stale:false},{name:'GR number',obis:'0.0.96.1.1.255',value:'<script>',unit:'',age_ms:100,stale:false}]});
  await vm.runInContext('refresh()',a.sandbox);
  assert.equal(a.elements.values.children[0].children[2].textContent,'00001234');
  assert.equal(a.elements.values.children[1].children[2].textContent,'<script>');
  a.sandbox.fetch=async()=>{if(!online)throw Error('offline');return ok({connected:true,errors:0,values:[]})};
  online=false;await vm.runInContext('refresh()',a.sandbox);assert.equal(a.elements.values.className,'stale');
  online=true;await vm.runInContext('refresh()',a.sandbox);assert.equal(a.elements.values.className,'');

  a.sandbox.fetch=async()=>ok({access:'Full',access_remaining_seconds:299,connected:true,errors:0,values:[]});
  await vm.runInContext('refresh()',a.sandbox);assert.match(a.elements.accessState.textContent,/299 s remaining/);assert.equal(a.elements.settings.hidden,false);
  const b=wizard();let scans=0;
  b.sandbox.fetch=async()=>++scans===1?{...ok({state:'scanning'}),status:202}:ok({state:'ready',networks:[
    {ssid:'<img src=x onerror=bad()>',rssi:-50,secure:true},
    {ssid:'home',rssi:-80,secure:false},{ssid:'home',rssi:-70,secure:false},{ssid:''}]});
  const scanning=b.elements.scan.onclick();assert.equal(b.elements.scan.disabled,true);assert.match(b.elements.result.textContent,/Scanning/);
  await scanning;assert.equal(scans,2);assert.equal(b.elements.scan.disabled,false);
  assert.equal(b.elements.networks.children.length,2);
  const first=b.elements.networks.children[0];
  assert.equal(first.children[0].textContent,'<img src=x onerror=bad()>');assert.match(first.children[1].textContent,/Secured/);
  b.elements.networks.children[1].onclick();assert.equal(b.elements.ssid.value,'home');assert.equal(b.elements.credentials.hidden,false);
  assert.equal(b.elements.step2.attributes['aria-current'],'step');
  b.elements.back.onclick();assert.equal(b.elements.choose.hidden,false);
  b.elements.manual.onclick();assert.equal(b.elements.ssid.value,'');assert.equal(b.elements.ssid.focused,true);
  b.sandbox.fetch=async()=>ok({state:'ready',networks:[]});await b.elements.scan.onclick();assert.match(b.elements.result.textContent,/No networks/);
  for(const status of [409,431,503,403]) {
    b.sandbox.fetch=async()=>({ok:false,status});await b.elements.scan.onclick();
    assert.equal(b.elements.result.dataset.error,'true');assert.equal(b.elements.scan.disabled,false);
    if(status===431)assert.match(b.elements.result.textContent,/Headers/);
  }
  b.sandbox.fetch=async()=>ok({state:'scanning'});await b.elements.scan.onclick();assert.match(b.elements.result.textContent,/did not complete/);

  const c=wizard();c.elements.ssid.value='home';c.elements.password.value='fake-password';
  await c.elements.form.onsubmit(submit);
  const posted=c.calls.find(([url])=>url==='/setup/wifi')[1];
  assert.equal(posted.headers['X-Setup-Token'],'test-token');
  assert.deepEqual(JSON.parse(posted.body),{ssid:'home',password:'fake-password'});
  assert.equal(c.elements.password.value,'');assert.equal(c.elements.successMark.hidden,false);
  assert.equal(c.elements.address.href,'http://192.168.1.42/');
  assert.equal(c.elements.step3.attributes['aria-current'],'step');
  assert(!c.elements.connectionText.textContent.includes('fake-password'));
  const before=c.calls.length;await c.elements.form.onsubmit(submit);assert.equal(c.calls.length,before);

  for(const state of ['timeout','failed','storage_error','idle','connecting']) {
    const d=wizard(state);d.elements.ssid.value='home';d.elements.password.value='fake-password';
    await d.elements.form.onsubmit(submit);assert.equal(d.elements.retry.hidden,false);assert.equal(d.elements.spinner.hidden,true);
    assert.equal(d.elements.password.value,'');d.elements.retry.onclick();assert.equal(d.elements.credentials.hidden,false);
    assert.equal(d.elements.connect.disabled,false);
  }
  const e=wizard();let polls=0;
  e.sandbox.fetch=async url=>url==='/setup/token'?ok({token:'t'}):url==='/setup/wifi'?ok({}):ok(++polls===1?{state:'saving'}:{state:'success',ip:'javascript:bad()'});
  e.elements.ssid.value='open';await e.elements.form.onsubmit(submit);
  assert.equal(polls,2);assert.equal(e.elements.address.hidden,true);
  const f=wizard();f.sandbox.fetch=async()=>{throw Error('offline')};f.elements.ssid.value='home';f.elements.password.value='fake-password';
  await f.elements.form.onsubmit(submit);assert.equal(f.elements.connect.disabled,false);assert.equal(f.elements.password.value,'');
  const g=wizard();g.elements.ssid.value='\u00e9'.repeat(17);await g.elements.form.onsubmit(submit);assert.equal(g.calls.length,0);
  g.elements.ssid.value='home';g.elements.password.value='short';await g.elements.form.onsubmit(submit);assert.equal(g.calls.length,0);
  let sent;
  const h=context(async(url,options)=>{
    if(options&&options.method==='PUT'){sent=JSON.parse(options.body);return ok({saved:true})}
    if(url==='/api/v1/mqtt/status')return ok({connected:true,messages_sent:42,errors:0});
    if(url==='/api/v1/firmware')return ok({version:'2026.10.26',supported:true,pending_verify:false,max_bytes:3145728});
    if(url==='/api/v1/serial')return ok({baud:115200,parity:'None',stop_bits:1,inverted:true});
    if(url==='/api/v1/simulation')return ok({enabled:false,layer:'llc_apdu',text:'e6e700',default_text:'AABB'});
    if(url==='/api/v1/display')return ok({rotate_180:true,gauge_limit_w:10000});
    if(url==='/api/v1/session')return ok({token:'csrf'});
    if(url==='/api/v1/access')return ok({persisted:'View',effective:'Full',remaining_seconds:120});
    return ok({host:'broker.example',port:8883,root:'',ntp:'pool.ntp.org',tls:true,enabled:false,discovery:true,password_configured:true,ca_configured:true,state_enabled:true,obis_enabled:false,text_raw_enabled:true,text_enabled:true,text_id:'ISK',text_dropped:2});
  });
  vm.runInContext(scripts[2],h.sandbox);await settle();
  assert.equal(h.elements.mqttSent.textContent,'42');assert.match(h.elements.mqttConnection.textContent,/Connected/);assert.match(h.elements.mqttErrors.textContent,/No MQTT errors/);
  assert.equal(h.elements.simulationEnabled.checked,false);assert.equal(h.elements.simulationFields.hidden,true);assert.equal(h.elements.simulationFields.disabled,true);
  assert.equal(h.elements.simulationText.value,'e6e700');
  h.elements.simulationEnabled.checked=true;h.elements.simulationEnabled.onchange();assert.equal(h.elements.simulationFields.hidden,false);
  h.elements.simulationDefault.onclick();assert.equal(h.elements.simulationText.value,'AABB');assert.equal(h.elements.simulationLayer.value,'llc_apdu');
  await h.elements.simulationForm.onsubmit(submit);assert.deepEqual(sent,{enabled:true,layer:'llc_apdu',text:'AABB'});assert.match(h.elements.message.textContent,/restarting/);
  h.elements.simulationEnabled.checked=false;h.elements.simulationEnabled.onchange();h.elements.simulationText.value='invalid';
  await h.elements.simulationForm.onsubmit(submit);assert.deepEqual(sent,{enabled:false});
  assert.equal(h.elements.rotate180.checked,true);
  h.elements.rotate180.checked=false;await h.elements.displayForm.onsubmit(submit);assert.deepEqual(sent,{rotate_180:false,gauge_limit_w:10000});
  h.elements.gaugeLimit.value='20';await h.elements.displayForm.onsubmit(submit);assert.equal(sent.gauge_limit_w,20000);h.elements.gaugeLimit.value='10';
  assert.equal(h.elements.text_enabled.checked,true);assert.equal(h.elements.text_id.value,'ISK');
  assert.equal(h.elements.mqttFields.hidden,true);assert.equal(h.elements.mqttFields.disabled,true);
  assert.equal(h.elements.tlsFields.hidden,true);
  h.elements.port.value='invalid';
  await h.elements.mqttForm.onsubmit(submit);assert.deepEqual(sent,{enabled:false});
  h.elements.port.value=8883;h.elements.enabled.checked=true;h.elements.enabled.onchange();
  assert.equal(h.elements.mqttFields.hidden,false);assert.equal(h.elements.mqttFields.disabled,false);
  assert.equal(h.elements.tlsFields.hidden,false);
  await h.elements.mqttForm.onsubmit(submit);assert.equal(sent.tls,true);assert(!('password' in sent));assert(!('ca' in sent));
  h.elements.text_enabled.checked=false;h.elements.text_id.value='Custom meter';await h.elements.mqttForm.onsubmit(submit);assert.equal(sent.text_enabled,false);assert.equal(sent.text_raw_enabled,true);assert.equal(sent.obis_enabled,false);assert.equal(sent.text_id,'Custom meter');
  h.elements.text_enabled.checked=true;h.elements.text_raw_enabled.checked=false;await h.elements.mqttForm.onsubmit(submit);assert.equal(sent.text_enabled,true);assert.equal(sent.text_raw_enabled,false);
  assert.equal(h.elements.state_enabled.checked,true);h.elements.state_enabled.checked=false;await h.elements.mqttForm.onsubmit(submit);assert.equal(sent.state_enabled,false);
  h.elements.obis_enabled.checked=true;await h.elements.mqttForm.onsubmit(submit);assert.equal(sent.obis_enabled,true);
  h.elements.password.value='fake-password';await h.elements.mqttForm.onsubmit(submit);assert.equal(sent.password,'fake-password');assert.equal(h.elements.password.value,'');
  h.elements.ca.value='pending CA';h.elements.tls.checked=false;h.elements.tls.onchange();
  assert.equal(h.elements.tlsFields.hidden,true);assert.equal(h.elements.tlsFields.disabled,true);
  await h.elements.mqttForm.onsubmit(submit);assert.equal(sent.tls,false);assert(!('ca' in sent));assert(!('ntp' in sent));assert.equal(h.elements.ca.value,'pending CA');
  h.elements.enabled.checked=false;h.elements.enabled.onchange();assert.equal(h.elements.mqttFields.hidden,true);
  h.elements.enabled.checked=true;h.elements.enabled.onchange();assert.equal(h.elements.tlsFields.hidden,true);
  h.elements.tls.checked=true;h.elements.tls.onchange();assert.equal(h.elements.tlsFields.hidden,false);assert.equal(h.elements.ca.value,'pending CA');
  h.elements.clearCredentials.checked=true;h.elements.clearCa.checked=true;await h.elements.mqttForm.onsubmit(submit);
  assert.equal(sent.password,'');assert.equal(sent.username,'');assert.equal(sent.ca,'');
  const fresh=context(async url=>url==='/api/v1/access'?ok({persisted:'Full',effective:'Full',remaining_seconds:0}):ok({host:'',port:1883,root:'',ntp:'pool.ntp.org',tls:false,enabled:false,discovery:false}));
  vm.runInContext(scripts[2],fresh.sandbox);await settle();
  assert.equal(fresh.elements.tls.checked,false);assert.equal(fresh.elements.port.value,1883);
  fresh.elements.enabled.checked=true;fresh.elements.enabled.onchange();assert.equal(fresh.elements.tlsFields.hidden,true);
  // Slow saves provide immediate local feedback and reject a second submission.
  let finishSave,puts=0;
  h.sandbox.fetch=async(url,options)=>{
    if(options?.method==='PUT'){puts++;return new Promise(resolve=>{finishSave=resolve})}
    return ok({token:'csrf'});
  };
  h.elements.displaySave.textContent='Save display';
  const pending=h.elements.displayForm.onsubmit(submit);await settle();
  assert.equal(h.elements.displaySave.disabled,true);
  assert.equal(h.elements.mqttSave.disabled,true);
  assert.equal(h.elements.displaySave.textContent,'Saving…');
  assert.equal(h.elements.displayFeedback.dataset.state,'pending');
  await h.elements.displayForm.onsubmit(submit);assert.equal(puts,1);
  finishSave(ok({saved:true}));await pending;
  assert.equal(h.elements.displaySave.disabled,false);
  assert.equal(h.elements.displaySave.textContent,'Save display');
  assert.equal(h.elements.displayFeedback.dataset.state,'success');
  assert.match(h.elements.displayFeedback.textContent,/saved/);
  h.sandbox.fetch=async()=>({ok:false,status:503,json:async()=>({})});
  await h.elements.displayForm.onsubmit(submit);
  assert.equal(h.elements.displayFeedback.dataset.state,'error');
  assert.equal(h.elements.displaySave.disabled,false);
  assert.match(h.elements.displayFeedback.textContent,/503/);
  assert.equal(vm.runInContext('formatUptime(90061000)',h.sandbox),'1 d 01:01:01');
  assert.equal(vm.runInContext('formatUptime(0)',h.sandbox),'0 d 00:00:00');
  h.sandbox.fetch=async()=>ok({uptime_ms:90061000,free_heap_bytes:32768});
  await vm.runInContext('refreshDevice()',h.sandbox);
  assert.equal(h.elements.uptime.textContent,'1 d 01:01:01');
  assert.match(h.elements.deviceStatus.textContent,/32 KiB/);
  // Concurrent callers share one connection, including initial settings load.
  let inFlight=0,peak=0;
  h.sandbox.fetch=async()=>{peak=Math.max(peak,++inFlight);await settle();inFlight--;return ok({})};
  await vm.runInContext("Promise.all([api('/one'),api('/two'),api('/three'),api('/four')])",h.sandbox);
  assert.equal(peak,1);
  let expire;
  h.sandbox.setTimeout=(fn,ms)=>{if(ms===8000)expire=fn;return 1};
  h.sandbox.fetch=async(url,options)=>new Promise((resolve,reject)=>options.signal.addEventListener('abort',()=>reject(Error('aborted'))));
  const timeout=vm.runInContext("api('/slow')",h.sandbox);await settle();expire();
  await assert.rejects(timeout,/8 seconds/);
  h.sandbox.fetch=async()=>ok({uptime_ms:1000});
  await vm.runInContext('refreshDevice()',h.sandbox);
  assert.equal(h.elements.uptime.textContent,'0 d 00:00:01');
  h.sandbox.fetch=async()=>{throw Error('offline')};
  await vm.runInContext('refreshDevice()',h.sandbox);
  assert.equal(h.elements.uptime.textContent,'Unavailable');
  h.sandbox.fetch=async()=>ok({connected:false,status:'error',messages_sent:45,errors:2,publish_failures:1,last_error:'Connection error',error_type:1,broker_code:5,tls_code:0,tls_stack_code:0,socket_code:111});
  await vm.runInContext('refreshMqtt()',h.sandbox);
  assert.equal(h.elements.mqttSent.textContent,'45');assert.match(h.elements.mqttErrors.textContent,/Errors: 2/);assert.match(h.elements.mqttErrors.textContent,/socket 111/);
  h.sandbox.fetch=async()=>({ok:false,status:403,json:async()=>({})});
  await vm.runInContext('refreshMqtt()',h.sandbox);assert.equal(h.elements.mqttSent.textContent,'—');
  a.sandbox.fetch=async()=>ok({connected:true,notifications:123,values:[]});
  await vm.runInContext('refresh()',a.sandbox);assert.equal(a.elements.received.textContent,'123');
  const grid=(imp,exp,extra={})=>({connected:true,wifi_rssi_dbm:-62,gauge_limit_w:10000,values:[
    {obis:'1.0.1.7.0.255',value:imp,unit:'W',unit_code:27,has_scaler_unit:true,age_ms:100,stale:false,timestamp_ms:1000},
    {obis:'1.0.2.7.0.255',value:exp,unit:'W',unit_code:27,has_scaler_unit:true,age_ms:100,stale:false,timestamp_ms:1000}],...extra});
  a.sandbox.grid=grid(1500,200);vm.runInContext('updateOverview(grid)',a.sandbox);
  assert.match(a.elements.wifiQuality.textContent,/Good.*-62 dBm.*76%/);assert.equal(a.elements.wifiMeter.hidden,false);
  assert.equal(a.elements.netPower.textContent,'+1300 W');assert.equal(a.elements.exportPower.textContent,'200 W');assert.equal(a.elements.importPower.textContent,'1500 W');
  assert.equal(a.elements.powerGauge.attributes['aria-valuenow'],'1300');
  a.sandbox.grid=grid(0,20000,{gauge_limit_w:15000});vm.runInContext('updateOverview(grid)',a.sandbox);
  assert.equal(a.elements.netPower.textContent,'-20000 W');assert.equal(a.elements.powerGauge.attributes['aria-valuenow'],'-15000');assert.match(a.elements.powerStatus.textContent,/Outside/);assert.equal(a.elements.gaugeMin.textContent,'−15 kW');
  a.sandbox.grid=grid(0,0);vm.runInContext('updateOverview(grid)',a.sandbox);assert.equal(a.elements.netPower.textContent,'0 W');
  for(const invalid of ['missing','stale','unit','timestamp','nan']){
    const data=grid(1,2);if(invalid==='missing')data.values.shift();if(invalid==='stale')data.values[0].stale=true;if(invalid==='unit')data.values[0].unit_code=30;if(invalid==='timestamp')data.values[0].timestamp_ms=999;if(invalid==='nan')data.values[0].value=NaN;
    a.sandbox.grid=data;vm.runInContext('updateOverview(grid)',a.sandbox);assert.equal(a.elements.powerGauge.hidden,true);assert.equal(a.elements.netPower.textContent,'Unavailable');
  }
  const noExport=grid(1250,0);noExport.values.pop();a.sandbox.grid=noExport;vm.runInContext('updateOverview(grid)',a.sandbox);assert.equal(a.elements.netPower.textContent,'+1250 W');assert.equal(a.elements.exportPower.textContent,'0 W');assert.equal(a.elements.powerGauge.hidden,false);assert.match(a.elements.powerStatus.textContent,/using 0 W/);
  const staleExport=grid(1,2);staleExport.values[1].stale=true;a.sandbox.grid=staleExport;vm.runInContext('updateOverview(grid)',a.sandbox);assert.equal(a.elements.powerGauge.hidden,true);
  a.sandbox.grid=grid(0,0,{connected:false,wifi_rssi_dbm:null});vm.runInContext('updateOverview(grid)',a.sandbox);assert.equal(a.elements.wifiMeter.hidden,true);assert.match(a.elements.wifiQuality.textContent,/disconnected/);
  // Access save failures stay on Settings. Success displays a restart page and stops polls.
  h.sandbox.fetch=async()=>({ok:false,status:503,json:async()=>({})});
  await h.elements.accessForm.onsubmit(submit);assert.equal(vm.runInContext('restarting',h.sandbox),false);
  assert.equal(h.elements.accessFeedback.dataset.state,'error');
  h.sandbox.fetch=async()=>ok({token:'csrf',saved:true});h.elements.policy.value='None';
  await h.elements.accessForm.onsubmit(submit);
  assert.equal(h.elements.settingsContent.hidden,true);assert.equal(h.elements.restartPage.hidden,false);
  assert.equal(h.elements.restartTitle.focused,true);assert.equal(h.elements.restartReadings.hidden,true);
  assert.match(h.elements.restartDetails.textContent,/disabled after restart/);
  let restartRequests=0;h.sandbox.fetch=async()=>{restartRequests++;return ok({})};
  await vm.runInContext('refreshDevice()',h.sandbox);await vm.runInContext('refreshMqtt()',h.sandbox);assert.equal(restartRequests,0);
  await h.elements.accessForm.onsubmit(submit);assert.equal(restartRequests,0);
  const locked=context(async()=>({ok:false,status:403}));
  vm.runInContext(scripts[3],locked.sandbox);await settle();
  assert.match(locked.elements.unlockStatus.textContent,/Waiting for the button/);assert.equal(locked.sandbox.location.redirect,undefined);
  locked.sandbox.fetch=async()=>({ok:true,status:204});await vm.runInContext('waitForFull()',locked.sandbox);assert.equal(locked.sandbox.location.redirect,'/');
  locked.sandbox.fetch=async()=>{throw Error('offline')};await vm.runInContext('waitForFull()',locked.sandbox);assert.match(locked.elements.unlockStatus.textContent,/Reconnecting/);
  assert.equal(h.elements.serialFields.disabled,false);assert.equal(h.elements.serialBaud.value,115200);
  let serialBody;
  fresh.sandbox.fetch=async(url,options)=>{if(options?.method==='PUT')serialBody=JSON.parse(options.body);return ok({token:'csrf',saved:true})};
  fresh.elements.serialBaud.value='19200';fresh.elements.serialParity.value='Even';fresh.elements.serialStop.value='2';fresh.elements.serialInverted.checked=false;
  await fresh.elements.serialForm.onsubmit(submit);assert.deepEqual(serialBody,{baud:19200,parity:'Even',stop_bits:2,inverted:false});
  assert.equal(fresh.elements.restartPage.hidden,false);assert.match(fresh.elements.restartMessage.textContent,/UART settings/);
  let diagnosticsCalls=[];
  const diagnostics=context(async url=>{diagnosticsCalls.push(url);return url==='/api/v1/values'?ok({access:'View',simulation:true,uptime_ms:123,errors:2,last_error:'bad fcs',last_error_ms:100,notifications:4,validated_hdlc:5,values:[{obis:'1.0.250.7.0.255',name:'<script>unknown</script>',value:'<img>',age_ms:20,timestamp_ms:100,type:9,stale:true}]}):ok({limit:4,history_overwritten:3,records:[{sequence:9,timestamp_ms:100,source:'replay',layer:'wire_hdlc',hex:'7E00FF'}]})});
  vm.runInContext(scripts[4],diagnostics.sandbox);await settle();
  assert.deepEqual(diagnosticsCalls,['/api/v1/values','/api/v1/raw']);assert.match(diagnostics.elements.diagStatus.textContent,/Simulation/);
  assert.equal(diagnostics.elements.diagValues.children[0].className,'stale');assert.equal(diagnostics.elements.diagValues.children[0].children[0].children[0].textContent,'<script>unknown</script>');
  assert.equal(diagnostics.elements.diagValues.children[0].children[1].textContent,'<img>');
  assert.equal(diagnostics.elements.diagRaw.children[0].children[1].textContent,'7E00FF');assert.match(diagnostics.elements.diagErrors.children[1].textContent,/bad fcs/);
  diagnostics.sandbox.fetch=async()=>{throw Error('offline')};await vm.runInContext('refreshDiagnostics()',diagnostics.sandbox);assert.equal(diagnostics.elements.diagStatus.dataset.error,'true');assert.equal(diagnostics.elements.diagValues.children.length,0);
  diagnostics.sandbox.fetch=async url=>url==='/api/v1/values'?ok({values:[]}):{ok:false,status:403};await vm.runInContext('refreshDiagnostics()',diagnostics.sandbox);assert.equal(diagnostics.sandbox.location.redirect,'/unlock');assert.equal(diagnostics.elements.diagRaw.children.length,0);
  const updater=context(async(url,options)=>{
    if(url==='/api/v1/access')return ok({persisted:'Full',effective:'Full'});
    if(url==='/api/v1/session')return ok({token:'csrf'});
    if(url==='/api/v1/firmware')return ok({version:'2026.10.26',supported:true,pending_verify:false,max_bytes:3145728});
    return ok({entries:[],enabled:false,port:1883,layer:'llc_apdu',text:'',default_text:'',baud:115200,parity:'None',stop_bits:1,inverted:true});
  });
  vm.runInContext(scripts[2],updater.sandbox);await settle();
  assert.equal(updater.elements.firmwareFields.disabled,false);assert.equal(updater.elements.firmwareVersion.textContent,'2026.10.26');
  updater.elements.firmwareFile.files=[{name:'boot.txt',size:4096}];await updater.elements.firmwareForm.onsubmit(submit);assert.equal(updater.elements.firmwareFeedback.dataset.state,'error');
  updater.elements.restartPage.hidden=true;updater.elements.firmwareFile.files=[{name:'app.bin',size:4096}];
  updater.sandbox.fetch=async(url,options)=>options?.method==='PUT'?{ok:false,status:400,json:async()=>({error:{message:'Invalid firmware'}})}:ok({token:'csrf'});
  await updater.elements.firmwareForm.onsubmit(submit);assert.equal(updater.elements.firmwareFeedback.dataset.state,'error');assert.equal(updater.elements.restartPage.hidden,true);assert.equal(updater.elements.firmwareSave.disabled,false);
  let binaryBody,binaryHeaders;
  const binaryFile={name:'openp1-display-2026.10.26.bin',size:4096};updater.elements.firmwareFile.files=[binaryFile];
  updater.sandbox.fetch=async(url,options)=>{if(options?.method==='PUT'){binaryBody=options.body;binaryHeaders=options.headers}return ok({token:'csrf',updated:true})};
  await updater.elements.firmwareForm.onsubmit(submit);assert.equal(binaryBody,binaryFile);assert.equal(binaryHeaders['Content-Type'],'application/octet-stream');assert.equal(binaryHeaders['X-CSRF-Token'],'csrf');assert.equal(updater.elements.restartPage.hidden,false);assert.match(updater.elements.restartMessage.textContent,/Firmware validated/);
  console.log('Embedded dashboard, Wi-Fi wizard and settings tests passed');
})().catch(e=>{console.error(e);process.exitCode=1});
