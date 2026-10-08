"""Live 2.3 checks that never command wheel movement."""
import json
import re
import time
import urllib.request
import urllib.error
from pathlib import Path
BASE='http://192.168.0.202'
token=''
results=[]
def request(path,post=False):
    req=urllib.request.Request(BASE+path,data=b'' if post else None,
        headers={'X-Car-Token':token,'X-Car-Owner':'studio-check'})
    try:
        with urllib.request.urlopen(req,timeout=4) as response:
            return response.status,response.read().decode()
    except urllib.error.HTTPError as error:
        return error.code,error.read().decode()
def command(query):
    code,body=request('/api/control?'+query,True)
    assert code==200,(code,body)
def status():
    code,body=request('/api/status');assert code==200
    s=json.loads(body);assert not s['armed'] and not s['moving'] and s['wheels']==[0]*4
    assert s['firmware']=='CarReady-2.11'
    return s
def record(name,**values):
    results.append({'check':name,'passed':True,**values});print('PASS:',name,flush=True)
try:
    request('/api/stop',True)
    code,page=request('/');assert code==200
    token=re.search(r"const token='([a-f0-9]+)'",page)[1]
    status()
    assert all('id="panel-'+name+'"' in page for name in ['drive','roam','sensors','studio'])
    record('all four dashboard panels served')
    for volume,pitch in [(25,600),(60,1400),(100,2400),(0,2000)]:
        command(f'op=sound&volume={volume}&pitch={pitch}')
        s=status();assert s['sound_volume']==volume and s['tone_hz']==pitch
        command('op=beep');time.sleep(.35)
    record('four volume/pitch settings including mute; wheels stopped')
    for query in ['op=sound&volume=101&pitch=2000','op=sound&volume=60&pitch=399','op=sound&volume=-1&pitch=3001']:
        assert request('/api/control?'+query,True)[0]==400
        s=status();assert s['sound_volume']==0 and s['tone_hz']==2000
    record('invalid sound settings rejected without changing state')
    command('op=sound&volume=60&pitch=2000')
    command('op=melody');assert status()['playing_melody']
    time.sleep(2);assert not status()['playing_melody']
    record('six-note chime finishes while stopped')
    command('op=party&sound=1');time.sleep(1)
    assert status()['led_effect']=='party'
    request('/api/stop',True);assert status()['led_effect']=='auto'
    record('changing-note stationary party; Stop clears it')
    for angle in [30,90,150,90]:
        command('op=servo&angle='+str(angle))
        assert status()['head_angle']==angle
    record('wide servo endpoints accepted with wheels stopped')
    fitted=status()['module']
    expected=409 if fitted=='matrix' else 400
    assert request('/api/control?op=mode&name=pilot&speed=20&seconds=10&escape=2',True)[0]==expected
    status();record('matrix automatic-mode gate rejects before arming' if fitted=='matrix' else 'invalid escape option rejected before arming')
finally:
    request('/api/stop',True)
    Path('studio-check.json').write_text(json.dumps({'results':results,'final':status()},indent=2))
