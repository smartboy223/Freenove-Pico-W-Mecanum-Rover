"""Check guarded forward refusal with an owner-positioned close obstacle, wheels lifted."""
import json
import re
import time
import urllib.request
from pathlib import Path

base='http://192.168.0.202'
token=''
def request(path, post=False):
    req=urllib.request.Request(base+path, data=b'' if post else None,
        headers={'X-Car-Token':token,'X-Car-Owner':'close-obstacle-check'})
    with urllib.request.urlopen(req,timeout=3) as reply:
        return reply.read().decode()
def status():
    return json.loads(request('/api/status'))

report={'before':[], 'guarded':[], 'passed':False}
try:
    request('/api/stop',True)
    token=re.search(r"const token='([a-f0-9]+)'",request('/'))[1]
    for _ in range(10):
        report['before'].append(status())
        time.sleep(.16)
    close=[s['front_cm'] for s in report['before'] if s['front_cm'] is not None and s['front_cm']<35]
    assert close, 'No close, reliable echoes: do not start the check.'
    request('/api/control?op=arm&guard=1',True)
    for _ in range(5):
        request('/api/control?op=drive&x=0&y=1&r=0&speed=20',True)
        s=status()
        report['guarded'].append(s)
        assert not s['moving'] and s['wheels']==[0]*4, s
        assert 'Forward blocked' in s['notice'], s
        time.sleep(.16)
    report['passed']=True
    print('PASS: close obstacle blocked all five forward requests; all wheel outputs stayed zero.')
    print('Reliable close distances:',close)
finally:
    request('/api/stop',True)
    report['final']=status()
    Path('obstacle-check.json').write_text(json.dumps(report,indent=2))
