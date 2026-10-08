"""Windows live open-hotspot, captive-page and home pairing checks; motors stay stopped."""
import argparse
import asyncio
import json
import re
import socket
import statistics
import struct
import subprocess
import sys
import time
import uuid
import urllib.error
import urllib.parse
import urllib.request
import serial
from pathlib import Path
from xml.sax.saxutils import escape

root=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--home-host',default='192.168.0.202')
parser.add_argument('--interface',default='Wi-Fi')
parser.add_argument('--port',default='COM3')
parser.add_argument('--leave-hotspot',action='store_true')
args=parser.parse_args()
profile='Freenove-Rover-Check-'+uuid.uuid4().hex[:8]
profile_file=root/'build/wifi-open-profile.xml'
results=[];token='';created=False;success=False
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs):return None
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),NoRedirect())

def raw(host,path,post=False,body=None,auth=True):
    headers={'X-Car-Token':token,'X-Car-Owner':'wifi-setup-check'} if auth else {}
    if body is not None:headers['Content-Type']='application/x-www-form-urlencoded'
    req=urllib.request.Request('http://'+host+path,data=body if body is not None else b'' if post else None,headers=headers)
    try:response=opener.open(req,timeout=4)
    except urllib.error.HTTPError as error:response=error
    with response:return response.status,response.headers,response.read()
def status(host):
    code,_,data=raw(host,'/api/status');assert code==200
    s=json.loads(data);assert s['firmware']=='CarReady-2.11'
    assert not s['armed'] and not s['moving'] and s['wheels']==[0]*4
    return s
def page(host):
    global token
    code,_,data=raw(host,'/');assert code==200
    html=data.decode();token=re.search(r"const token='([a-f0-9]+)'",html)[1]
    return html
def command(host,op,**values):
    code,_,body=raw(host,'/api/control?'+urllib.parse.urlencode(dict(op=op,**values)),True)
    assert code==200,(code,body.decode())
def form(host,**values):return raw(host,'/api/wifi',body=urllib.parse.urlencode(values).encode())
def netsh(*values,required=True):
    result=subprocess.run(['netsh','wlan',*values],capture_output=True,text=True)
    if required and result.returncode:raise RuntimeError('Windows Wi-Fi operation failed')
    return result.returncode==0
def join_pc():netsh('connect','name='+profile,'ssid='+hotspot['ssid'],'interface='+args.interface,required=False)
def usb_status():
    with serial.Serial(args.port,115200,timeout=2,write_timeout=2) as device:
        device.write(b'\n');time.sleep(.05);device.reset_input_buffer();device.write(b'STATUS\n')
        s=json.loads(device.readline())
        assert not s['armed'] and not s['moving'] and s['wheels']==[0]*4
        return s
def wait_host(host,mode,seconds=35,rejoin=False):
    deadline=time.monotonic()+seconds;last_join=0
    if rejoin:
        # Wait for the AP to exist before requesting association. Windows can
        # back off a profile when asked to connect while the radio is switching.
        while time.monotonic()<deadline:
            try:
                if usb_status()['network_mode']=='hotspot':break
            except (serial.SerialException,OSError,ValueError):pass
            time.sleep(.4)
        netsh('disconnect','interface='+args.interface,required=False)
        netsh('show','networks','mode=bssid',required=False)
        time.sleep(2);join_pc();last_join=time.monotonic()
    while time.monotonic()<deadline:
        if rejoin and time.monotonic()-last_join>15:
            netsh('disconnect','interface='+args.interface,required=False)
            netsh('show','networks','mode=bssid',required=False)
            time.sleep(2);join_pc();last_join=time.monotonic()
        try:
            s=status(host)
            if s['network_mode']==mode:return s
        except (OSError,ValueError):pass
        time.sleep(.35)
    raise RuntimeError('Car did not become reachable in '+mode+' mode')
def record(name,**details):
    results.append(dict(check=name,passed=True,**details));print('PASS:',name,flush=True)
def portal_dns(name):
    packet=struct.pack('!6H',0x524f,0x100,1,0,0,0)+b''.join(bytes([len(x)])+x.encode() for x in name.split('.'))+b'\0'+struct.pack('!2H',1,1)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        sock.settimeout(2);sock.sendto(packet,('192.168.4.1',53));reply,_=sock.recvfrom(512)
    assert reply[:2]==b'RO' and socket.inet_aton('192.168.4.1') in reply

async def browser_pair(settings):
    from playwright.async_api import async_playwright
    async with async_playwright() as p:
        browser=await p.chromium.launch()
        tab=await browser.new_page(viewport={'width':390,'height':844},is_mobile=True,has_touch=True)
        try:
            await tab.goto('http://192.168.4.1/',wait_until='domcontentloaded')
            await tab.wait_for_function("document.getElementById('connection').textContent==='Connected to car hotspot'")
            assert await tab.locator('#networkSettings').evaluate('e=>e.open')
            assert await tab.evaluate('document.documentElement.scrollWidth<=innerWidth')
            await tab.locator('#wifiSsid').fill(settings['ssid'])
            if settings['password']:await tab.locator('#wifiPassword').fill(settings['password'])
            else:await tab.locator('#wifiOpen').check()
            await tab.locator('#wifiJoin').click()
            await tab.wait_for_function("document.getElementById('wifiResult').textContent.startsWith('Joining now.')")
            assert await tab.locator('#wifiPassword').input_value()==''
        finally:await browser.close()

async def capture_setup():
    from playwright.async_api import async_playwright
    async with async_playwright() as p:
        browser=await p.chromium.launch()
        tab=await browser.new_page(viewport={'width':700,'height':1000})
        try:
            await tab.goto('http://'+args.home_host+'/',wait_until='domcontentloaded')
            await tab.wait_for_function("document.getElementById('connection').textContent==='Connected on your LAN'")
            await tab.locator('#connection').click()
            await tab.locator('#networkSettings').screenshot(path=str(root/'docs/images/dashboard-wifi-2.11.png'),style='.dock{visibility:hidden}')
        finally:await browser.close()

try:
    literal=args.interface.replace("'","''")
    adapter=subprocess.run(['powershell','-NoProfile','-Command',"(Get-NetAdapter -Name '"+literal+"').Status"],capture_output=True,text=True)
    if adapter.returncode or adapter.stdout.strip()!='Disconnected':raise SystemExit('Use a disconnected Wi-Fi adapter; existing connections are preserved.')
    hotspot=json.loads((root/'hotspot-config.json').read_text())
    assert hotspot['password']==''
    if usb_status()['network_mode']!='home':
        with serial.Serial(args.port,115200,timeout=2,write_timeout=2) as device:
            device.write(b'\n');time.sleep(.05);device.reset_input_buffer();device.write(b'WIFI HOME\n')
            assert device.readline().startswith(b'OK')
    initial=wait_host(args.home_host,'home',seconds=30)
    raw(args.home_host,'/api/stop',True);initial=status(args.home_host);page(args.home_host)
    assert raw(args.home_host,'/api/wifi',body=b'ssid=Lab&password=eight123&open=0',auth=False)[0]==403
    for body in [b'ssid=Lab&password=short&open=0',b'ssid=Lab&password=&open=0',b'ssid=A&ssid=B&password=eight123&open=0',b'ssid=bad%00name&password=eight123&open=0',b'ssid=%gg&password=eight123&open=0']:
        assert raw(args.home_host,'/api/wifi',body=body)[0]==400
    assert raw(args.home_host,'/api/wifi',body=b'x'*513)[0]==413
    assert status(args.home_host)['home_settings_saved']==initial['home_settings_saved']
    record('unauthorized, malformed, duplicate and oversized Wi-Fi forms reject without changing saved settings')
    command(args.home_host,'network',name='hotspot')
    time.sleep(1)
    profile_file.write_text('<?xml version="1.0"?><WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1"><name>'+profile+'</name><SSIDConfig><SSID><name>'+escape(hotspot['ssid'])+'</name></SSID></SSIDConfig><connectionType>ESS</connectionType><connectionMode>manual</connectionMode><MSM><security><authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption></security></MSM></WLANProfile>',encoding='utf-8')
    netsh('add','profile','filename='+str(profile_file),'interface='+args.interface,'user=current');created=True
    s=wait_host('192.168.4.1','hotspot',rejoin=True)
    assert s['hotspot_open'] and s['hotspot_clients']>=1
    record('PC joins Freenove-Rover without a password; actual dashboard/status respond')
    for name in ['rover.local','connectivitycheck.gstatic.com','captive.apple.com']:portal_dns(name)
    for path in ['/generate_204','/hotspot-detect.html','/connecttest.txt','/ncsi.txt']:
        code,headers,_=raw('192.168.4.1',path)
        assert code==302 and headers['Location']=='http://192.168.4.1/'
    record('local DNS and Android/Apple/Windows portal probes redirect to the dashboard')
    times=[]
    for _ in range(5):
        began=time.perf_counter();html=page('192.168.4.1');times.append(time.perf_counter()-began)
        assert 'id="wifiForm"' in html and 'with no password' in html
    record('five full dashboard downloads over car Wi-Fi',median_seconds=round(statistics.median(times),3),slowest_seconds=round(max(times),3))
    # Fragment a valid-length but invalid-key request to verify the nonblocking body reader.
    invalid=b'ssid=Lab&password=short&open=0'
    headers=('POST /api/wifi HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Type: application/x-www-form-urlencoded\r\nX-Car-Token: '+token+'\r\nX-Car-Owner: wifi-setup-check\r\nContent-Length: '+str(len(invalid))+'\r\n\r\n').encode()
    with socket.create_connection(('192.168.4.1',80),timeout=4) as sock:
        sock.sendall(headers+invalid[:8]);time.sleep(.7);sock.sendall(invalid[8:]);reply=b''
        while True:
            data=sock.recv(4096)
            if not data:break
            reply+=data
    assert reply.startswith(b'HTTP/1.1 400')
    record('fragmented Wi-Fi POST body waits for complete credentials without blocking the server')
    assert form('192.168.4.1',ssid='Freenove-Lab-Unavailable-Test',password='test-only123',open=0)[0]==200
    began=time.monotonic();time.sleep(3)
    s=wait_host('192.168.4.1','hotspot',seconds=60,rejoin=True)
    assert s['wifi_join_state']=='failed' and s['home_settings_saved']==initial['home_settings_saved']
    record('failed home join returns to open hotspot and keeps prior settings',seconds=round(time.monotonic()-began,2))
    settings=json.loads((root/'wifi-config.json').read_text(encoding='utf-8-sig'))
    asyncio.run(browser_pair(settings))
    s=wait_host(args.home_host,'home')
    assert s['wifi_join_state']=='joined' and s['home_settings_saved']
    record('real mobile dashboard form joins and saves the home network; password cleared after submission')
    asyncio.run(capture_setup())
    page(args.home_host);command(args.home_host,'network',name='hotspot')
    wait_host('192.168.4.1','hotspot',rejoin=True)
    update=subprocess.run([sys.executable,str(root/'wireless_update.py'),'--host','192.168.4.1'],capture_output=True,text=True)
    if update.returncode:raise RuntimeError('Wireless update via open hotspot failed')
    s=wait_host('192.168.4.1','hotspot',rejoin=True)
    assert s['home_settings_saved'] and s['ota_window_s']==0
    record('authenticated OTA over open Wi-Fi; saved home settings survive reboot; motors remain stopped')
    page('192.168.4.1');command('192.168.4.1','network',name='home')
    s=wait_host(args.home_host,'home');assert s['home_settings_saved']
    record('Retry saved home Wi-Fi reconnects using the settings stored on the Pico')
    if args.leave_hotspot:
        page(args.home_host);command(args.home_host,'network',name='hotspot')
        s=wait_host('192.168.4.1','hotspot',rejoin=True)
    success=True
finally:
    # Leave the requested car mode, but restore the PC's previously disconnected adapter.
    try:
        host='192.168.4.1' if args.leave_hotspot or not success else args.home_host
        raw(host,'/api/stop',True)
    except (OSError,ValueError):pass
    if created:
        netsh('disconnect','interface='+args.interface,required=False)
        netsh('delete','profile','name='+profile,'interface='+args.interface,required=False)
    if profile_file.exists():profile_file.unlink()
    (root/'wifi-setup-check.json').write_text(json.dumps(dict(passed=success,results=results,final=s if 's' in globals() else None),indent=2),encoding='utf-8')
