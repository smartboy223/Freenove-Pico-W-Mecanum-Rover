# Freenove Pico W Mecanum Rover

CarReady **2.4** turns the Freenove FNK0089 mecanum car into a Wi-Fi rover with a phone dashboard, physical remote control, sensor-assisted driving, RGB effects and sound alerts. The dashboard runs on the Pico W itself; the PC is needed for setup and flashing only.

## Features

- Mecanum manual driving: forward, reverse, crab walks, diagonals and rotation.
- Timed obstacle roaming with a servo-mounted ultrasonic sensor, five-angle scans and heading recovery when echoes are missing.
- Live radar with distance labels, approximate echo sectors and selectable 100/150/300 cm range.
- Black-line and flashlight following, with configurable sensor polarity and light baseline.
- Battery voltage, approximate charge level, sensor readings and individual wheel activity.
- Chassis RGB colors, rainbow/chase/breathing effects, stationary party mode and a lifted-wheel show.
- Buzzer tones, a short melody, approximate loudness adjustment and distinct alerts.
- Stop/disarm, command expiry, an independent roaming deadline and watchdog recovery.
- Automatic detection of either the ultrasonic front module or the LED matrix, without reflashing.

## Hardware

Freenove **FNK0089**, **Raspberry Pi Pico W**, mecanum roller wheels and the kit's battery pack. Wi-Fi needs a compatible **2.4 GHz** network. The supplied IR remote is supported; see [the short remote guide](REMOTE-GUIDE.md).

| Component | Pico GPIO |
|---|---|
| Motor 1: front-left | 18 / 19 |
| Motor 2: rear-left | 21 / 20 |
| Motor 3: front-right | 7 / 6 |
| Motor 4: rear-right | 9 / 8 |
| Servo | 13 |
| Ultrasonic trigger / echo | 4 / 5 |
| LED matrix SDA / SCL | 4 / 5; I2C address 0x71 |
| Chassis RGB LEDs | 16 |
| Buzzer / IR receiver | 2 / 3 |
| Battery ADC | 26 |
| Left / right light sensors | 28 / 27 |
| Left / center / right line sensors | 12 / 11 / 10 |

The ultrasonic module and matrix share the front connector and cannot operate there together. Automatic driving requires the ultrasonic module.

## Setup and flashing

Tested environment: Windows, Python 3.10+, Arduino CLI, Arduino-Pico **6.2.0**, board **rp2040:rp2040:rpipicow**. The required NeoPixel and IRremote libraries are included under `project-libraries` with their original licenses.

1. Install Arduino CLI and the [Arduino-Pico core](https://arduino-pico.readthedocs.io/en/latest/install.html):

   ```powershell
   arduino-cli config add board_manager.additional_urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   arduino-cli core update-index
   arduino-cli core install rp2040:rp2040@6.2.0
   python -m pip install -r requirements.txt
   ```

2. Copy the example and enter your own Wi-Fi name/password in the local file:

   ```powershell
   Copy-Item wifi-config.example.json wifi-config.json
   ```

   `wifi-config.json` is private and ignored by Git. The build generates a private credentials header. Compiled firmware also contains these credentials, so build your own UF2 locally.

3. Build:

   ```powershell
   ./build.ps1
   ```

4. Keep the car lifted and stopped. Find the USB port with `arduino-cli board list`, then upload; replace `COM3` with your port:

   ```powershell
   arduino-cli upload --fqbn rp2040:rp2040:rpipicow --port COM3 --input-dir build
   python car_tool.py STOP
   python car_tool.py --verify
   ```

For a first flash or recovery, disconnect USB, hold **BOOTSEL** while reconnecting, then run `Flash-Car.bat` once the **RPI-RP2** drive appears. The script verifies that drive before copying firmware. Allow time for USB and Wi-Fi to return after flashing.

## Open the dashboard

Run `python car_tool.py STATUS` to find the Pico's current `ip`. On a phone connected to the same main Wi-Fi/LAN, open `http://<car-ip>/`. For example, this installation currently uses `http://192.168.0.202/`. Use **HTTP** and reload after a firmware update.

The Pico serves the dashboard directly on port 80. DHCP may change its address; a router reservation keeps it stable. `http://freenove-car.local/` is also advertised, where the phone/router supports local name resolution. Guest-network or mesh client isolation can block phone access even when the PC can reach the car. This dashboard is intended for the local network.

## Driving

In **Drive**, select a low speed, keep **Front obstacle guard** enabled and arm manual controls. Hold a direction to move; release to stop. Crab arrows move sideways, corners move diagonally and curved arrows rotate. **STOP & DISARM** cancels every mode. The configured forward/backward correction preserves left/right crab and rotation; verify wheel direction while lifted after changing hardware.

The front guard stops forward movement below 25 cm, with uncertain echoes, or with the head off-center. Sideways, reverse and rotation are not covered by the front sensor.

### Obstacle roaming

In **Roam**, select 5–600 seconds and press **Start timed obstacle roaming**.

1. Check the centered sensor; advance when reliable front clearance is at least 35 cm.
2. Keep watching the front while moving. Stop to scan 30°, 60°, 90°, 120° and 150° at an obstacle, uncertain echo or periodic route check.
3. Prefer an open forward route or turn toward a measured clear side.
4. If no usable route echo exists and **Turn out of dead ends & missing echoes** is enabled, turn in short steps to inspect other headings. Keep the same direction rather than oscillating. Stop and check fresh front readings after each step; repeat the wide scan after four unsuccessful inspection turns.
5. Resume forward travel only after a reliable clear front reading. Stop/disarm after eight unsuccessful turns or at the selected timer deadline.

Inspection turns last 450 ms at up to 25%; measured-exit turns last 500 ms and measured dead-end pivots 350 ms. A known echo below 18 cm blocks a turn, and a newly detected close echo interrupts one already in progress. Missing echoes can mean open space, an unsuitable surface or a failed sensor; they never authorize forward travel. Turn angles are timed estimates, so the car may inspect the opposite side but cannot guarantee a precise 180° turn. The rear/corners remain unsensed. Use an open, level area away from edges and stairs.

**Continue until the timer ends** runs the selected timer on the Pico without keeping the page open. Wi-Fi loss, low battery, Stop and watchdog expiry still stop the car. A hardware timer enforces the overall deadline independently of HTTP requests.

Radar points represent recent sensor measurements, not a room map or object shape. They fade after six seconds and clear after a chassis turn.

### Line and flashlight following

- **Line:** place a continuous black strip beneath the three underside sensors on a light surface. This installation's center-only black fixture reads `[1,0,1]`, so **black reads 0** is the default. Change polarity in **Sensors** if your hardware differs. Centered patterns drive all four wheels straight; side patterns apply gentle corrections. All-black stops; a white gap is crossed for at most 500 ms before stopping.
- **Flashlight:** set the ambient baseline in **Sensors** with the flashlight off, then aim it at the front photoresistors. Adjust sensitivity for your room. No bright target means stopped.

Keep the dashboard visible for line/light modes. Real floor tracking and stopping distance depend on surface, battery, speed and sensor height.

## Lights, sound and front-module swaps

**Lights & sound** offers RGB colors, effects, brightness, party mode, test tones from 400–3000 Hz and a six-note chime. Loudness controls GPIO duty cycle; it is approximate rather than amplifier volume. Zero mutes all sound. Movement alerts can be toggled separately. Stationary party and melody commands stop driving first.

The ten-second crab/spin demonstration requires acknowledgement that all four wheels are lifted. It finishes stopped and disarmed.

To swap the front module: switch the car off, unplug USB, fit the ultrasonic sensor or matrix in the vendor orientation, then restore power. A response at I2C address 0x71 selects matrix mode; otherwise ultrasonic mode is selected. Check the dashboard or `Check-Car.bat`. An absent module can also select ultrasonic mode, so selection alone does not prove successful ranging.

## Checks and diagnostics

| Command | Purpose |
|---|---|
| `python car_tool.py STATUS` | USB sensor/controller status |
| `python car_tool.py STOP` | Stop/disarm |
| `python car_tool.py --verify` | USB checks without wheel motion |
| `./test.ps1` | Actual roaming state machine and policy tests on the PC |
| `python control_check.py --host <car-ip> --lifted` | Live LAN controls, wheel patterns, timed roaming and show |
| `python studio_check.py` | Stopped-car sound/head checks |
| `python lan_check.py` | HTTP compatibility checks |
| `python http_recovery_check.py` | Stopped-car HTTP stress and intentional watchdog restart |
| `python recovery_check.py --lifted` | Live fault-injection test: two turns with echoes suppressed, then restore the real sensor |

Live tests write local JSON evidence, excluded from the repository. Scripts other than `control_check.py` and `recovery_check.py` currently use this installation's default address/COM3; change their HOST/BASE/port for another setup. The native C++ tests use g++ or Visual Studio C++ Build Tools. See [validation notes](docs/VALIDATION.md) for current results and limits.

The USB recovery diagnostic requires the exact command `TESTNOECHO LIFTED`. It intentionally suppresses filtered echoes until two turns complete, then restores real sonar readings; an independent 15-second deadline ends the test. Stop, reset and watchdog recovery clear the diagnostic. It is a lifted-wheel test, not a floor-navigation certification.

## Project layout

```text
firmware/CarReady/       Firmware, motion/sensing policies, roaming controller and dashboard
project-libraries/      Required third-party libraries and licenses
tests/                 PC tests of the production controller and policies
build.ps1               Generate dashboard/credentials and compile
prepare_wifi.py         Read local Wi-Fi settings without logging credentials
prepare_dashboard.py    Embed the dashboard HTML in firmware
car_tool.py             USB setup/status/Stop commands
recovery_check.py        Lifted-car heading-recovery test
prepare_repo.py         Prepare a clean repository copy without local secrets/artifacts
REMOTE-GUIDE.md          Short physical-remote guide
wifi-config.example.json Example settings only
```

## Prepare a new repository

Run `python prepare_repo.py` to create **repo-export/** with firmware source, dashboard, required libraries/licenses, scripts, tests and documentation. It excludes Wi-Fi credentials, compiled firmware, private backups, downloaded vendor archives and local hardware reports. The original workspace remains intact. The script checks the exported files for the actual local Wi-Fi credentials without displaying them.

The export is ready for `git init` and a new GitHub repository when you choose its name and visibility. No repository is created or pushed by the preparation script. If working directly in this workspace, the supplied `.gitignore` also excludes the private/generated files. Keep the original licenses with vendored code; see [third-party notices](THIRD-PARTY.md).

## References

- [Freenove FNK0089 mecanum assembly guide](https://docs.freenove.com/projects/fnk0089/en/latest/fnk0089/codes/Mecanum/1_Assembling_Smart_Car.html)
- [Official Freenove Pico kit resources](https://github.com/Freenove/Freenove_4WD_Car_Kit_for_Raspberry_Pi_Pico)
- [Arduino-Pico installation and USB recovery](https://arduino-pico.readthedocs.io/en/latest/install.html)
