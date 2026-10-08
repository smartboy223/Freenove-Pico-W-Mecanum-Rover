"""Find the Pico's dashboard and open it; never arm, flash or move the car."""
import argparse
import ipaddress
import json
import time
import urllib.request
import webbrowser
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CACHE = ROOT / 'dashboard-address.json'


def valid_host(value):
    if value == 'freenove-car.local':
        return value
    try:
        address = ipaddress.IPv4Address(value)
        return str(address) if address.is_private and not address.is_unspecified else None
    except (ValueError, TypeError):
        return None


def usb_address():
    try:
        import serial
        from serial.tools import list_ports
        ports = [port.device for port in list_ports.comports() if port.vid == 0x2E8A]
        if len(ports) != 1:
            return None
        with serial.Serial(ports[0], 115200, timeout=.6, write_timeout=.6) as device:
            device.write(b'\n')
            time.sleep(.1)
            device.reset_input_buffer()
            device.write(b'STATUS\n')
            for _ in range(3):
                line = device.readline()
                if line.startswith(b'{'):
                    status = json.loads(line)
                    if status.get('board') == 'Pico W':
                        return valid_host(status.get('ip'))
    except (ImportError, OSError, ValueError):
        pass
    return None


def find_dashboard(host=None):
    cached = None
    try:
        cached = valid_host(json.loads(CACHE.read_text(encoding='utf-8'))['host'])
    except (OSError, ValueError, KeyError):
        pass
    candidates = [valid_host(host)] if host else [usb_address(), cached, '192.168.0.202', '192.168.4.1', 'freenove-car.local']
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    for address in dict.fromkeys(value for value in candidates if value):
        try:
            with opener.open(f'http://{address}/api/status', timeout=2) as response:
                status = json.loads(response.read(65536))
            if status.get('board') != 'Pico W' or not status.get('firmware', '').startswith('CarReady-'):
                continue
            if not status.get('http_listening'):
                continue
            try:
                CACHE.write_text(json.dumps({'host': address})+'\n', encoding='utf-8')
            except OSError:
                pass
            return f'http://{address}/'
        except (OSError, ValueError):
            continue
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', help='Optional current local IP address of the car')
    parser.add_argument('--no-browser', action='store_true', help='Check the address without opening a browser')
    args = parser.parse_args()
    if args.host and not valid_host(args.host):
        parser.error('Use a local IPv4 address or freenove-car.local.')
    print('Finding your Freenove rover dashboard...', flush=True)
    url = find_dashboard(args.host)
    if not url:
        print('Car dashboard is not reachable yet.')
        print('Power the car ON, wait up to 30 seconds, and connect to the same home Wi-Fi.')
        print('If home Wi-Fi is unavailable, join Freenove-Rover and open http://192.168.4.1/.')
        print('For another router address, use: start.bat --host YOUR-CAR-IP')
        return 1
    print('Dashboard ready: '+url)
    if not args.no_browser and not webbrowser.open(url):
        print('Open this address in your browser: '+url)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
