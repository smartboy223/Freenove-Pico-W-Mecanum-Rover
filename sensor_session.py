"""30-second staged ultrasonic/flashlight check. No motor movement."""
import json
import re
import time
import urllib.request
from pathlib import Path
base='http://192.168.0.202'
token=''
def request(path,post=False):
    r=urllib.request.Request(base+path,data=b'' if post else None,
        headers={'X-Car-Token':token,'X-Car-Owner':'sensor-verification'})
    with urllib.request.urlopen(r,timeout=3) as response:return response.read().decode()
request('/api/stop',True)
token=re.search(r"const token='([a-f0-9]+)'",request('/'))[1]
request('/api/control?op=servo&angle=90',True)
request('/api/control?op=calibrate',True)
readings=[]
start=time.monotonic()
print('Recording: 10s near/far object, 10s left flashlight, 10s right flashlight.',flush=True)
try:
    while time.monotonic()-start<30:
        s=json.loads(request('/api/status'))
        assert not s['armed'] and not s['moving'], s
        s['elapsed']=round(time.monotonic()-start,2)
        s['phase']=min(2,int(s['elapsed']//10))
        readings.append(s)
        time.sleep(.15)
finally:
    request('/api/stop',True)
    phases=[[s for s in readings if s['phase']==p] for p in range(3)]
    distances=[s['front_cm'] for s in phases[0] if s['front_cm'] is not None]
    response=[max([s['light_delta'][i] for s in phases[i+1]] or [0]) for i in range(2)]
    summary={'samples':len(readings),'all_stopped':True,
        'ultrasonic_filtered_range':[min(distances),max(distances)] if distances else None,
        'ultrasonic_near_far_response':bool(distances and min(distances)<25 and max(distances)>35),
        'flashlight_left_right_max_delta':response,
        'flashlight_left_right_response':[v>=3 for v in response],
        'light_baseline':readings[0]['light_baseline'] if readings else None,
        'light_ranges':[[min(s['light'][i] for s in readings),max(s['light'][i] for s in readings)] for i in range(2)] if readings else None}
    Path('sensor-session-2.1.json').write_text(json.dumps({'summary':summary,'readings':readings},indent=2))
    print(json.dumps(summary,indent=2))
