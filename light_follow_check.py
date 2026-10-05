"""Timed lifted-wheel flashlight check, using real light and ultrasonic readings."""
import argparse
import json
import re
import time
import urllib.request
from pathlib import Path
import serial
from serial.tools import list_ports

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host',default='192.168.0.202')
parser.add_argument('--lifted',action='store_true',help='Confirm every wheel is clear of the ground')
parser.add_argument('--seconds',type=int,default=32)
parser.add_argument('--phase',choices=['sequence','left','right','straight','dark','any'],default='sequence')
args=parser.parse_args()
if not args.lifted:raise SystemExit('Lift every wheel, then explicitly use --lifted; this test can move the car.')
if not 5<=args.seconds<=60:raise SystemExit('Use 5 to 60 seconds for this hardware check.')
base='http://'+args.host
token=''
samples=[]
report={'test':'independent timed real flashlight following','phase':args.phase,'passed':False}

def request(path,post=False):
    r=urllib.request.Request(base+path,data=b'' if post else None,
        headers={'X-Car-Token':token,'X-Car-Owner':'light-follow-verification'})
    with urllib.request.urlopen(r,timeout=3) as response:return response.read().decode()
def status():return json.loads(request('/api/status'))
try:
    request('/api/stop',True)
    token=re.search(r"const token='([a-f0-9]+)'",request('/'))[1]
    initial=status();report['initial']=initial
    assert initial['firmware']=='CarReady-2.6'
    assert initial['light_calibrated'],'Set the ambient baseline with the flashlight OFF first.'
    assert initial['front_cm'] is not None and initial['front_cm']>=35,'Clear the front and keep hands out of the sonar beam.'
    request(f'/api/control?op=mode&name=light&speed=25&threshold=3&seconds={args.seconds}&autonomous=1',True)
    started=time.monotonic()
    # Deliberately send no heartbeat: the Pico must own its timer.
    while time.monotonic()-started<args.seconds+1:
        s=status();samples.append({'elapsed_s':round(time.monotonic()-started,3),**s})
        assert max(map(abs,s['wheels']))<=25
        if s['armed']:
            assert s['mode']=='light' and s['autonomous'] and s['head_angle']==90
        if not s['armed']:break
        time.sleep(.12)
    left=[s for s in samples if s['moving'] and s['wheels']==[-25,-25,25,25]]
    right=[s for s in samples if s['moving'] and s['wheels']==[25,25,-25,-25]]
    straight=[s for s in samples if s['moving'] and s['wheels']==[-25]*4]
    dark=[s for s in samples if s['armed'] and not s['light_target'] and not s['moving'] and max(s['light_delta'])<3]
    counts={'left':len(left),'right':len(right),'straight':len(straight),'dark':len(dark)}
    report['counts']=counts
    report['notices']=sorted({s['notice'] for s in samples})
    report['light_max']=[max(s['light'][i] for s in samples) for i in (0,1)]
    required=['left','right','straight','dark'] if args.phase=='sequence' else [args.phase] if args.phase!='any' else []
    for phase in required:assert counts[phase]>=3,f'No sustained {phase} response; inspect the saved readings and notices.'
    if args.phase=='any':assert any(s['moving'] for s in samples),'No bright target produced wheel movement.'
    final=samples[-1];report['final']=final
    assert not final['armed'] and not final['moving'] and final['wheels']==[0]*4
    assert final['notice']=='Flashlight timer finished',final['notice']
    assert final['remaining_s']==0 and not final['autonomous']
    report['passed']=True
    print('PASS: real flashlight response samples:',counts)
    print(f'PASS: {args.seconds}-second timer ran without heartbeats and finished stopped/disarmed.')
finally:
    report['samples']=samples
    if samples:report['final']=samples[-1]
    Path('light-follow-check.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    try:request('/api/stop',True)
    except OSError:
        for port in list_ports.comports():
            if port.vid==0x2E8A:
                try:
                    with serial.Serial(port.device,115200,timeout=1,write_timeout=1) as device:device.write(b'\nSTOP\n')
                except (OSError,serial.SerialException):pass
