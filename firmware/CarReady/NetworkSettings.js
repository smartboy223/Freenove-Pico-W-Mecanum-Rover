let networkReportKey='',wifiNetworks=[],wifiScanBusy=false,wifiResultLock=false,pendingWifi=null,latestWifiUptime=0;
function setWifiOpen(value){$('wifiOpen').checked=value;$('wifiPassword').disabled=value;$('wifiPassword').required=!value}
function renderNetwork(s){
  latestWifiUptime=s.uptime_ms||0;
  let home=s.network_mode==='home',saved=s.home_settings_source==='saved',none=s.home_settings_source==='none';
  $('connection').textContent=s.network_mode==='hotspot'?'Connected to car hotspot':'Connected on your LAN';
  $('networkInfo').textContent=(home?'Home Wi-Fi connected':s.network_mode==='hotspot'?'Car hotspot connected':'Connecting…')+' · '+s.ip+' · Car Wi-Fi: '+(s.hotspot_ssid||'Freenove-Rover');
  $('savedNetwork').textContent=none?'No home network saved':(saved?'Saved home network: ':'Build default network: ')+(s.home_ssid||'Not set');
  $('currentNetwork').textContent='Connected to: '+(s.connected_ssid||'Connecting…')+(home&&s.wifi_signal_dbm!=null?' · '+s.wifi_signal_dbm+' dBm':'');
  let url=s.dashboard_url||'http://'+s.ip+'/';$('networkUrl').href=url;$('networkUrl').textContent=url;
  $('homeAddress').href=s.home_url||'http://freenove-car.local/';$('homeAddress').textContent=s.home_url||'http://freenove-car.local/';
  $('homeWifi').disabled=none;$('wifiForget').disabled=none||wifiSubmitting||wifiScanBusy;$('hotspot').disabled=s.network_mode==='hotspot';
  if(s.network_mode==='hotspot')$('networkSettings').open=true;
  let key=[s.network_mode,s.home_ssid,s.home_settings_source,s.wifi_last_result,s.wifi_join_state].join('|');
  if(pendingWifi&&!wifiSubmitting){
    if(s.wifi_join_state==='failed'){pendingWifi=null;wifiResultLock=false;networkReportKey='';}
    else if(home&&s.connected_ssid===pendingWifi.ssid&&(latestWifiUptime<pendingWifi.uptime||s.wifi_join_state==='joined')){pendingWifi=null;wifiResultLock=false;networkReportKey='';}
  }
  if(key!==networkReportKey&&!wifiSubmitting&&!wifiResultLock&&!pendingWifi){
    networkReportKey=key;
    if(s.wifi_join_state==='failed'||s.wifi_last_result==='failed')$('wifiResult').textContent='Last join failed. Previous settings were kept; check the Wi-Fi name/password and retry.';
    else if(none)$('wifiResult').textContent='Home network forgotten. This stays deleted after restarts and updates. Choose a network to save a new connection.';
    else if(home)$('wifiResult').textContent='Connected successfully to '+(s.connected_ssid||s.home_ssid||'home Wi-Fi')+'. Dashboard: '+url+(saved?' · Settings saved on the Pico.':'');
    else $('wifiResult').textContent='Using car Wi-Fi. '+(s.home_ssid?'Saved home network: '+s.home_ssid+'. Retry it or choose another.':'Choose your home network below.');
  }
}
async function wifiRequest(action){
  let r=await fetch('/api/wifi/'+action,{method:'POST',headers:{'X-Car-Token':token,'X-Car-Owner':owner},cache:'no-store',signal:AbortSignal.timeout(3000)});
  let data=await r.json();if(!r.ok)throw Error(data.error||'Wi-Fi request rejected');return data;
}
function openWifiSetup(){$('networkSettings').open=true;$('networkSettings').scrollIntoView({behavior:'smooth',block:'start'})}
$('connection').onclick=openWifiSetup;$('connection').onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();openWifiSetup()}};
$('wifiShow').onchange=()=>$('wifiPassword').type=$('wifiShow').checked?'text':'password';$('wifiOpen').onchange=()=>setWifiOpen($('wifiOpen').checked);
$('wifiScan').onclick=async()=>{
  if(wifiScanBusy||wifiSubmitting)return;wifiScanBusy=true;$('wifiScan').disabled=$('wifiJoin').disabled=true;$('wifiScanStatus').textContent='Scanning nearby 2.4 GHz networks · car stopped…';
  try{
    await wifiRequest('scan');let data;
    for(let i=0;i<22;i++){
      await new Promise(resolve=>setTimeout(resolve,600));let r=await fetch('/api/wifi/scan',{cache:'no-store',signal:AbortSignal.timeout(2500)});data=await r.json();if(!r.ok)throw Error(data.error||'Scan unavailable');if(data.state!=='scanning')break;
    }
    if(data.state!=='ready')throw Error('Scan did not finish. Try again.');
    wifiNetworks=data.networks||[];$('wifiNetworks').replaceChildren(new Option('Choose a nearby network…',''));
    wifiNetworks.forEach((n,i)=>$('wifiNetworks').add(new Option(n.ssid+' · '+n.rssi+' dBm · '+(n.open?'Open':'Password required'),String(i))));
    $('wifiNetworks').disabled=!wifiNetworks.length;$('wifiScanStatus').textContent=wifiNetworks.length+' visible networks found. Choose one, or type a hidden network name below.';
  }catch(e){$('wifiScanStatus').textContent=e.message||'Scan interrupted. Try again.'}finally{wifiScanBusy=false;$('wifiScan').disabled=$('wifiJoin').disabled=false;await update()}
};
$('wifiNetworks').onchange=()=>{let index=$('wifiNetworks').value;if(index==='')return;let n=wifiNetworks[Number(index)];$('wifiSsid').value=n.ssid;$('wifiPassword').value='';setWifiOpen(n.open);if(!n.open)$('wifiPassword').focus()};
$('wifiForm').onsubmit=async e=>{
  e.preventDefault();if(wifiSubmitting||wifiScanBusy)return;wifiSubmitting=true;wifiResultLock=false;$('wifiJoin').disabled=$('wifiScan').disabled=true;$('wifiResult').textContent='Stopping the car and sending Wi-Fi settings…';
  try{
    let payload=new URLSearchParams({ssid:$('wifiSsid').value,password:$('wifiOpen').checked?'':$('wifiPassword').value,open:$('wifiOpen').checked?'1':'0'});
    let r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Car-Token':token,'X-Car-Owner':owner},body:payload,cache:'no-store',signal:AbortSignal.timeout(3000)});let data=await r.json();if(!r.ok)throw Error(data.error||'Wi-Fi settings rejected');
    $('wifiPassword').value='';pendingWifi={ssid:$('wifiSsid').value,uptime:latestWifiUptime};networkReportKey='';$('wifiResult').textContent='Joining '+$('wifiSsid').value+'. Connect your phone to that Wi-Fi, then open freenove-car.local. The connection panel will show success and its LAN URL. If it fails, Freenove-Rover returns after about 30 seconds.';message('Car stopped · joining home Wi-Fi');
  }catch(err){pendingWifi=null;wifiResultLock=true;$('wifiResult').textContent=err.message||'Connection interrupted. Reconnect and check the connection panel.'}finally{wifiSubmitting=false;$('wifiJoin').disabled=$('wifiScan').disabled=false}
};
$('wifiForget').onclick=async()=>{
  if(wifiSubmitting||wifiScanBusy||!confirm('Forget the saved home network? The car will stop and switch to Freenove-Rover.'))return;
  wifiSubmitting=true;pendingWifi=null;wifiResultLock=false;$('wifiForget').disabled=true;
  try{await wifiRequest('forget');$('wifiPassword').value='';$('savedNetwork').textContent='No home network saved';$('wifiResult').textContent='Home network forgotten permanently. Join Freenove-Rover and open http://192.168.4.1/ to choose another.';message('Car stopped · home Wi-Fi forgotten')}catch(e){$('wifiResult').textContent=e.message||'Delete not confirmed. Reload to check saved settings.'}finally{wifiSubmitting=false}
};
$('hotspot').onclick=()=>command('network',{name:'hotspot'}).then(()=>message('Car stopped. Join Freenove-Rover and open http://192.168.4.1/.')).catch(()=>{});
$('homeWifi').onclick=()=>command('network',{name:'home'}).then(()=>message('Car stopped. Reconnect to home Wi-Fi, then open '+$('homeAddress').href)).catch(()=>{});
