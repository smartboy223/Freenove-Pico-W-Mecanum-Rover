"""Record passive sensor responses; no wheel movement."""
import json
import time
from pathlib import Path
from urllib.request import urlopen

readings=[]
start=time.monotonic()
print("Recording sensors for 30 seconds. Press remote buttons, illuminate each light sensor and show black/white beneath the line sensors.",flush=True)
while time.monotonic()-start<30:
    with urlopen("http://192.168.0.202/api/status",timeout=3) as response:
        state=json.load(response)
    assert not state["moving"] and not state["armed"]
    readings.append(state)
    time.sleep(.2)
summary={
    "samples":len(readings),
    "light_ranges":[[min(r["light"][i] for r in readings),max(r["light"][i] for r in readings)] for i in range(2)],
    "line_states":[sorted(set(r["line"][i] for r in readings)) for i in range(3)],
    "ir_frames_increase":readings[-1]["ir_count"]-readings[0]["ir_count"],
    "ir_codes":[hex(code) for code in sorted({r["ir_raw"] for r in readings if r["ir_raw"]})],
    "all_stopped":True
}
Path("sensor-response-check.json").write_text(json.dumps({"summary":summary,"readings":readings},indent=2))
print(json.dumps(summary,indent=2))
