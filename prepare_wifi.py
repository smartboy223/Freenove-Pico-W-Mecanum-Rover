"""Generate a local firmware header without logging Wi-Fi credentials."""
import json
from pathlib import Path

root = Path(__file__).resolve().parent
settings = json.loads((root / "wifi-config.json").read_text(encoding="utf-8-sig"))
ssid, password = settings.get("ssid"), settings.get("password")
if not isinstance(ssid, str) or not 1 <= len(ssid.encode("utf-8")) <= 32:
    raise SystemExit("Wi-Fi name must be between 1 and 32 UTF-8 bytes.")
if not isinstance(password, str) or not (8 <= len(password.encode("utf-8")) <= 63 or (len(password) == 64 and all(c in "0123456789abcdefABCDEF" for c in password))):
    raise SystemExit("Enter an 8–63 byte WPA password or a 64-character hexadecimal key.")
header = "#pragma once\nconst char WIFI_SSID[] = " + json.dumps(ssid, ensure_ascii=False) + ";\nconst char WIFI_PASSWORD[] = " + json.dumps(password, ensure_ascii=False) + ";\n"
(root / "firmware/CarReady/wifi_credentials.h").write_text(header, encoding="utf-8")
print("Wi-Fi configuration validated and prepared; password not displayed.")
