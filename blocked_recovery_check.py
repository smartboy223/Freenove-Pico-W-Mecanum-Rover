"""Lifted-wheel check of braking and recent-path retreat with a staged simulated wall."""
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
parser.add_argument('--port')
parser.add_argument('--lifted',action='store_true',help='Confirm every wheel is securely clear of the ground')
args=parser.parse_args()
if not args.lifted:raise SystemExit('Lift all wheels, then explicitly use --lifted. This test moves the car.')
ports=[p.device for p in list_ports.comports() if p.vid==0x2E8A]
if not args.port and len(ports)!=1:raise SystemExit('Connect one Pico or specify --port.')
base='http://'+args.host
def request(path,post=False):
    with urllib.request.urlopen(urllib.request.Request(base+path,data=b'' if post else None),timeout=3) as response:
        return response.read().decode()
def status():return json.loads(request('/api/status'))
samples=[]
report={'test':'lifted recent-path blocked-route recovery','sensor_fault_injection':True,'passed':False}
try:
    request('/api/stop',True)
    initial=status();assert initial['firmware']=='CarReady-2.6'
    assert not initial['armed'] and not initial['moving']
    report['initial']=initial
    assert initial['front_cm'] is not None and initial['front_cm']>=55, 'Recovery needs a reliable clear front (55 cm minimum); move nearby objects away before testing.'
    with serial.Serial(args.port or ports[0],115200,timeout=2,write_timeout=2) as device:
        device.write(b'\n');time.sleep(.15);device.reset_input_buffer()
        device.write(b'TESTBLOCKED LIFTED\n')
        acknowledgement=device.readline().decode().strip()
        assert acknowledgement.startswith('OK lifted blocked-route test'),acknowledgement
    started=time.monotonic()
    while time.monotonic()-started<18.7:
        s=status();samples.append({'elapsed_s':round(time.monotonic()-started,3),**s})
        assert max(map(abs,s['wheels']))<=25
        if s['test_wall_phase']==1 and s['moving']:
            assert all(v>0 for v in s['wheels']), 'Simulated close wall must permit only a bounded retreat'
        if not s['armed']:break
        time.sleep(.12)
    before=[s for s in samples if s['test_wall_phase']==0 and all(v<0 for v in s['wheels'])]
    reverse=[s for s in samples if s['test_wall_phase']==1 and all(v>0 for v in s['wheels'])]
    after=[s for s in samples if s['test_wall_phase']==2 and all(v<0 for v in s['wheels'])]
    assert before, 'No real clear front produced initial travel credit'
    assert reverse, 'Confirmed blocked route never produced recent-path retreat'
    assert after, 'Real ranging was restored but no clear route resumed'
    assert max(s['retreats'] for s in samples)<=2
    assert all(s['alert'] not in ('sensor','obstacle','blocked') for s in after), 'Resolved range warning remained after verified forward recovery'
    assert all(s['front_cm'] is not None and s['front_cm']>=45 for s in after)
    assert max(abs(v) for s in reverse for v in s['wheels'])<=18
    final=samples[-1];assert not final['armed'] and final['wheels']==[0]*4 and not final['test_echo_muted'] and final['test_wall_phase']==-1
    assert final['notice']=='Roaming timer finished',final['notice']
    report.update(passed=True,initial_forward_samples=len(before),reverse_samples=len(reverse),
                  resumed_forward_samples=len(after),reverse_wheels=reverse[0]['wheels'],
                  resumed_s=after[0]['elapsed_s'],final=final,samples=samples)
    print(f"PASS: real initial forward ({len(before)} samples); simulated wall triggered short retreat ({len(reverse)} samples).")
    print(f"PASS: real sonar restored; forward resumed ({len(after)} samples) at {after[0]['elapsed_s']} seconds.")
    print('PASS: 18-second hardware deadline stopped/disarmed and cleared the diagnostic.')
finally:
    try:request('/api/stop',True)
    except OSError:
        try:
            with serial.Serial(args.port or ports[0],115200,timeout=2,write_timeout=2) as device:device.write(b'\nSTOP\n')
        except (OSError,serial.SerialException):pass
    report['samples']=samples
    Path('blocked-recovery-check.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
