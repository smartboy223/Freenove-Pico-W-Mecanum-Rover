"""Lifted-car light-follow test; owner holds flashlight on a front-corner light sensor."""
import json
import re
import time
import urllib.request
from pathlib import Path
base='http://192.168.0.202'
token=''
def request(path,post=False):
    r=urllib.request.Request(base+path,data=b'' if post else None,
        headers={'X-Car-Token':token,'X-Car-Owner':'light-follow-verification'})
    with urllib.request.urlopen(r,timeout=3) as response:return response.read().decode()
request('/api/stop',True)
token=re.search(r"const token='([a-f0-9]+)'",request('/'))[1]
samples=[]
try:
    request('/api/control?op=mode&name=light&speed=15&threshold=3',True)
    started=time.monotonic()
    while time.monotonic()-started<12:
        request('/api/control?op=heartbeat',True)
        s=json.loads(request('/api/status'));samples.append(s)
        assert max(abs(v) for v in s['wheels'])<=15
        time.sleep(.16)
finally:
    request('/api/stop',True)
    summary={'samples':len(samples),'target_detected_samples':sum(max(s['light_delta'])>=3 for s in samples),
        'moving_samples':sum(s['moving'] for s in samples),
        'light_max':[max(s['light'][i] for s in samples) for i in range(2)] if samples else [],
        'notices':sorted({s['notice'] for s in samples}),
        'final':json.loads(request('/api/status'))}
    Path('light-follow-check.json').write_text(json.dumps({'summary':summary,'samples':samples},indent=2))
    print(json.dumps(summary,indent=2))
