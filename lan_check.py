"""Exercise the Pico HTTP server with desktop and mobile-style requests."""
import concurrent.futures
import json
import socket
import time
from pathlib import Path

HOST='192.168.0.202'
def check(name, delay=0, large=False):
    try:
        with socket.create_connection((HOST,80),timeout=5) as connection:
            connection.settimeout(5)
            connection.sendall(b'GET /api/status HTTP/1.1\r\n')
            if delay:
                time.sleep(delay)
            padding='X-Mobile-Test: '+('x'*2500)+'\r\n' if large else ''
            headers=f'Host: {HOST}\r\n{padding}Connection: close\r\n\r\n'
            connection.sendall(headers.encode())
            reply=b''
            while True:
                part=connection.recv(4096)
                if not part:
                    break
                reply+=part
            passed=reply.startswith(b'HTTP/1.1 200')
            return {'check':name,'passed':passed,'bytes':len(reply)}
    except OSError as error:
        return {'check':name,'passed':False,'error':str(error)}

results=[check('normal request'),check('headers delayed 700ms',delay=.7),check('large mobile headers',large=True)]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results.extend(pool.map(lambda n:check(f'concurrent client {n}'),range(1,5)))
print(json.dumps(results,indent=2))
Path('lan-request-check.json').write_text(json.dumps(results,indent=2))
