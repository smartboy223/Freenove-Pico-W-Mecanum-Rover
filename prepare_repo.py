"""Create a clean local repository copy; never initializes or pushes Git."""
import hashlib
import json
import shutil
from pathlib import Path

root=Path(__file__).resolve().parent
destination=root/'repo-export'
files=['README.md','REMOTE-GUIDE.md','THIRD-PARTY.md','.gitignore','requirements.txt',
       'wifi-config.example.json','hotspot-config.example.json','build.ps1','test.ps1','flash.ps1','Flash-Car.bat',
       'Check-Car.bat','Check-Ultrasonic.bat','prepare_wifi.py','prepare_dashboard.py',
       'prepare_repo.py','car_tool.py','control_check.py','studio_check.py','lan_check.py',
       'http_recovery_check.py','blocked_recovery_check.py','recovery_check.py','hardware_check.py','sensor_check.py',
       'sensor_session.py','matrix_effects_check.py','wifi_setup_check.py','matrix_check.py','wireless_update.py','hotspot_check.py','light_follow_check.py','obstacle_check.py','docs/VALIDATION.md']
selected={Path(name) for name in files}
for directory in ['firmware/CarReady','tests','project-libraries']:
    for path in (root/directory).rglob('*'):
        if path.is_file() and '.git' not in path.parts and '__pycache__' not in path.parts:
            if path.name not in ['wifi_credentials.h','Dashboard.h']:
                selected.add(path.relative_to(root))
for path in (root/'docs'/'images').rglob('*'):
    if path.is_file():selected.add(path.relative_to(root))

# Credentials are compared in memory only; never print their values.
secrets=[]
config=root/'wifi-config.json'
if config.exists():
    settings=json.loads(config.read_text(encoding='utf-8-sig'))
    for key in ['ssid','password']:
        value=settings.get(key)
        if isinstance(value,str) and value:
            secrets.extend([value.encode('utf-8'),json.dumps(value,ensure_ascii=False)[1:-1].encode('utf-8')])
ota_config=root/'ota-config.json'
if ota_config.exists():
    value=json.loads(ota_config.read_text(encoding='utf-8-sig')).get('password')
    if isinstance(value,str) and value:secrets.append(value.encode('utf-8'))
hotspot_config=root/'hotspot-config.json'
if hotspot_config.exists():
    value=json.loads(hotspot_config.read_text(encoding='utf-8-sig')).get('password')
    if isinstance(value,str) and value:secrets.append(value.encode('utf-8'))
payload={path:(root/path).read_bytes() for path in sorted(selected)}
for path,data in payload.items():
    if any(secret in data for secret in secrets):raise SystemExit(f'Export blocked: local Wi-Fi value found in {path}.')

# Preserve unexpected files instead of removing a user's existing export checkout.
expected={str(path) for path in payload}|{'EXPORT-MANIFEST.txt'}
if destination.exists():
    extras=[str(p.relative_to(destination)) for p in destination.rglob('*') if p.is_file() and str(p.relative_to(destination)) not in expected]
    if extras:raise SystemExit('Existing repo-export has additional files. Move it aside before preparing a fresh copy.')
destination.mkdir(exist_ok=True)
manifest=[]
for path,data in payload.items():
    target=destination/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    manifest.append(f'{hashlib.sha256(data).hexdigest()}  {path.as_posix()}')
(destination/'EXPORT-MANIFEST.txt').write_text('\n'.join(manifest)+'\n',encoding='utf-8')
print(f'Prepared {len(payload)} files ({sum(map(len,payload.values()))/1048576:.1f} MiB) in repo-export.')
print('Local Wi-Fi credential scan passed; firmware binaries, backups and hardware logs excluded.')
print('Ready for a new repository later; no GitHub changes were made.')
