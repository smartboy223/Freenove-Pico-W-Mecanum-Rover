"""Stopped-car Wi-Fi discovery, saved/deleted persistence and recovery check.

Temporarily forgets the home network and restores wifi-config.json. Requires
USB, a disconnected Windows Wi-Fi adapter, and explicit --allow-forget.
Optional --test-update also verifies deletion survives a real firmware update.
"""
import argparse
import json
import re
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from xml.sax.saxutils import escape
import serial
from serial.tools import list_ports

ROOT=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--home-host',default='192.168.0.202')
parser.add_argument('--interface',default='Wi-Fi')
parser.add_argument('--allow-forget',action='store_true')
parser.add_argument('--test-update',action='store_true')
args=parser.parse_args()
if not args.allow_forget:parser.error('This check temporarily forgets Wi-Fi; use --allow-forget to authorize it.')
home=json.loads((ROOT/'wifi-config.json').read_text(encoding='utf-8-sig'))
hotspot=json.loads((ROOT/'hotspot-config.json').read_text(encoding='utf-8-sig'))
assert not hotspot.get('password'),'This check requires the open development hotspot.'
ports=[p.device for p in list_ports.comports() if p.vid==0x2E8A]
assert len(ports)==1,'Connect exactly one Pico USB device.'
port=ports[0];owner='network-settings-check';token='';results=[]
profile='Freenove-Settings-Check';xml=ROOT/'build/network-settings-profile.xml'
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
restore_needed=False;profile_added=False

def usb(command='STATUS'):
    with serial.Serial(port,115200,timeout=1,write_timeout=1) as device:
        device.write(b'\n');time.sleep(.05);device.reset_input_buffer();device.write((command+'\n').encode())
        line=device.readline()
        if command!='STATUS':assert line.startswith(b'OK'),line;return
        value=json.loads(line)
        assert not value['armed'] and not value['moving'] and value['wheels']==[0]*4
        return value

def wait_usb(mode,seconds=50):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        try:
            s=usb()
            if s['network_mode']==mode and s['http_listening']:return s
        except (OSError,ValueError,serial.SerialException):pass
        time.sleep(.4)
    raise RuntimeError('Timed out waiting for '+mode)

def request(host,path,body=None,auth=True):
    headers={'X-Car-Token':token if auth else 'invalid','X-Car-Owner':owner}
    if body is not None:headers['Content-Type']='application/x-www-form-urlencoded'
    req=urllib.request.Request('http://'+host+path,data=body,headers=headers)
    with opener.open(req,timeout=4) as response:return response.read()

def page(host):
    global token
    html=request(host,'/').decode();token=re.search(r"const token='([a-f0-9]+)'",html)[1];return html

def status(host):
    s=json.loads(request(host,'/api/status'))
    assert not s['armed'] and not s['moving'] and s['wheels']==[0]*4
    assert home['password'].encode() not in request(host,'/api/status')
    return s

def netsh(*values):
    result=subprocess.run(['netsh','wlan',*values],capture_output=True,text=True)
    return result.returncode==0

def join_pc():
    netsh('disconnect','interface='+args.interface)
    netsh('show','networks','mode=bssid');time.sleep(2)
    netsh('connect','name='+profile,'ssid='+hotspot['ssid'],'interface='+args.interface)
    deadline=time.monotonic()+25;retry=time.monotonic()+12
    while time.monotonic()<deadline:
        try:page('192.168.4.1');return
        except (OSError,ValueError,TypeError):pass
        if time.monotonic()>retry:
            netsh('connect','name='+profile,'ssid='+hotspot['ssid'],'interface='+args.interface);retry=time.monotonic()+12
        time.sleep(.6)
    raise RuntimeError('PC could not reach the car hotspot.')

def record(name,**details):
    results.append(dict(check=name,passed=True,**details));print('PASS: '+name,flush=True)

def scan(host):
    request(host,'/api/wifi/scan',b'');deadline=time.monotonic()+15
    while time.monotonic()<deadline:
        value=json.loads(request(host,'/api/wifi/scan'))
        if value['state']!='scanning':break
        time.sleep(.4)
    assert value['state']=='ready' and value['networks']
    names=[n['ssid'] for n in value['networks']]
    assert len(names)==len(set(names)) and home['ssid'] in names
    assert [n['rssi'] for n in value['networks']]==sorted([n['rssi'] for n in value['networks']],reverse=True)
    return len(names)

try:
    adapter=subprocess.run(['powershell','-NoProfile','-Command',"(Get-NetAdapter -Name '"+args.interface.replace("'","''")+"').Status"],capture_output=True,text=True,check=True)
    assert adapter.stdout.strip()=='Disconnected','Disconnect PC Wi-Fi first; Ethernet can stay connected.'
    usb('STOP');page(args.home_host);s=status(args.home_host)
    assert s['home_settings_source']=='saved' and s['home_ssid']==home['ssid'] and s['connected_ssid']==home['ssid']
    assert s['dashboard_url']=='http://'+args.home_host+'/' and s['wifi_signal_dbm'] is not None
    record('Saved network, actual connection, signal and working URL; passwords absent from status')
    record('Live home-mode discovery lists unique networks strongest first',networks=scan(args.home_host))
    try:request(args.home_host,'/api/wifi/forget',b'',auth=False);raise AssertionError('Unauthorized forget accepted')
    except urllib.error.HTTPError as error:assert error.code==403
    assert status(args.home_host)['home_settings_source']=='saved'
    record('Unauthorized deletion rejected without changing saved settings')
    xml.parent.mkdir(exist_ok=True)
    xml.write_text('<?xml version="1.0"?><WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1"><name>'+profile+'</name><SSIDConfig><SSID><name>'+escape(hotspot['ssid'])+'</name></SSID></SSIDConfig><connectionType>ESS</connectionType><connectionMode>manual</connectionMode><MSM><security><authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption></security></MSM></WLANProfile>',encoding='utf-8')
    assert netsh('add','profile','filename='+str(xml),'interface='+args.interface);profile_added=True
    bad=urllib.parse.urlencode(dict(ssid='Freenove-No-WiFi-Test',password='not-a-real-key',open='0')).encode()
    request(args.home_host,'/api/wifi',bad);wait_usb('hotspot');join_pc();s=status('192.168.4.1')
    assert s['home_settings_source']=='saved' and s['home_ssid']==home['ssid'] and s['wifi_last_result']=='failed'
    record('Failed joining preserves previous saved network and reports failure')
    record('Live hotspot-mode discovery works without switching off the dashboard',networks=scan('192.168.4.1'))
    restore_needed=True;request('192.168.4.1','/api/wifi/forget',b'');wait_usb('hotspot');join_pc();s=status('192.168.4.1')
    assert s['home_settings_source']=='none' and s['home_ssid']==s['saved_ssid']=='' and s['wifi_last_result']=='forgotten'
    record('Forget removes saved settings and suppresses compiled home defaults')
    usb('TESTWATCHDOG');time.sleep(3);wait_usb('hotspot');join_pc();s=status('192.168.4.1')
    assert s['home_settings_source']=='none' and s['wifi_last_result']=='forgotten'
    record('Deleted settings remain deleted after hardware restart')
    if args.test_update:
        subprocess.run(['python',str(ROOT/'wireless_update.py'),'--host','192.168.4.1'],cwd=ROOT,check=True)
        page('192.168.4.1');s=status('192.168.4.1');assert s['home_settings_source']=='none' and s['wifi_last_result']=='forgotten'
        record('Actual wireless firmware update preserves deleted state')
    good=urllib.parse.urlencode(dict(ssid=home['ssid'],password=home['password'],open='0' if home['password'] else '1')).encode()
    request('192.168.4.1','/api/wifi',good);wait_usb('home');page(args.home_host);s=status(args.home_host)
    assert s['home_settings_source']=='saved' and s['home_ssid']==s['connected_ssid']==home['ssid'] and s['wifi_last_result']=='joined'
    restore_needed=False
    record('New save restores home Wi-Fi with visible successful connection and LAN URL')
    usb('TESTWATCHDOG');time.sleep(3);wait_usb('home');s=status(args.home_host)
    assert s['home_settings_source']=='saved' and s['wifi_last_result']=='joined' and s['connected_ssid']==home['ssid']
    record('Successful save and confirmation survive restart')
    (ROOT/'network-settings-check.json').write_text(json.dumps({'passed':True,'results':results,'final':{'network_mode':s['network_mode'],'ip':s['ip'],'home_settings_source':s['home_settings_source'],'moving':s['moving']}},indent=2),encoding='utf-8')
finally:
    if restore_needed:
        try:
            usb('WIFI HOTSPOT');wait_usb('hotspot');join_pc()
            good=urllib.parse.urlencode(dict(ssid=home['ssid'],password=home['password'],open='0' if home['password'] else '1')).encode()
            request('192.168.4.1','/api/wifi',good);wait_usb('home')
        except Exception:print('Recovery needed: use the car hotspot form with your local wifi-config.json settings.',flush=True)
    else:
        try:
            s=usb()
            if s['network_mode']!='home' and s.get('home_ssid')==home['ssid']:
                usb('WIFI HOME');wait_usb('home')
        except Exception:pass
    try:usb('STOP')
    except Exception:pass
    if profile_added:netsh('disconnect','interface='+args.interface);netsh('delete','profile','name='+profile,'interface='+args.interface)
    xml.unlink(missing_ok=True)
