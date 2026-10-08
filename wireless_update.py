"""Upload a locally built Pico W firmware over Wi-Fi with the wheels stopped."""
import argparse
import importlib.util
import json
import os
import re
import socket
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', default='192.168.0.202')
parser.add_argument('--binary', type=Path, default=root/'build/CarReady.ino.bin')
parser.add_argument('--port', type=int, default=32382, help='PC TCP port used for the transfer')
args = parser.parse_args()
if not args.binary.is_file():
    raise SystemExit('Build the firmware first with build.ps1.')
password = json.loads((root/'ota-config.json').read_text(encoding='utf-8-sig'))['password']
tool_root = Path(os.environ['LOCALAPPDATA'])/'Arduino15/packages/rp2040/hardware/rp2040'
uploaders = list(tool_root.glob('*/tools/espota.py'))
if not uploaders:
    raise SystemExit('Install the Arduino-Pico core before updating.')
uploader = max(uploaders, key=lambda p: tuple(int(n) for n in p.parent.parent.name.split('.')))
spec = importlib.util.spec_from_file_location('pico_ota', uploader)
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)
base = 'http://'+args.host
token = ''

def request(path, post=False):
    req = urllib.request.Request(base+path, data=b'' if post else None,
        headers={'X-Car-Token':token, 'X-Car-Owner':'wireless-update'})
    with urllib.request.urlopen(req, timeout=4) as response:
        return json.loads(response.read()) if path != '/' else response.read().decode()

def window(value):
    request('/api/control?'+urllib.parse.urlencode({'op':'ota','value':value}), True)

try:
    request('/api/stop', True)
    token = re.search(r"const token='([a-f0-9]+)'", request('/'))[1]
    before = request('/api/status')
    if not before.get('ota_supported'):
        raise SystemExit('Install firmware 2.8 or later by USB once to enable wireless updates.')
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
        route.connect((args.host,80))
        source = route.getsockname()[0]
    window(1)
    locked = request('/api/status')
    assert not locked['armed'] and not locked['moving'] and locked['ota_window_s'] > 0
    # Check the maintenance gate before transferring. A successful arm is a failure.
    try:
        request('/api/control?op=arm&guard=0', True)
        raise RuntimeError('Maintenance window did not block movement.')
    except urllib.error.HTTPError as error:
        assert error.code == 409
    print('Car stopped; protected wireless update window opened.', flush=True)
    # Keep the password out of command lines and process listings.
    result = ota.serve(args.host, source, 2040, args.port, password, str(args.binary), ota.FLASH)
    if result != 0:
        raise RuntimeError('Wireless transfer failed. Check PC firewall permissions for Python.')
    deadline = time.monotonic()+60
    final = None
    time.sleep(2)
    while time.monotonic() < deadline:
        try:
            current = request('/api/status')
            if current['uptime_ms'] < locked['uptime_ms'] and not current['ota_window_s']:
                final = current
                break
        except (OSError, ValueError):
            pass
        time.sleep(1)
    if final is None:
        raise RuntimeError('Transfer completed, but reboot was not confirmed. Check USB status.')
    assert not final['armed'] and not final['moving'] and final['wheels'] == [0]*4
    (root/'wireless-update.json').write_text(json.dumps({'passed':True,
        'binary_bytes':args.binary.stat().st_size,'movement_blocked':True,
        'reboot_confirmed':True,'final':final},indent=2),encoding='utf-8')
    print('PASS: wireless transfer and reboot confirmed; car stopped and disarmed.')
finally:
    try:
        request('/api/stop', True)
        window(0)
    except (OSError, ValueError):
        pass
