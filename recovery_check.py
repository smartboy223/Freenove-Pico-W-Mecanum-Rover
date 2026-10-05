"""Lifted-wheel check of production heading recovery using a bounded echo dropout."""
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
report={'test':'lifted echo-dropout heading recovery','sensor_fault_injection':True,'passed':False}
try:
    request('/api/stop',True)
    initial=status();assert initial['firmware']=='CarReady-2.6'
    assert not initial['armed'] and not initial['moving']
    report['initial']=initial
    assert initial['front_cm'] is not None and initial['front_cm']>=55, 'Recovery needs a reliable clear front (55 cm minimum); move nearby objects away before testing.'
    with serial.Serial(args.port or ports[0],115200,timeout=2,write_timeout=2) as device:
        device.write(b'\n');time.sleep(.15);device.reset_input_buffer()
        device.write(b'TESTNOECHO LIFTED\n')
        acknowledgement=device.readline().decode().strip()
        assert acknowledgement.startswith('OK lifted recovery test'),acknowledgement
    started=time.monotonic()
    while time.monotonic()-started<15.7:
        s=status();samples.append({'elapsed_s':round(time.monotonic()-started,3),**s})
        assert max(map(abs,s['wheels']))<=25
        if s['test_echo_muted']:
            assert s['front_cm'] is None
            assert sum(s['wheels'])==0,'Echo dropout must never authorize forward movement'
        if not s['armed']:break
        time.sleep(.12)
    turn_samples=[s for s in samples if s['test_echo_muted'] and s['moving']]
    assert turn_samples,'The missing-echo scan never produced a recovery turn'
    directions={tuple(1 if v>0 else -1 if v<0 else 0 for v in s['wheels']) for s in turn_samples}
    assert len(directions)==1,'Recovery oscillated instead of inspecting the other side'
    assert max(s['escape_turns'] for s in samples)>=2
    forward=[s for s in samples if not s['test_echo_muted'] and s['moving'] and all(v<0 for v in s['wheels'])]
    assert forward,'Real sensor restored, but this placement never provided a clear forward route'
    assert all(s['front_cm'] is not None and s['front_cm']>=45 for s in forward)
    final=samples[-1];assert not final['armed'] and final['wheels']==[0]*4 and not final['test_echo_muted']
    assert final['notice']=='Roaming timer finished',final['notice']
    report.update(passed=True,turn_samples=len(turn_samples),forward_samples=len(forward),
                  turn_wheels=turn_samples[0]['wheels'],first_forward_s=forward[0]['elapsed_s'],
                  final=final,samples=samples)
    print(f"PASS: missing echoes triggered two same-direction turns ({len(turn_samples)} wheel samples).")
    print(f"PASS: real sonar restored; forward resumed in {len(forward)} samples at {forward[0]['elapsed_s']} seconds.")
    print('PASS: 15-second hardware deadline ended stopped/disarmed; diagnostic cleared.')
finally:
    try:request('/api/stop',True)
    except OSError:
        try:
            with serial.Serial(args.port or ports[0],115200,timeout=2,write_timeout=2) as device:device.write(b'\nSTOP\n')
        except (OSError,serial.SerialException):pass
    report['samples']=samples
    Path('recovery-check.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
