"""Stopped-car checks for full pages, aborted clients and hardware recovery."""
import concurrent.futures
import json
import re
import socket
import time
import urllib.request
from pathlib import Path
import serial

HOST = '192.168.0.202'
BASE = 'http://' + HOST
results = []

def request(path, post=False, token=''):
    req = urllib.request.Request(BASE+path, data=b'' if post else None,
        headers={'X-Car-Token':token, 'X-Car-Owner':'http-recovery-check'})
    with urllib.request.urlopen(req, timeout=6) as response:
        body = response.read()
        assert len(body) == int(response.headers['Content-Length'])
        return body.decode()

def status():
    value = json.loads(request('/api/status'))
    if value['armed'] or value['moving'] or value['wheels'] != [0]*4:
        Path('http-recovery-unexpected-state.json').write_text(json.dumps(value,indent=2))
        request('/api/stop', True)
        raise AssertionError('Unexpected active control during stopped-car check; stopped; status saved')
    return value

def page(_=None):
    body = request('/')
    assert '</html>' in body.lower() and "const token='" in body
    return len(body.encode())

request('/api/stop', True)
token = re.search(r"const token='([a-f0-9]+)'", request('/'))[1]
request('/api/control?op=calibrate', True, token)
assert status()['light_calibrated']
sizes = [page() for _ in range(20)]
results.append({'check':'20 complete dashboard downloads', 'passed':True, 'page_bytes':sizes})
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    sizes = list(pool.map(page, range(12)))
results.append({'check':'12 dashboard downloads in groups of four', 'passed':True, 'page_bytes':sizes})
for index in range(12):
    with socket.create_connection((HOST,80), timeout=3) as connection:
        connection.sendall(b'GET / HTTP/1.1\r\nHost: car\r\n\r\n')
        if index % 2:
            connection.recv(100)
    time.sleep(.1)
    assert status()['light_calibrated'], 'Unexpected firmware restart during aborted requests'
results.append({'check':'12 aborted dashboard requests; server stays running', 'passed':True})
with socket.create_connection((HOST,80), timeout=3) as connection:
    connection.sendall(b'GET / HTTP/1.1\r\nHost: car\r\n')
    time.sleep(.2)
    started=time.monotonic()
    with serial.Serial('COM3',115200,timeout=2,write_timeout=2) as device:
        device.write(b'\nSTATUS\n')
        value=json.loads(device.readline().decode())
        assert not value['moving'] and not value['armed']
    results.append({'check':'USB remains responsive with unfinished HTTP request', 'passed':True,
                    'response_seconds':round(time.monotonic()-started,3)})
request('/api/stop', True)
before=status()
with serial.Serial('COM3',115200,timeout=2,write_timeout=2) as device:
    device.write(b'\nTESTWATCHDOG\n')
    acknowledgement=device.readline().decode().strip()
    assert acknowledgement.startswith('OK watchdog recovery test')
started=time.monotonic()
usb_recovered=None
lan_recovered=None
while time.monotonic()-started<65:
    if usb_recovered is None and time.monotonic()-started>=3:
        try:
            with serial.Serial('COM3',115200,timeout=1,write_timeout=1) as device:
                device.write(b'\nSTATUS\n')
                value=json.loads(device.readline().decode())
                assert not value['armed'] and value['wheels']==[0]*4
                assert not value['light_calibrated'], 'Watchdog did not restart firmware'
                usb_recovered=round(time.monotonic()-started,2)
        except (OSError, ValueError, serial.SerialException):
            pass
    if usb_recovered is not None:
        try:
            value=status()
            assert not value['light_calibrated']
            lan_recovered=round(time.monotonic()-started,2)
            break
        except (OSError, ValueError):
            pass
    time.sleep(.5)
assert usb_recovered is not None and lan_recovered is not None, 'Recovery deadline exceeded'
results.append({'check':'intentional main-loop freeze recovers stopped on USB and LAN',
    'passed':True, 'usb_recovered_seconds':usb_recovered, 'lan_recovered_seconds':lan_recovered,
    'acknowledgement':acknowledgement, 'final':status()})
Path('http-recovery-check.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
