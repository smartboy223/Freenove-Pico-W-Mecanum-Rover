"""Live matrix-mode checks; --lifted permits brief wheel movements."""
import argparse
import json
import re
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', default='192.168.0.202')
parser.add_argument('--lifted', action='store_true')
args = parser.parse_args()
if not args.lifted:
    raise SystemExit('Lift all four wheels, then explicitly use --lifted.')
base = 'http://' + args.host
token = ''
results = []

def request(path, post=False):
    req = urllib.request.Request(base + path, data=b'' if post else None,
        headers={'X-Car-Token': token, 'X-Car-Owner': 'matrix-verification'})
    try:
        with urllib.request.urlopen(req, timeout=3) as response:
            return response.status, response.read().decode()
    except urllib.error.HTTPError as error:
        return error.code, error.read().decode()

def command(op, **values):
    code, body = request('/api/control?' + urllib.parse.urlencode({'op': op, **values}), True)
    assert code == 200, (code, body)

def status():
    code, body = request('/api/status')
    assert code == 200
    return json.loads(body)

def stopped():
    s = status()
    assert not s['armed'] and not s['moving'] and s['wheels'] == [0] * 4, s
    return s

def record(name, **values):
    results.append({'check': name, 'passed': True, **values})
    print('PASS:', name, flush=True)

try:
    request('/api/stop', True)
    code, page = request('/')
    assert code == 200
    token = re.search(r"const token='([a-f0-9]+)'", page)[1]
    initial = stopped()
    assert initial['firmware'] == 'CarReady-2.11', initial['firmware']
    assert initial['module'] == 'matrix', 'Fit the LED matrix and reboot first.'
    assert initial['front_cm'] is None and initial['distance_cm'] is None
    assert all(text in page for text in ('moduleControls', 'LED matrix fitted', 'guardHelp'))
    record('matrix detected; dashboard labels unavailable distance sensing')

    faces=['auto','eyes','happy','heart','angry','sad','wink','surprised','sleepy',
        'cool','party','forward','reverse','crab_left','crab_right','forward_left',
        'forward_right','reverse_left','reverse_right','turn_left','turn_right','off']
    for face in faces:
        command('matrix',face=face,brightness=4)
        s=stopped()
        assert s['matrix_face']==face and len(s['matrix_rows'])==8
        assert s['matrix_active']==('eyes' if face=='auto' else face)
        if face=='off':assert s['matrix_rows']==[0]*8
    record('all 22 matrix selections accepted without wheel movement')
    for rotation in (0,90,180,270):
        command('matrix',face='happy',brightness=4,rotation=rotation)
        s=stopped();assert s['matrix_rotation']==rotation
        mapped=[0]*8
        for y,row in enumerate(s['matrix_rows']):
            for x in range(16):
                if not row&(1<<x):continue
                local=x%8
                rx,ry=(local,y) if rotation==0 else (7-y,local) if rotation==90 else (7-local,7-y) if rotation==180 else (y,7-local)
                mapped[ry]|=1<<((x//8)*8+rx)
        assert s['matrix_wire']==list(reversed(mapped)),s
    record('all four alignments produce the expected full-panel I2C buffer while stopped')
    for brightness in (1,15,4):
        command('matrix',face='happy',brightness=brightness)
        assert stopped()['matrix_brightness']==brightness
    for values in ({'face':'invalid','brightness':4},{'face':'sad','brightness':0},
                   {'face':'sad','brightness':16},{'face':'sad','rotation':45},
                   {'face':'sad','rotation':360}):
        code,body=request('/api/control?'+urllib.parse.urlencode({'op':'matrix',**values}),True)
        assert code==400,(code,body)
        s=stopped();assert s['matrix_face']=='happy' and s['matrix_brightness']==4 and s['matrix_rotation']==270
    record('brightness endpoints accepted; invalid matrix settings preserve the display')
    command('matrix',face='auto',brightness=4)

    for requested in (1, 0):
        command('arm', guard=requested)
        assert not status()['front_guard']
        command('drive', x=0, y=1, r=0, speed=20)
        s = status()
        assert s['moving'] and s['wheels'] == [-20] * 4, s
        request('/api/stop', True)
        stopped()
        record('matrix forward works with requested guard ' + str(requested))

    vectors = [
        ('forward', 0, 1, 0, [-20, -20, -20, -20]),
        ('reverse', 0, -1, 0, [20, 20, 20, 20]),
        ('crab left', -1, 0, 0, [-20, 20, -20, 20]),
        ('crab right', 1, 0, 0, [20, -20, 20, -20]),
        ('rotate left', 0, 0, -1, [-20, -20, 20, 20]),
        ('rotate right', 0, 0, 1, [20, 20, -20, -20]),
        ('forward left', -1, 1, 0, [-20, 0, -20, 0]),
        ('forward right', 1, 1, 0, [0, -20, 0, -20]),
        ('reverse left', -1, -1, 0, [0, 20, 0, 20]),
        ('reverse right', 1, -1, 0, [20, 0, 20, 0]),
    ]
    for name, x, y, rotation, expected in vectors:
        command('arm', guard=1)
        command('drive', x=x, y=y, r=rotation, speed=20)
        time.sleep(.18)
        s = status()
        assert s['moving'] and s['wheels'] == expected and not s['front_guard'], s
        expected_face=name.replace('rotate','turn').replace(' ','_')
        assert s['matrix_active']==expected_face,(name,s['matrix_active'])
        command('halt')
        assert not status()['moving']
        request('/api/stop', True)
        stopped()
        record(name + ' and release stop', wheels=expected)

    command('arm', guard=1)
    command('drive', x=0, y=1, r=0, speed=20)
    time.sleep(.75)
    stopped()
    record('matrix forward expires without renewal')

    for mode in ('pilot', 'line', 'light'):
        code, body = request('/api/control?' + urllib.parse.urlencode(
            {'op': 'mode', 'name': mode, 'speed': 20, 'threshold': 3}), True)
        assert code == 409 and 'ultrasonic' in body.lower(), (code, body)
        stopped()
    record('guarded automatic modes require ultrasonic module')
finally:
    request('/api/stop', True)
    if token:command('matrix',face='auto',brightness=4)
    Path('matrix-check.json').write_text(json.dumps(
        {'results': results, 'final': stopped()}, indent=2), encoding='utf-8')
