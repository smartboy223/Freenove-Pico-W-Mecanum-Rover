"""Live LAN control checks. --lifted explicitly enables short wheel movements."""
import argparse
import json
import re
import socket
import time
import urllib.request
import urllib.error
import urllib.parse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--host', default='192.168.0.202')
parser.add_argument('--lifted', action='store_true')
args = parser.parse_args()
base = 'http://' + args.host
owner = 'lifted-verification'
token = ''
results = []

def request(path, post=False, auth=True, who=owner):
    headers = {'X-Car-Token': token, 'X-Car-Owner': who} if auth else {}
    req = urllib.request.Request(base+path, data=b'' if post else None, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=4) as response:
            return response.status, response.read().decode()
    except urllib.error.HTTPError as error:
        return error.code, error.read().decode()

def command(op, **values):
    code, text = request('/api/control?'+urllib.parse.urlencode({'op':op, **values}), True)
    assert code == 200, (code, text)
    return json.loads(text)

def status():
    code, text = request('/api/status')
    assert code == 200
    return json.loads(text)

def record(name, **extra):
    results.append({'check': name, 'passed': True, **extra})
    print('PASS:', name)

def stopped():
    s = status()
    assert not s['armed'] and not s['moving'] and s['wheels'] == [0]*4, s
    return s

try:
    request('/api/stop', True, False)
    code, page = request('/')
    assert code == 200
    token = re.search(r"const token='([a-f0-9]+)'", page)[1]
    assert status()['firmware'] == 'CarReady-2.5'
    record('new dashboard and stopped boot', status=stopped())
    assert request('/api/control?op=arm&guard=1', True, False)[0] == 403
    record('unauthorized control rejected')
    command('arm', guard=1)
    assert request('/api/control?op=heartbeat', True, who='another-phone')[0] == 409
    record('second controller rejected')
    assert request('/api/control?op=drive&x=0&y=0&r=0&speed=99', True)[0] == 400
    stopped()
    record('invalid speed stops and disarms')
    assert request('/api/control?op=mode&name=show&speed=25', True)[0] == 400
    record('show requires lifted-car acknowledgement')
    command('arm', guard=1)
    time.sleep(.9)
    stopped()
    record('idle manual lease expires')
    command('calibrate')
    command('mode', name='light', speed=20, threshold=80)
    time.sleep(.25)
    s=status()
    assert not s['moving'], s
    request('/api/stop', True, False)
    record('light mode waits without a bright target', status=s)
    command('servo', angle=60)
    assert stopped()['head_angle'] == 60
    command('servo', angle=90)
    command('led', color='blue')
    command('beep')
    command('led', color='auto')
    record('head, RGB and buzzer commands accepted')
    s=status()
    assert 6.7 < s['battery_volts'] < 8.8 and 0 <= s['battery_percent'] <= 100, s
    record('voltage and estimated battery level', volts=s['battery_volts'], estimated_percent=s['battery_percent'])
    for effect in ['yellow','cyan','purple','white','rainbow','chase','breathe']:
        command('led', color=effect)
        assert status()['led_effect'] == effect
    command('brightness',value=25)
    assert status()['brightness'] == 25
    assert request('/api/control?op=brightness&value=100',True)[0] == 400
    command('alerts',value=0)
    assert not status()['sound_alerts']
    command('alerts',value=1)
    command('party',sound=1)
    time.sleep(1)
    assert stopped()['led_effect'] == 'party'
    request('/api/stop',True,False)
    assert status()['led_effect'] == 'auto'
    command('brightness',value=12)
    record('extra LED effects, brightness, stationary party, and sound toggle')
    if args.lifted:
        for enabled in (0, 1):
            command('mode', name='pilot', speed=20, seconds=5, backtrack=enabled)
            assert status()['backtrack_enabled'] == bool(enabled)
            request('/api/stop', True, False)
        assert request('/api/control?op=mode&name=pilot&speed=20&backtrack=2',True)[0] == 400
        stopped()
        record('recent-path retreat toggle and bounds')
        # Fixed expected wheel patterns from Freenove's movement examples.
        vectors = [
            ('forward',0,1,0,[-25,-25,-25,-25]),
            ('reverse',0,-1,0,[25,25,25,25]),
            ('crab left',-1,0,0,[-25,25,-25,25]),
            ('crab right',1,0,0,[25,-25,25,-25]),
            ('rotate left',0,0,-1,[-25,-25,25,25]),
            ('rotate right',0,0,1,[25,25,-25,-25]),
            ('forward left',-1,1,0,[-25,0,-25,0]),
            ('forward right',1,1,0,[0,-25,0,-25]),
            ('reverse left',-1,-1,0,[0,25,0,25]),
            ('reverse right',1,-1,0,[25,0,25,0]),
        ]
        for name,x,y,r,expected in vectors:
            command('arm', guard=0)  # Lifted wheels only: inspect mixer independent of echoes.
            command('drive', x=x, y=y, r=r, speed=25)
            s=status()
            assert s['wheels'] == expected and s['moving'], s
            command('halt')
            assert not status()['moving']
            request('/api/stop', True, False)
            record(name+' and release stop', wheels=s['wheels'])
        command('arm', guard=0)
        command('drive', x=1, y=0, r=0, speed=25)
        # A partial HTTP client must not keep motors running past their lease.
        with socket.create_connection((args.host,80), timeout=3) as slow:
            slow.sendall(b'GET / HTTP/1.1\r\n')
            time.sleep(.75)
            s=stopped()
        record('movement expires with stalled HTTP client', status=s)
        assert request('/api/control?op=mode&name=pilot&speed=20&seconds=601',True)[0] == 400
        record('roaming duration bounds enforced')
        command('mode',name='pilot',speed=20,seconds=15,autonomous=1,scanleft=60)
        started=time.monotonic()
        time.sleep(2.5)  # Longer than old heartbeat lease; do not renew from the client.
        s=status()
        assert s['armed'] and s['autonomous'] and s['remaining_s'] > 0, s
        samples=[s]
        while time.monotonic()-started < 15.5:
            s=status();samples.append(s)
            assert max(abs(v) for v in s['wheels']) <= (25 if s['roam_action'].startswith('Turning') or s['pilot_stage']==6 else 20)
            time.sleep(.2)
        stopped()
        angles={s['head_angle'] for s in samples}
        assert {30,60,90,120,150}.issubset(angles), angles
        assert any(s['mode']=='pilot' and s['moving'] for s in samples), 'No clear path produced motion in this environment'
        Path('roaming-samples.json').write_text(json.dumps(samples,indent=2))
        record('independent timed roaming: five scan directions and deadline stop', angles=sorted(angles), final=samples[-1])
        for name in ['line', 'pilot']:
            command('mode', name=name, speed=20)
            samples=[]
            started=time.monotonic()
            while time.monotonic()-started < 1.4:
                s=status()
                samples.append(s)
                assert s['armed'] and s['mode'] == name, s
                if name == 'line':
                    bits=(s['line'][0]<<2)|(s['line'][1]<<1)|s['line'][2]
                    if s['line_black']==0: bits^=7
                    if bits==7 or (bits==0 and time.monotonic()-started>.55):
                        assert not s['moving'], s
                assert max(abs(v) for v in s['wheels']) <= (25 if name=='pilot' and s['pilot_stage']==6 else 20)
                command('heartbeat')
                time.sleep(.16)
            time.sleep(1.2)
            stopped()
            record(name+' mode and lost-controller stop', samples=samples)
        command('mode', name='show', lift=1, speed=25)
        samples=[]
        started=time.monotonic()
        while time.monotonic()-started < 10.6:
            s=status()
            samples.append(s)
            assert max(abs(v) for v in s['wheels']) <= 25
            if s['armed']:
                code, body = request('/api/control?op=heartbeat', True)
                if code == 409:
                    # The show can finish between the status read and renewal.
                    final = stopped()
                    assert time.monotonic()-started >= 9.8 and final['notice'] == 'Show finished', final
                    samples.append(final)
                    break
                assert code == 200, (code, body)
            time.sleep(.16)
        stopped()
        patterns={tuple(s['wheels']) for s in samples if s['moving']}
        assert len(patterns) == 8, patterns
        record('ten-second show: eight patterns then automatic stop', patterns=[list(p) for p in sorted(patterns)])
        Path('show-samples.json').write_text(json.dumps(samples,indent=2))
    print('Checks completed; car stopped.')
finally:
    request('/api/stop', True, False)
    Path('control-check.json').write_text(json.dumps(results,indent=2))
