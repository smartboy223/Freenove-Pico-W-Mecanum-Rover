"""Windows stopped-car check: home failure, hotspot dashboard/OTA, home recovery.

Uses the PC's disconnected Wi-Fi adapter and removes its temporary profile.
No wheel movement is requested. USB remains connected for recovery.
"""
import argparse
import json
import re
import subprocess
import sys
import time
import uuid
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from xml.sax.saxutils import escape
import serial

root=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port',default='COM3')
parser.add_argument('--interface',default='Wi-Fi')
parser.add_argument('--home-host',default='192.168.0.202')
args=parser.parse_args()
profile='Freenove-Rover-Verification-'+uuid.uuid4().hex[:8]
settings=json.loads((root/'hotspot-config.json').read_text())
profile_file=root/'build/hotspot-test-profile.xml'
results=[]
token=''
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))

def usb(command):
    with serial.Serial(args.port,115200,timeout=2,write_timeout=2) as device:
        device.write(b'\n');time.sleep(.05);device.reset_input_buffer()
        device.write((command+'\n').encode())
        reply=device.readline().decode().strip()
        if not reply:raise RuntimeError('Pico did not answer USB.')
        return json.loads(reply) if command=='STATUS' else reply

def stopped(s):
    assert not s['armed'] and not s['moving'] and s['wheels']==[0]*4,s
    return s

def http(host,path,post=False):
    request=urllib.request.Request('http://'+host+path,data=b'' if post else None,
        headers={'X-Car-Token':token,'X-Car-Owner':'hotspot-check'})
    with opener.open(request,timeout=3) as response:
        body=response.read().decode()
        return body if path=='/' else json.loads(body)

def record(name,**values):
    results.append({'check':name,'passed':True,**values})
    print('PASS:',name,flush=True)

def netsh(*values):
    result=subprocess.run(['netsh','wlan',*values],capture_output=True,text=True)
    if result.returncode:raise RuntimeError('Windows Wi-Fi operation failed: '+result.stdout.strip())

created=False
try:
    literal=args.interface.replace("'","''")
    adapter=subprocess.run(['powershell','-NoProfile','-Command',
        "(Get-NetAdapter -Name '"+literal+"').Status"],capture_output=True,text=True)
    if adapter.returncode or adapter.stdout.strip()!='Disconnected':
        raise SystemExit('Use a disconnected Wi-Fi adapter for this test; existing connections are preserved.')
    usb('STOP');initial=stopped(usb('STATUS'))
    assert initial['firmware']=='CarReady-2.11' and initial['network_mode']=='home'
    page=http(args.home_host,'/');token=re.search(r"const token='([a-f0-9]+)'",page)[1]
    try:
        http(args.home_host,'/api/control?op=network&name=invalid',True)
        raise AssertionError('Invalid network mode accepted.')
    except urllib.error.HTTPError as error:
        assert error.code==400
    record('home dashboard available; invalid network mode rejected')
    assert usb('WIFI TESTOFFLINE').startswith('OK')
    began=time.monotonic();deadline=began+45;seen_connecting=False
    while time.monotonic()<deadline:
        s=stopped(usb('STATUS'))
        if s['network_mode']=='connecting':seen_connecting=True
        if s['network_mode']=='hotspot':break
        time.sleep(.5)
    assert seen_connecting and s['network_mode']=='hotspot' and s['ip']=='192.168.4.1'
    elapsed=time.monotonic()-began
    assert elapsed>=29 and not s['home_wifi_connected']
    record('failed home association falls back to car hotspot while stopped',seconds=round(elapsed,2))
    ssid=escape(settings['ssid']);password=escape(settings['password']);secured=bool(password)
    security=('<authentication>WPA2PSK</authentication><encryption>AES</encryption><useOneX>false</useOneX></authEncryption><sharedKey><keyType>passPhrase</keyType><protected>false</protected><keyMaterial>'+password+'</keyMaterial></sharedKey>' if secured else '<authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption>')
    profile_file.write_text('<?xml version="1.0"?>\n'
        '<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">'
        '<name>'+profile+'</name><SSIDConfig><SSID><name>'+ssid+'</name></SSID></SSIDConfig>'
        '<connectionType>ESS</connectionType><connectionMode>manual</connectionMode>'
        '<MSM><security><authEncryption>'+security+'</security></MSM></WLANProfile>',encoding='utf-8')
    netsh('add','profile','filename='+str(profile_file),'interface='+args.interface,'user=current');created=True
    netsh('connect','name='+profile,'ssid='+settings['ssid'],'interface='+args.interface)
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
        try:
            s=stopped(http('192.168.4.1','/api/status'));break
        except (OSError,ValueError):time.sleep(1)
    else:raise RuntimeError('PC could not reach the car hotspot dashboard.')
    assert s['network_mode']=='hotspot' and s['hotspot_clients']>=1
    page=http('192.168.4.1','/');token=re.search(r"const token='([a-f0-9]+)'",page)[1]
    assert 'id="networkInfo"' in page and 'id="homeWifi"' in page
    record('PC joined car hotspot; real dashboard/status served without home Wi-Fi',clients=s['hotspot_clients'])
    if s['module']=='matrix':
        http('192.168.4.1','/api/control?op=matrix&face=happy&rotation=270&brightness=4',True)
        assert stopped(http('192.168.4.1','/api/status'))['matrix_face']=='happy'
        http('192.168.4.1','/api/control?op=matrix&face=auto',True)
    record('dashboard accessory commands respond through hotspot; wheels stopped')
    update=subprocess.run([sys.executable,str(root/'wireless_update.py'),'--host','192.168.4.1'],
        capture_output=True,text=True)
    if update.returncode:
        raise RuntimeError('Hotspot firmware update failed: '+update.stdout.strip()+' '+update.stderr[-1600:])
    s=stopped(http('192.168.4.1','/api/status'))
    assert s['network_mode']=='hotspot' and not s['ota_window_s']
    record('real authenticated hotspot firmware update; reboot stays on hotspot and stopped')
    # Use the same production request as the dashboard's Retry home Wi-Fi button.
    page=http('192.168.4.1','/');token=re.search(r"const token='([a-f0-9]+)'",page)[1]
    http('192.168.4.1','/api/control?op=network&name=home',True)
    deadline=time.monotonic()+40
    while time.monotonic()<deadline:
        try:
            s=stopped(http(args.home_host,'/api/status'))
            if s['network_mode']=='home':break
        except (OSError,ValueError):pass
        time.sleep(.5)
    else:raise RuntimeError('Home Wi-Fi did not recover.')
    record('dashboard request returns to home Wi-Fi; car remains stopped',ip=s['ip'])
finally:
    try:
        usb('STOP')
        if usb('STATUS')['network_mode']!='home':usb('WIFI HOME')
    except (OSError,ValueError,RuntimeError):pass
    if created:
        netsh('disconnect','interface='+args.interface)
        netsh('delete','profile','name='+profile,'interface='+args.interface)
    if profile_file.exists():profile_file.unlink()
    (root/'hotspot-check.json').write_text(json.dumps({'results':results},indent=2),encoding='utf-8')
