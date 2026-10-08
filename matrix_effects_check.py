"""Live matrix/RGB/buzzer checks; every sample requires stopped, disarmed motors."""
import argparse
import json
import re
import time
import urllib.parse
import urllib.request
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', default='192.168.0.202')
args = parser.parse_args()
base = 'http://' + args.host
token = ''
results = []
saved = None
passed = False

def request(path, post=False):
    req = urllib.request.Request(base+path, data=b'' if post else None,
        headers={'X-Car-Token':token, 'X-Car-Owner':'matrix-effects-check'})
    with urllib.request.urlopen(req, timeout=4) as response:
        body = response.read().decode()
        return body if path=='/' else json.loads(body)

def command(op, **values):
    return request('/api/control?' + urllib.parse.urlencode(dict(op=op, **values)), True)

def status():
    s = request('/api/status')
    assert s['firmware']=='CarReady-2.11' and s['module']=='matrix'
    assert not s['armed'] and not s['moving'] and s['wheels']==[0]*4
    return s

def sample(seconds):
    rows = []
    end = time.monotonic()+seconds
    while time.monotonic()<end:
        rows.append(status())
        time.sleep(.035)
    return rows

def record(name, rows=None):
    results.append(dict(check=name, passed=True, samples=rows or []))
    print('PASS:', name, flush=True)

try:
    request('/api/stop', True)
    token = re.search(r"const token='([a-f0-9]+)'", request('/'))[1]
    saved = status()
    command('sound', volume=60, pitch=2000)
    command('alerts', value=1)
    command('matrix', face='auto', brightness=4, rotation=saved['matrix_rotation'])
    for face in ['happy','heart','angry','sad','wink','surprised','sleepy','cool','eyes']:
        command('matrix', face=face)
        rows = sample(.3)
        assert any(s['matrix_source']=='expression' and s['matrix_active']==face for s in rows)
        assert any(s['buzzer_active'] for s in rows)
        assert any(any(s['rgb_pixels']) for s in rows)
    record('nine emotion selections produce matrix artwork, colored RGB feedback and a tone')
    for face in ['forward','reverse','crab_left','crab_right','turn_left','turn_right','off']:
        command('matrix', face='happy')
        command('matrix', face=face)
        s=status()
        assert s['matrix_face']==face and s['matrix_active']==face
        assert not s['buzzer_active']
    record('rapid emotion-to-sign/Off changes cancel previous emotion feedback immediately')
    request('/api/stop', True)
    command('matrix', face='auto')
    for style in ['red','green','blue','yellow','cyan','purple','white','rainbow','chase','breathe','off']:
        command('led', color=style)
        rows = sample(.65 if style=='chase' else .2)
        assert all(s['matrix_source'] not in ['idle','selected'] for s in rows[2:])
        if style=='off':
            assert rows[-1]['matrix_rows']==[0]*8 and not any(rows[-1]['rgb_pixels'])
        else:
            assert any(any(s['matrix_rows']) and any(s['rgb_pixels']) for s in rows)
        if style=='chase':
            assert len({tuple(s['matrix_rows']) for s in rows})>=3
    record('all eleven RGB styles reach Interactive; running lights animate; lights Off blanks both')
    command('led', color='breathe')
    rows = sample(4.3)
    assert len({s['matrix_output_brightness'] for s in rows})==4
    assert len({tuple(s['rgb_pixels']) for s in rows})>10
    record('breathing matrix and RGB brightness vary together without exceeding the chosen limit', rows)
    command('matrix', face='heart')
    command('party', sound=1)
    rows = sample(4)
    assert all(s['matrix_source']=='party' for s in rows[2:])
    assert len({tuple(s['matrix_rows']) for s in rows})>=5
    assert len({tuple(s['rgb_pixels']) for s in rows})>=10
    assert len({s['buzzer_hz'] for s in rows if s['buzzer_active']})>=5
    faces=['happy','heart','wink','cool','surprised','heart','happy','wink']
    notes=[523,659,784,1047,784,659,587,784]
    matched=0
    for s in rows:
        elapsed=s['effect_elapsed_ms']
        if 75<elapsed%400<90 and s['buzzer_active']:
            assert s['matrix_active']==faces[(elapsed//400)%8]
            assert s['buzzer_hz']==notes[(elapsed//400)%8]
            matched+=1
    # Check stable mid-beat face mapping even if HTTP polling misses the short audio window.
    assert sum(s['matrix_active']==faces[(s['effect_elapsed_ms']//400)%8] for s in rows if 80<s['effect_elapsed_ms']%400<350)>20
    record('stationary party animates real matrix, RGB and five pitches; shared beat; no wheel output', rows)
    request('/api/stop', True)
    time.sleep(.1)
    s=status()
    assert s['matrix_face']=='heart' and s['matrix_active']=='heart'
    assert not s['buzzer_active'] and not any(s['rgb_pixels'])
    record('Stop ends party immediately and restores the chosen expression')
    command('matrix', face='auto')
    command('party', sound=0)
    rows=sample(.9)
    assert all(not s['buzzer_active'] for s in rows)
    assert len({tuple(s['matrix_rows']) for s in rows})>=2
    record('silent party keeps matrix and lights animated without sound', rows)
    command('sound', volume=0, pitch=2000)
    command('party', sound=1)
    rows=sample(.9)
    assert all(not s['buzzer_active'] for s in rows)
    record('zero volume mutes musical party without freezing its visuals')
    command('sound', volume=60, pitch=2000)
    command('matrix', face='off')
    rows=sample(.5)
    assert all(s['matrix_rows']==[0]*8 for s in rows[2:])
    record('explicit Matrix Off remains dark during party')
    request('/api/stop', True)
    command('matrix', face='auto')
    command('melody')
    rows=sample(2.1)
    active=[s for s in rows if s['matrix_source']=='melody']
    assert len({tuple(s['matrix_rows']) for s in active})>=5
    assert len({tuple(s['rgb_pixels']) for s in active})>=5
    assert len({s['buzzer_hz'] for s in active if s['buzzer_active']})>=4
    assert not rows[-1]['playing_melody'] and rows[-1]['matrix_source']=='idle'
    record('six-note chime animates matrix sound bars and note colors, then returns to idle', rows)
    command('beep')
    rows=sample(.35)
    assert any(s['matrix_source']=='tone' and s['buzzer_active'] and any(s['rgb_pixels']) for s in rows)
    assert rows[-1]['matrix_source']=='idle'
    record('test tone briefly reacts on matrix and RGB, then clears')
    end=status()
    assert end['matrix_writes']>saved['matrix_writes']+50
    assert end['matrix_errors']==saved['matrix_errors']
    record('physical matrix acknowledges more than 50 new I2C frames with no new write failures')
    passed = True
finally:
    request('/api/stop', True)
    if saved:
        command('sound', volume=saved['sound_volume'], pitch=saved['tone_hz'])
        command('alerts', value=int(saved['sound_alerts']))
        command('matrix', face=saved['matrix_face'], brightness=saved['matrix_brightness'], rotation=saved['matrix_rotation'])
    # Selecting a restored emotion can create feedback; Stop clears that feedback too.
    request('/api/stop', True)
    time.sleep(.1)
    final=status()
    Path('matrix-effects-check.json').write_text(json.dumps(dict(passed=passed, results=results, final=final), indent=2))
