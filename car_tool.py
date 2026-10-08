"""USB setup tool for CarReady; no dependencies beyond pyserial."""
import argparse
import json
import time
from pathlib import Path
import serial
from serial.tools import list_ports

parser = argparse.ArgumentParser()
parser.add_argument("command", nargs="*", default=["STATUS"])
parser.add_argument("--port")
parser.add_argument("--verify", action="store_true")
parser.add_argument("--samples", type=int, default=0, help="Collect ultrasonic status readings without moving motors")
args = parser.parse_args()
ports = [p.device for p in list_ports.comports() if p.vid == 0x2E8A]
if not args.port and len(ports) != 1:
    raise SystemExit(f"Expected one Pico USB port, found {ports}. Use --port COMx.")
with serial.Serial(args.port or ports[0], 115200, timeout=2, write_timeout=2) as device:
    time.sleep(0.3)
    # Finish a partial line left by port enumeration or a previous session.
    device.write(b"\n")
    time.sleep(0.1)
    device.reset_input_buffer()
    def send(command):
        device.write((command+"\n").encode("ascii"))
        response = device.readline().decode().strip()
        if not response:
            raise RuntimeError(f"No response to {command}")
        print(response)
        return response
    if args.samples:
        if not 1 <= args.samples <= 120:
            raise SystemExit("Use 1 to 120 samples.")
        send("STOP")
        readings = []
        connection_error = None
        try:
            for _ in range(args.samples):
                readings.append(json.loads(send("STATUS")))
                time.sleep(0.25)
        except (serial.SerialException, RuntimeError, json.JSONDecodeError) as error:
            connection_error = str(error)
        valid = [r["distance_cm"] for r in readings if r["distance_cm"] is not None]
        report = {"port":device.port,"readings":readings,"valid_echoes":len(valid),"total":len(readings),"requested":args.samples,"connection_error":connection_error}
        Path("ultrasonic-check.json").write_text(json.dumps(report,indent=2))
        print(f"Received {len(valid)}/{len(readings)} distance readings.")
        if connection_error:
            raise SystemExit(f"Check interrupted; partial readings saved. {connection_error}")
    elif args.verify:
        results = []
        status = json.loads(send("STATUS"))
        assert status["firmware"] in ("CarReady-1.0", "CarReady-1.1-LAN", "CarReady-1.2-Diagnostics", "CarReady-1.3-LAN-Fix", "CarReady-2.0", "CarReady-2.1", "CarReady-2.2", "CarReady-2.6", "CarReady-2.7", "CarReady-2.8", "CarReady-2.9", "CarReady-2.10", "CarReady-2.11")
        assert not status["armed"] and not status["moving"]
        results.append({"check":"boot stopped", "status":status})
        assert send("DRIVE 10 10 10 10") == "ERR disarmed"
        results.append({"check":"unarmed movement rejected", "passed":True})
        assert send("ARM").startswith("OK armed")
        time.sleep(0.7)
        status = json.loads(send("STATUS"))
        assert not status["armed"] and not status["moving"]
        results.append({"check":"500ms idle expiry", "passed":True})
        assert send("ARM").startswith("OK armed")
        assert send("DRIVE 41 0 0 0") == "ERR speed range -40..40"
        status = json.loads(send("STATUS"))
        assert not status["armed"] and not status["moving"]
        results.append({"check":"out-of-range drive rejected and disarmed", "passed":True})
        assert send("SERVO 90") == "OK servo"
        assert send("SERVO 151").startswith("ERR")
        assert send("X"*120).startswith("ERR command too long")
        assert send("STOP").startswith("OK stopped")
        results.append({"check":"servo command, invalid input and stop", "passed":True})
        final = json.loads(send("STATUS"))
        assert not final["moving"] and not final["armed"]
        Path("verification.json").write_text(json.dumps({"port":device.port,"results":results,"final":final},indent=2))
        print("PASS: USB verification completed; no wheel movement commanded.")
    else:
        command = " ".join(args.command) or "STATUS"
        # Prevent leaving an armed session after this one-shot tool exits.
        if command.startswith("DRIVE") or command == "ARM":
            raise SystemExit("Use an interactive serial monitor for ARM/DRIVE with wheels lifted.")
        send(command)
