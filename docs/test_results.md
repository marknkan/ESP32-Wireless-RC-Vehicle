Version 1 test results

The results below record the completed indoor acceptance test. The reported
range is an approximate tested operating distance, not the maximum possible
ESP-NOW range.

Test

Expected result

Observed result

Status

Forward

Both wheels drive the vehicle forward

Vehicle moved forward correctly

Pass

Backward

Both wheels reverse the vehicle

Vehicle moved backward correctly

Pass

Left

Vehicle turns left

Vehicle turned left correctly

Pass

Right

Vehicle turns right

Vehicle turned right correctly

Pass

Neutral stop

Releasing the joystick stops movement

Vehicle stopped correctly

Pass

Push-button stop

Pressing the joystick stops movement

Vehicle stopped correctly

Pass

Communication failsafe

Loss of sender commands stops the vehicle

Vehicle stopped after sender shutdown

Pass

Indoor range

Reliable control across the living room

Reliable at approximately 4 m indoors

Pass

Evidence

A demonstration video was recorded showing the working vehicle and controller.
Add the final compressed video to assets/demo/ or link to it from the README.

Test limitations

The 4 m figure is an approximate indoor distance.

Maximum outdoor line-of-sight range was not measured.

A formal battery-runtime test was not recorded for Version 1.

Open-loop motors were manually calibrated; wheel speed is not measured by
encoders in Version 1.
