"""Generate a local firmware header without logging Wi-Fi credentials."""
import json
import secrets
import re
from pathlib import Path

root = Path(__file__).resolve().parent
settings = json.loads((root / "wifi-config.json").read_text(encoding="utf-8-sig"))
ssid, password = settings.get("ssid"), settings.get("password")
if not isinstance(ssid, str) or not 1 <= len(ssid.encode("utf-8")) <= 32:
    raise SystemExit("Wi-Fi name must be between 1 and 32 UTF-8 bytes.")
if not isinstance(password, str) or not (password=='' or 8 <= len(password.encode("utf-8")) <= 63 or (len(password) == 64 and all(c in "0123456789abcdefABCDEF" for c in password))):
    raise SystemExit("Enter an 8–63 byte WPA password, a 64-character hexadecimal key, or an empty password for open Wi-Fi.")
header = "#pragma once\nconst char WIFI_SSID[] = " + json.dumps(ssid, ensure_ascii=False) + ";\nconst char WIFI_PASSWORD[] = " + json.dumps(password, ensure_ascii=False) + ";\n"
ota_file=root/'ota-config.json'
if not ota_file.exists():
    ota_file.write_text(json.dumps({'password':secrets.token_urlsafe(24)},indent=2)+'\n',encoding='utf-8')
ota=json.loads(ota_file.read_text(encoding='utf-8-sig')).get('password')
if not isinstance(ota,str) or not 16<=len(ota)<=64:
    raise SystemExit('OTA password must contain 16 to 64 characters in local ota-config.json.')
header+='const char OTA_PASSWORD[] = '+json.dumps(ota)+';\n'
hotspot_file=root/'hotspot-config.json'
if not hotspot_file.exists():
    hotspot_file.write_text(json.dumps({'ssid':'Freenove-Rover',
        'password':''},indent=2)+'\n',encoding='utf-8')
hotspot=json.loads(hotspot_file.read_text(encoding='utf-8-sig'))
if not isinstance(hotspot.get('ssid'),str) or not re.fullmatch(r'[A-Za-z0-9 _-]{1,32}',hotspot['ssid']):
    raise SystemExit('Hotspot name must contain 1 to 32 letters, numbers, spaces, hyphens or underscores.')
key=hotspot.get('password')
if not isinstance(key,str) or (key!='' and (not 8<=len(key)<=63 or any(not 32<=ord(c)<=126 for c in key) or key.startswith('CHANGE-ME'))):
    raise SystemExit('Use an empty hotspot password for open access, or an 8 to 63 character ASCII password.')
header+='const char HOTSPOT_SSID[] = '+json.dumps(hotspot['ssid'])+';\n'
header+='const char HOTSPOT_PASSWORD[] = '+json.dumps(key)+';\n'
(root / "firmware/CarReady/wifi_credentials.h").write_text(header, encoding="utf-8")
print("Wi-Fi configuration validated and prepared; password not displayed.")
