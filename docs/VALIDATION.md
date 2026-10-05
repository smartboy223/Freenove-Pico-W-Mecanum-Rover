# Validation

Checked on **5–6 October 2026**, with a Pico W and Freenove FNK0089 mecanum car. Live wheel checks require the car to be securely lifted.

## CarReady 2.6 — current firmware

Built and installed on 6 October 2026: **446,224 bytes** of program storage and **75,544 bytes** of global RAM. The dashboard JavaScript syntax check passed. `./test.ps1` passed **62 compile-time assertions** and **20 controller scenarios**: the existing twelve roaming scenarios plus eight cases using production `Light.h`.

Light checks cover two distinct sensor samples before acquisition, left/right pivots, balanced forward travel, loss of light, unequal room baselines, close/missing front echoes, stale telemetry, target hysteresis and a stale bright baseline. Brightness steering and motor output are capped at 25%; a lower selected speed remains effective. Forward movement retains the 35 cm guard, and pivots require fresh centered front clearance of at least 28 cm with no first close raw echo below 18 cm.

The dashboard now has separate flashlight duration (5–600 seconds), speed (15–25%) and independent-run controls. The shared hardware deadline applies to both Roam and Light. Target loss pauses the wheels while the selected timer continues; the deadline stops and disarms. The action message reports waiting, steering and obstacle refusal. Physical remote C still uses its existing short repeat-frame lease.

### Real flashlight evidence

A five-second independent run with the flashlight off recorded **33 stopped samples**, received no client heartbeat, and finished with `Flashlight timer finished`, zero wheel outputs and a disarmed state.

A 32-second real-light recording produced **57 left-pivot samples**, **51 right-pivot samples** and **53 straight-forward samples**. It also finished stopped/disarmed at the requested deadline without heartbeats. No dark interval occurred in that recording, so the four-phase script correctly flagged its missing dark phase; target-loss validation is recorded separately.

The owner confirmed that the wheels physically moved during that recording. A subsequent eight-second independent run with the flashlight off passed with **52 stopped samples**, no moving samples and no heartbeats, then stopped/disarmed at the deadline. A stopped-car RGB check showed only small ambient changes between LEDs off and green (less than one ADC unit on each sensor on average).

### Other current live checks

Firmware 2.6 passed **28 LAN control checks**, **seven stopped-car studio checks**, **seven HTTP compatibility checks**, and the USB stopped-state/invalid-command checks. This includes all ten manual wheel patterns, unfinished-request expiry, independent roaming deadlines and the ten-second lifted-wheel show. The new light timer rejects durations outside 5–600 seconds and invalid independent-run options.

The actual dashboard Start button also passed a five-second flashlight-off run: it showed `Independent run · 5 seconds left`, waited without wheel movement, and finished with `Flashlight timer finished` and a disarmed state. The flashlight panel fits **320- and 390-pixel phone widths** without horizontal overflow. The current screenshots show the live 2.6 firmware; the light controls are saved in `docs/images/dashboard-flashlight-2.6.jpg`.

## Earlier CarReady 2.5 checks

Built and installed on 6 October 2026. The complete firmware build and dashboard JavaScript syntax check passed: **443,096 bytes** of program storage and **75,520 bytes** of global RAM on Arduino-Pico 6.2.0.

`./test.ps1` passed **53 compile-time policy assertions** and **12 scenarios using the production `Pilot.h` controller**. Alongside the earlier eight recovery scenarios, the new cases cover isolated echo dropouts, isolated short spikes, a newly blocked route after forward travel, and disabled backtracking. The policy checks cover two-reading obstacle confirmation, supported-cluster filtering, stopping/restart margins, warning cooldowns and retreat history/age/attempt limits.

The current movement policy brakes roaming at 45 cm, resumes with 55 cm clearance, and caps cruising at 20% below 70 cm or 25% farther away. Manual/line/light forward guarding stops below 35 cm. A suspicious first short echo pauses movement immediately; two consistent short readings confirm an obstacle warning. Persistent missing readings delay their warning; separate warning cooldowns prevent repeated alternating alerts. Resolved range alerts clear when verified forward movement resumes.

### Other earlier live checks

Firmware 2.5 passed **27 LAN control checks**, **seven stopped-car studio checks**, **seven HTTP compatibility checks**, and the USB stopped-state/invalid-command checks. The controls include all ten manual wheel patterns, command expiry during an unfinished HTTP request, retreat setting bounds, the independent roaming deadline, and all eight show patterns. The show check accepts the normal deadline race only when a fresh status confirms `Show finished` and all wheels are stopped.

A real flat object, confirmed by the owner as less than 10 cm away, produced reliable readings of **6.9–8.9 cm**. All **five guarded forward requests were blocked**, with every wheel output zero. This confirms a responsive near target and the stop gate, not full sensor calibration or floor stopping distance.

All four dashboard panels fit **320- and 390-pixel phone widths** without horizontal overflow. The saved dashboard image shows the actual 2.5 firmware and new retreat option. Scan-sector cards now use recent radar measurements instead of retaining old navigation scan values; the centered card follows the current front reading. No current physical-floor dead-end or stopping-distance test was performed.

### Live lifted-wheel recovery

Both diagnostic scripts passed on the actual Pico and motor hardware, using deliberate filtered-sensor fault injection followed by real sonar readings:

- **Missing-echo recovery:** two turns in the same direction, six moving turn samples, no forward travel during suppression. Actual ranging restored, then 37 forward samples, first at **9.515 seconds**. The independent 15-second deadline stopped/disarmed and cleared suppression.
- **Blocked-route recovery:** six initial real forward samples produced travel history. A simulated 12 cm wall triggered a short retreat (two sampled reverse outputs, capped at 18%), then a fresh scan. Restored actual sonar produced 72 forward samples, first at **7.359 seconds**. All resumed-forward samples had resolved range alerts cleared. The independent 18-second deadline stopped/disarmed and cleared the diagnostic.

A retreat requires recent forward history on the same heading, no older than eight seconds. Each reverse step is capped at 350 ms, with at most two attempts. Turning clears the travel history, so it cannot justify reversing on a different heading. Both scripts now check for at least 55 cm reliable front clearance before starting.

These tests prove the controller transitions and motor outputs while lifted. They do not prove physical floor escape, exact turn angles, rear clearance or collision-free navigation. A genuinely close corner with no usable recent path correctly remains stopped.

## Earlier CarReady 2.4 checks

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
- Known scan-sector readings below 18 cm or centered readings below 28 cm block turning. Recent-path retreats are limited and do not measure the rear. Blind rotation with missing echoes cannot guarantee side/rear clearance; use an open area and supervision.
- Timed pivots have no encoder feedback or measured chassis heading. The radar displays approximate recent echo sectors, not a map or object outline.
- Line sensors responded in prior hardware tests. Flashlight steering, balanced forward output, light-off waiting and timer expiry were verified on lifted wheels in 2.6; reliable floor tracking still depends on calibration and placement.
- Battery percentage is a voltage estimate between 6.7 V and 8.4 V, not measured capacity or predicted runtime.
- A hardware-watchdog reboot begins stopped/disarmed. Reconnecting USB and Wi-Fi takes additional time.
