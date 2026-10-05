"""Bounded, lifted-car diagnostics. Run only with wheels clear of the surface."""
import json
import time
from pathlib import Path
import serial
from serial.tools import list_ports

ports = [p.device for p in list_ports.comports() if p.vid == 0x2E8A]
if len(ports) != 1:
    raise SystemExit("Connect exactly one Pico via USB.")
results = []
with serial.Serial(ports[0],115200,timeout=2) as device:
    time.sleep(.3)
    device.write(b"\n")
    time.sleep(.1)
    device.reset_input_buffer()
    def send(command):
        device.write((command+"\n").encode())
        device.flush()
        reply=device.readline().decode().strip()
        if not reply:
            raise RuntimeError("No USB reply")
        if command == "STATUS":
            return json.loads(reply)
        if not reply.startswith("OK"):
            raise RuntimeError(reply)
        return reply
    try:
        send("STOP")
        results.append({"baseline":send("STATUS")})
        for wheel in range(1,5):
            for speed in (30,-30):
                print(f"Wheel M{wheel}: {'forward' if speed>0 else 'reverse'} for 350ms",flush=True)
                reply=send(f"TESTMOTOR {wheel} {speed} 350")
                time.sleep(.5)
                state=send("STATUS")
                assert not state["moving"] and not state["armed"]
                results.append({"wheel":wheel,"speed_percent":speed,"duration_ms":350,"reply":reply,"stopped_after":True})
                time.sleep(.6)
        for name,rgb in [("red",(255,0,0)),("green",(0,255,0)),("blue",(0,0,255)),("off",(0,0,0))]:
            print(f"Chassis LEDs: {name}",flush=True)
            send("LED "+" ".join(map(str,rgb)))
            results.append({"led_command":name,"accepted":True})
            time.sleep(1)
        print("Buzzer: short beep",flush=True)
        send("BEEP")
        for angle in (60,90,120,90):
            print(f"Head servo: {angle} degrees",flush=True)
            send(f"SERVO {angle}")
            time.sleep(.75)
        results.append({"servo_commands":[60,90,120,90],"buzzer_command":"accepted"})
    finally:
        send("STOP")
        send("LED 0 0 0")
        send("SERVO 90")
        results.append({"final":send("STATUS")})
        Path("hardware-command-check.json").write_text(json.dumps({"results":results,"physical_observation":"requires owner confirmation"},indent=2))
print("Commands completed; car stopped, LEDs off, head centered. Physical results require observation.")
