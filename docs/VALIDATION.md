# Validation

Checked on **5–6 October 2026**, with a Pico W and Freenove FNK0089 mecanum car. Live wheel checks require the car to be securely lifted.

## CarReady 2.4

The production roaming controller lives in `firmware/CarReady/Pilot.h`. PC tests include that same controller with hardware/time boundaries substituted, rather than reproducing its logic in a separate implementation.

`./test.ps1` passed **37 compile-time policy assertions** and **eight controller scenarios**:

1. All echoes missing: eight turns in the same direction, no forward movement, then stop.
2. Missing echoes followed by a clear front: two inspection turns, then forward travel.
3. Measured dead end: retain the turn direction until a forward exit appears.
4. A known close scan sector vetoes inspection turns.
5. A fresh close echo interrupts an ongoing turn.
6. Recovery disabled: missing echoes keep the car stopped.
7. A clear front advances without unnecessary scanning.
8. Recovery works across the `millis()` wrap boundary.

The complete firmware build and dashboard JavaScript syntax check passed. The clean repository export also built independently using only example Wi-Fi settings; no private header or binary was required from the original workspace. The sketch used **439,088 bytes** of program storage and **75,424 bytes** of global RAM in the tested Arduino-Pico 6.2.0 build.

## Lifted-car recovery check

`python recovery_check.py --lifted` passed on the actual Pico and motor hardware. The diagnostic intentionally suppressed filtered echoes until two inspection turns completed, then restored actual sonar readings. It recorded:

- Two turns in the same direction, with **six moving turn samples**.
- Turn wheel outputs `[25,25,-25,-25]`; no forward movement during echo suppression.
- Forward travel after the real sensor returned clear readings: **nine moving forward samples**, first observed at **10.75 seconds**.
- Automatic stopped/disarmed state at the independent **15-second deadline**, with diagnostic suppression cleared.

This is controlled sensor fault injection with real motor output and subsequent real ranging. It does not prove that a timed turn reaches exactly 180 degrees, that the car escapes every physical dead end, or that the unsensed rear/corners are clear.

## Other checks

The local scripts cover USB stopped-state handling, controller ownership, all ten manual mecanum patterns, release/lease stops, independent roaming deadlines, the lifted-wheel show, sound settings, servo commands and LAN requests. Local JSON reports are excluded from the repository export because they describe one installation rather than portable test fixtures.

Firmware 2.4 passed all 26 live control cases, seven stopped-car studio cases, seven HTTP compatibility cases and USB stopped-state checks. The owner also confirmed the physical two-turn sequence followed by forward wheel movement. All four dashboard tabs fit 320- and 390-pixel phone viewports without horizontal overflow. HTTP stress passed 32 complete pages and 12 aborted requests; an intentional watchdog freeze recovered stopped on USB in 4.56 seconds and on LAN in 17.61 seconds. In one earlier stress attempt an unexpected armed state interrupted the stopped-car check; it was stopped and did not reproduce on the successful repeat. Its cause was not established. The stress tool now stops and saves diagnostic state if it observes an active controller unexpectedly.

## Practical limits

- Forward travel requires fresh, stable ultrasonic evidence. A failed or poorly reflecting sensor can still prevent forward movement after the bounded recovery attempts.
- Known readings below 18 cm block turning. Blind rotation with missing echoes cannot guarantee side/rear clearance; use an open area and supervision.
- Timed pivots have no encoder feedback or measured chassis heading. The radar displays approximate recent echo sectors, not a map or object outline.
- Line/flashlight sensors responded in prior hardware tests; reliable tracking on a floor still depends on calibration and placement.
- Battery percentage is a voltage estimate between 6.7 V and 8.4 V, not measured capacity or predicted runtime.
- A hardware-watchdog reboot begins stopped/disarmed. Reconnecting USB and Wi-Fi takes additional time.
