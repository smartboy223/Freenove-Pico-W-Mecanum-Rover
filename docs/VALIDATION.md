# Validation

Checked on **5–8 October 2026**, with a Pico W and Freenove FNK0089 mecanum car. Live wheel checks require the car to be securely lifted.

## CarReady 2.11 — open development hotspot and dashboard home-Wi-Fi setup

### Network discovery, durable save/delete and interactive matrix head follow-up

The connection panel now separates the saved home network from the actual radio-associated network, reports signal strength and supplies a clickable current dashboard URL and remembered home URL. An authenticated asynchronous scan lists up to 20 unique visible networks in signal order; hidden names remain available through manual entry. Network text is JSON-escaped in firmware and inserted as text in the browser. Passwords remain absent from status and scan responses.

Successful save, failed-join outcome, deletion and the last home IP are retained in LittleFS. Forget writes a persistent tombstone that disables compiled credentials as well as removing saved credentials. A later successful save clears it. Network switches fully disassociate the radio before the controlled CPU restart. Connection confirmation then checks the radio SSID and DHCP address and requires a stable association before saving a candidate. Candidate confirmation is suspended throughout the 300 ms queued-switch response window, so the still-connected previous network cannot mark an unavailable new SSID as saved. Scanning blocks dashboard, USB and remote movement.

The matrix-mounted servo follows actual wheel output for crab/turn/diagonal movement, stays centered for straight travel and uses bounded stationary party/effect poses. Motion is smoothed to three degrees per 40 ms. Dashboard controls toggle automatic head motion and reverse its mounting direction. Explicit head commands hold for five seconds; Stop ends effects and centers the automatic head. Ultrasonic scanning is unchanged.

The native checks passed **72 compile-time assertions** and **30 runtime groups**, including production head mapping and bounded party poses. The mobile fixture passed **16 groups**, including discovered secure/open networks, saved/actual network/URL presentation, durable deletion display, rejected-input feedback and interactive-head settings. Its public screenshot uses demonstration network names. Firmware built with **530,236 bytes** of program storage and **76,716 bytes** of global RAM.

`network_settings_check.py --allow-forget --test-update` passed **10 live stopped-car groups**: saved/actual connection and URL, home scanning, rejected unauthorized deletion, unavailable-SSID rollback, hotspot scanning with its dashboard still active, persistent forgetting of saved and compiled defaults, deletion surviving watchdog restart, deletion surviving an authenticated wireless firmware update, a successful new save with its LAN URL, and that save/result surviving another restart. The original real home network was restored, the temporary Windows profile was removed, and the PC Wi-Fi adapter returned to disconnected. Final car state is home Wi-Fi at **192.168.0.202**, stopped/disarmed. Local evidence is ignored `network-settings-check.json`.

Three live stationary matrix-head groups passed: party produced at least eight commanded servo angles within 70–110 degrees and changing matrix/RGB/buzzer feedback, with every sampled wheel output zero; disabling automatic head motion held a manually selected angle during party; and a manual head command held for five seconds before returning smoothly to 90 degrees. This verifies firmware commands and telemetry, not measured physical angles from a servo encoder. Directional head behavior uses actual wheel mixing in the native tests; no live wheel movement was commanded. Local evidence is ignored `matrix-head-check.json`.




### Home-first restart and Windows launcher follow-up

The 8 October follow-up adds `start.bat` and `launch_dashboard.py`. The launcher only reads USB/HTTP status, remembers a working local address in an ignored JSON file, and opens the dashboard; it does not arm, flash or command movement. Both the project launcher and the parent-folder launcher resolved the live home dashboard in checks with browser opening disabled.

Ordinary restarts now prefer saved home Wi-Fi. The explicit hotspot marker is consumed once at boot; automatic fallback no longer writes a permanent hotspot preference. Pending home pairing takes priority over a stale marker. OTA preserves its active network for its completion check, then subsequent ordinary restarts return to the home-first policy. This supersedes the persistent hotspot choice described in the original 2.11 checks below.

The updated native suite passed **72 compile-time assertions** and **28 runtime groups**. Firmware built with **515,716 bytes** of program storage and **76,612 bytes** of global RAM, and the stopped wireless transfer and reboot check passed.

Five live stopped-car checks passed: explicit hotspot selection; a hardware-watchdog restart from hotspot returning to saved home Wi-Fi; the home dashboard responding after that restart; the unavailable-network diagnostic falling back to the open hotspot after 30 seconds; and a second restart from fallback returning to the saved home network without re-entering credentials. USB status verified saved settings, a listening dashboard and zero wheel output throughout. Final state is home Wi-Fi at **192.168.0.202**, stopped/disarmed. Evidence is recorded locally in ignored `boot-start-check.json`; this was a firmware reset test, not a physical battery disconnect.

Built and installed on **8 October 2026**, with Arduino-Pico **6.2.0**. The final firmware uses **515,692 bytes** of program storage and **76,612 bytes** of global RAM. Network changes stop/disarm the car and use a controlled restart, clearing old TCP contexts before the new network starts. The hotspot choice and successful home credentials are stored in LittleFS. Failed candidate credentials do not replace the saved home network.

The native suite passed **68 compile-time assertions** and **28 runtime groups**. The new production Wi-Fi parser checks open/WPA keys, length bounds, UTF-8 and URL-form special characters, while rejecting malformed encodings, embedded NUL bytes and invalid keys. The **13 browser groups** include a phone-sized home-Wi-Fi form, private POST submission with special characters, password clearing after acceptance, explicit open-network selection and rejected-input feedback. Existing touch-drive, matrix, party and network controls still pass.

The live Windows check `python wifi_setup_check.py --leave-hotspot` passed **nine integration groups**, with stopped motors throughout: unauthenticated/malformed/duplicate/oversized forms; open association; local DNS; Android, Apple and Windows probe redirects; five full dashboard downloads; a fragmented POST body; failed-join rollback; successful home pairing through the actual mobile dashboard; authenticated hotspot OTA with saved settings surviving reboot; and Retry saved home Wi-Fi. Connection changes intentionally restart USB, and the checker tolerates that brief disappearance. The 30-second home association/fallback window is retained to allow the mesh to connect reliably; opening a chosen hotspot is immediate rather than waiting for that window.

On the PC Wi-Fi adapter, five full dashboard downloads had a **0.070-second median** and **1.492-second slowest** time. Failed joining returned to the hotspot and the PC re-associated in **38.61 seconds**, including the 30-second association window. The car was left hosting the open hotspot at **192.168.4.1**, with a live HTTP listener, saved home credentials, a closed update window and zero wheel output; the PC temporary profile was removed and its Wi-Fi adapter restored to disconnected. `docs/images/dashboard-wifi-2.11.png` is a live screenshot after successful home pairing.

The owner physically joined Freenove-Rover on their phone without a password and confirmed the dashboard opened from the **Sign in to network** prompt. Captive-popup behavior on other phones depends on the operating system. The probe routes and direct dashboard address are tested from the PC's Wi-Fi adapter; a physical phone can always use http://192.168.4.1/ after joining the car's network. Home and hotspot remain alternative modes. No wheel movement is commanded by these networking checks.

## Earlier CarReady 2.10 — coordinated matrix, RGB and sound

Built and installed over home Wi-Fi on **8 October 2026**, using Arduino-Pico **6.2.0**. The final build uses **503,028 bytes** of program storage and **76,416 bytes** of global RAM; the authenticated transfer was **521,424 bytes**. The update gate rejected arming, and reboot returned to stopped/disarmed with a closed maintenance window.

The native suite passed **68 compile-time assertions** and **27 runtime groups**. Three new groups exercise the production `MatrixEffects.h`: all eight shared party beats, note/face mapping, brightness pulse and clock wrap; alert interruption/resumption and explicit Off; melody/tone overlays and selected-face restoration; and every RGB style, bounded breathing brightness, running animation and movement-sign priority. The existing pixel-orientation checks still cover all 128 pixels at all four alignments. Alerts were checked in the native policy tests; a physical ultrasonic obstacle alert cannot be generated with the matrix occupying that connector.

`python matrix_effects_check.py` passed **12 stopped-car integration groups** on the actual Pico W:

- Nine emotion selections produced artwork, a matching RGB flash and a short tone.
- Rapid emotion-to-sign or Off changes cancelled the previous feedback immediately, preserving the newly selected artwork.
- All eleven RGB styles reached Interactive; running lights changed display frames and Off cleared both outputs.
- Breathing varied matrix brightness through all four chosen levels and varied the actual RGB output.
- Stationary party changed physical matrix frames, chassis RGB output and at least five buzzer pitches. Its matrix face followed the shared 400 ms beat, and every sampled wheel output remained zero.
- Stop ended party, silenced sound, cleared chassis LEDs and restored the chosen Heart expression.
- Silent party animated without sound; volume zero also muted musical party without freezing its visuals.
- Explicit Matrix Off remained blank during party.
- The six-note chime animated sound bars and note colors, then returned to idle.
- A test tone briefly activated the matrix and RGB feedback, then cleared.
- More than 50 new physical I2C display writes were acknowledged, with **zero new write failures**.

The existing **seven studio checks** and **seven LAN compatibility cases** also passed on the final firmware. No wheel movement was commanded in this session. The mobile browser fixture passed **12 groups**, including the stationary-party/gallery shortcut and live sound labels, plus all earlier hold/release, alignment and network controls. A separate check against the actual Pico dashboard confirmed the gallery Party shortcut started all three effects with motors disarmed, and the studio fitted 320/390 px and desktop widths without script errors. `docs/images/dashboard-party-2.10.png` comes from that live dashboard.

The I2C acknowledgements and output telemetry verify firmware activity. During the stopped demonstration, the owner also confirmed that changing matrix faces, pulsing chassis lights and changing notes worked together on the physical car. The previously confirmed 90-degree-left tile correction is retained. Party and melody temporarily animate a manually selected face; Stop returns that selection. Explicit Off takes priority. Gallery Party starts the full stationary show; the low-level matrix API's `face=party` alone remains a display-only selection.

## Earlier CarReady 2.9 — protected hotspot fallback and direct wireless updates

Built and installed on **8 October 2026** using Arduino-Pico **6.2.0**: **500,260 bytes** of program storage and **76,376 bytes** of global RAM. The 1 MiB sketch / 1 MiB filesystem layout is unchanged. Initial installation used the existing home-LAN update path, and the firmware was subsequently transferred again through the car's own hotspot.

The native suite passed **68 compile-time assertions** and the existing **24 runtime groups**. The six additional assertions use the production `NetworkPolicy.h` to verify the 30-second association window, the fallback deadline, connected-home/hotspot stability and clock wrapping. The mobile Chromium fixture passed **11 browser groups**, including both connection controls and their network labels, plus the prior driving/expression/alignment checks.

`python hotspot_check.py` passed **six stopped-car integration groups** on the actual Pico:

- The home dashboard served 2.9 and rejected an invalid network choice.
- The USB-only unavailable-network diagnostic produced a connecting state, then automatic hotspot fallback at **30.50 seconds**, with zero wheel outputs throughout.
- The PC joined the **WPA2-protected** car network and received its dashboard/status at **192.168.4.1**, with one associated client and home Wi-Fi disconnected.
- Expression commands worked through the hotspot without starting movement.
- A real, authenticated **518,640-byte firmware transfer** through the hotspot succeeded; reboot retained hotspot mode, a closed maintenance window and a stopped/disarmed state.
- The dashboard's production Retry home Wi-Fi request restored the home-LAN address **192.168.0.202** while stopped. The PC's temporary Wi-Fi profile/password XML was removed and its adapter restored to disconnected.

Seven HTTP compatibility cases also passed after recovery to the home LAN, including delayed headers, large headers and four simultaneous clients. The live desktop/mobile dashboard rendered and selected expressions without script errors. The car is currently returned to home mode, stopped/disarmed, and its matrix alignment remains 90° left. No new wheel movement was commanded in this networking session.

Hotspot credentials are generated once in ignored `hotspot-config.json`, separate from the firmware-update password. The build/export check compares all three private credentials against public files. Automatic and manual hotspot selection persist in LittleFS until Retry home Wi-Fi is chosen; this keeps the direct update address stable through reboots. Station and hotspot are alternative modes in this firmware. Physical phone joining was not tested here; the direct connection and wireless update were verified from the PC's Wi-Fi adapter.

## Earlier CarReady 2.8 — matrix expressions, mobile driving and wireless updates

Built and installed on **8 October 2026** using Arduino-Pico **6.2.0**. The final matrix-alignment build uses **496,652 bytes** of program storage and **76,360 bytes** of global RAM. The build uses the Pico W target with a 1 MiB sketch / 1 MiB filesystem partition. USB enumerated on COM3, and the car rejoined its LAN at `192.168.0.202`, stopped/disarmed with the matrix fitted.

The native tests passed **62 compile-time assertions** and **24 runtime groups**: twelve production roaming cases, eight production light-following cases, and four matrix groups. Matrix checks cover all ten actual mecanum patterns, every expression name, distinct emotions, blinking eyes, animated party frames and clearing the display. Every one of the 128 pixels is checked under all four panel rotations for unique placement, no clipping, preserved panel order, inverse mapping and agreement with the vendor's `setRow` buffer mapping.

`python tests/mobile_dashboard_test.py` passed **ten browser groups** against a local fixture, without contacting the car: long touch without text selection; sliding between forward/crab and neutral/outside stops; touch cancellation; release ordered after a delayed drive request; panel-switch release; expression/brightness commands; all four alignment controls; blur stop/disarm; keyboard hold/release; and drive/studio layouts at 320, 390 and 1180 pixels with no script errors. Safari callout suppression is present in CSS; these automated tests ran in Chromium, not on a physical iPhone.

### Live hardware and LAN checks

The final 2.8 matrix check passed **18 groups** on the powered, lifted car: module detection; all **22 matrix selections**; brightness endpoints and invalid-input preservation; all four transmitted I2C alignment buffers and invalid-angle rejection; forward with either requested guard setting; all **ten wheel patterns with matching automatic signs and release stops**; command expiry; and rejection of guarded automatic modes without ultrasonic sensing. The owner reported split/clipped expressions in the initial build and requested a 90° left rotation, superseding their earlier visual confirmation. The installed correction rotates each panel separately and defaults to 270° (90° left). The owner then confirmed Happy and Heart appear **upright and complete across both halves**.

The USB stopped-state/invalid-command checks, **seven stopped-car studio checks**, and **seven HTTP compatibility checks** passed. Studio checks include variable tone/loudness, mute, melody completion, stationary party cancellation, servo endpoints and the matrix automatic-mode gate. The live dashboard rendered without script errors on desktop and a 390-pixel mobile viewport; expression selection was confirmed over LAN. The two 2.8 dashboard screenshots in `docs/images` come from the actual Pico, not the fixture.

### Real wireless firmware update

After the initial USB installation, `python wireless_update.py` authenticated and transferred both the expression-handler correction and the final **515,032-byte alignment `.bin`** over Wi-Fi. The maintenance window rejected arming with HTTP 409. Each upload confirmed a new boot uptime, a closed update window, zero wheel outputs, and a stopped/disarmed state. Windows still detected COM3 afterwards. Matrix and LAN checks passed on the final build; sound/head checks passed on the preceding 2.8 build, whose sound/head code was unchanged by the alignment correction.

Wi-Fi and update passwords remain in ignored local configuration/header files; compiled firmware and hardware reports are excluded from the public export. There was no Git commit or push. Physical floor movement, iPhone behavior, and an ultrasonic-module swap were not retested in this matrix session; earlier ranging/roaming evidence is recorded below.

## Earlier CarReady 2.7 preparation — matrix manual-driving fix

Prepared and built on 8 October 2026: **447,344 bytes** of program storage and **75,544 bytes** of global RAM. This version was superseded by 2.8 before installation; no live 2.7 result is claimed.

The actual car reported `module: matrix` on firmware 2.6. A brief lifted-wheel check reproduced the fault: requesting the ultrasonic front guard produced zero wheel output and `Forward guard needs the ultrasonic module`; disabling it produced forward outputs `[-20,-20,-20,-20]`. Every case ended stopped/disarmed.

Version 2.7 selects the manual guard from the fitted module at boot, at dashboard arming, and at remote/manual mode entry. Matrix manual driving has no distance sensing. With the ultrasonic module fitted, normal forward guarding remains enabled by default; missing echoes do not disable it. Guarded automatic modes still reject the matrix.

The dashboard disables and labels the unavailable guard in matrix mode, explains the missing distance sensor, and disables guarded automatic-mode buttons. Returning to the ultrasonic module restores the default guard. The production `moduleControls` function passed isolated DOM checks for both modules, returning to ultrasonic, light calibration and preservation of a user's guard choice across repeated status polls. This is not a visual browser test.

The existing **62 compile-time assertions** and **20 controller scenarios** passed again, and Python/dashboard JavaScript syntax checks passed. The matrix regression script was prepared here and subsequently extended and run against installed 2.8, as recorded above.

## Earlier CarReady 2.6 checks

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
