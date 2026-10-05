# Physical remote — CarReady-2.6

The remote is enabled. Aim it at the car's IR receiver. Line following uses the dashboard's polarity setting; this car is calibrated to black reads 0.

| Button | Action |
|---|---|
| + / − | Forward / reverse |
| ⏮ / ⏭ | Rotate left / right |
| ▶ or 9 | Stop and disarm, including dashboard modes |
| TEST | Short tone using the dashboard's pitch/loudness setting |
| 0 / 1 | Head angle +10° / −10°; stops driving first |
| 4 | Center head; stops driving first |
| 7 | Cycle red → green → blue chassis LEDs |
| 8 | Chassis LEDs off |
| C | Follow light using the saved ambient baseline |
| 3 | Follow a black line |
| 6 | Obstacle autopilot |
| Power / MENU / return / 2 / 5 | Unassigned |

Manual movement expires after 350 ms without repeat frames. Automatic remote modes expire after 3 seconds without renewal; holding the matching button can renew them. Forward movement needs reliable front ultrasonic echoes at least 35 cm away. Sideways and diagonal movement are available on the dashboard. The owner-confirmed forward/backward reversal is corrected in this version; turning stays unchanged. The dashboard can start longer, independently timed roaming. A remote 6 press starts a scan; hold it to renew the short remote lease long enough to complete the scan. Calibrate the ambient light baseline on the dashboard before remote C light following.

While the dashboard owns an armed car, the remote can stop it with ▶ or 9; other remote commands wait until it is disarmed. The dashboard cannot take over an active remote session without Stop first. TEST and + raw codes were physically verified; full button operation and floor behavior still need individual checks.

Dashboard: **http://192.168.0.202/**. Power is not an emergency-stop button in this firmware. Use ▶, 9, or the large dashboard Stop button.

The dashboard has a separate 5–600 second flashlight timer in Sensors. The remote C button continues to use its short repeat-frame lease; it does not start the dashboard timer.
