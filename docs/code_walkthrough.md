# Code walkthrough

## System overview

Two ESP32 boards communicate directly using ESP-NOW. The handheld controller
reads a two-axis analogue joystick, converts its readings into a movement
command and transmits a `ControlPacket`. The vehicle receives the packet and
controls two TT motors through a TB6612FNG dual H-bridge.

## Shared data definitions

The sender and receiver contain matching versions of `VehicleCommand` and
`ControlPacket`.

`VehicleCommand` defines Forward, Backward, Left, Right, Stop and Invalid.

`ControlPacket` contains:

- `command`: the requested vehicle movement;
- `speed`: a PWM value from 0 to 255;
- `sequence`: an incrementing number used to monitor packet transmission.

The order and data types in this structure must remain identical in both
sketches so that the receiver interprets the transmitted bytes correctly.

## Controller sender

### Pin and calibration constants

GPIO34 and GPIO35 read the joystick's analogue X and Y outputs. GPIO32 reads
the active-low Select switch using `INPUT_PULLUP`, meaning a pressed switch
produces `LOW` and a released switch produces `HIGH`.

The ESP32 uses 12-bit analogue readings between 0 and 4095. Threshold constants
were selected from measured joystick readings. The perpendicular axis must also
remain within a central band before a cardinal direction is accepted.

### `joystickRead()`

This function reads the Select switch followed by both analogue axes. It prints
the readings to the Serial Monitor, which was useful during calibration and
debugging.

### `determineDirection()`

The function compares the readings with the calibrated thresholds:

- high Y with central X produces Forward;
- low Y with central X produces Backward;
- low X with central Y produces Left;
- high X with central Y produces Right.

Movement is accepted only while the Select switch is released. A centred,
diagonal or unrecognised position falls through to Stop. Pressing the joystick
also produces Stop.

### `convertDirection()`

This switch statement converts the controller-specific `joyStick` value into
the shared `VehicleCommand` value sent to the vehicle.

### `prepareControlPacket()`

This function stores the selected command and speed in `outgoingPacket`, then
increments the sequence number. Stop and Invalid commands use speed 0;
movement commands use speed 130.

### ESP-NOW setup

The controller starts Wi-Fi in station mode and sets channel 1. It initialises
ESP-NOW, copies the vehicle ESP32's station MAC address into `peerInfo`, and
registers the vehicle as an unencrypted peer. The MAC address identifies the
specific receiver on the local ESP-NOW link; it is not a password.

The `onDataSent()` callback reports whether ESP-NOW delivery succeeded or
failed.

### Main loop

Every 50 ms, the sender:

1. reads the joystick;
2. determines a direction;
3. converts it into a vehicle command;
4. selects speed 0 or 130;
5. prepares the packet;
6. transmits it to the vehicle MAC address.

## Vehicle receiver

### Motor-driver pins and calibration

The receiver defines seven GPIO connections for the TB6612FNG driver. Motor A
and Motor B each have two direction inputs and one PWM input, while `STBY`
enables the driver.

Forward and backward use independently calibrated speeds:

| Movement | Left motor PWM | Right motor PWM |
| --- | ---: | ---: |
| Forward | 228 | 130 |
| Backward | 219 | 130 |

These unequal values compensate for differences between the two motors in this
open-loop system.

### Motor functions

`motorAClockwise()`, `motorACounterClockwise()`, `motorBClockwise()` and
`motorBCounterClockwise()` set the relevant direction pins and PWM values.
The PWM output is first set to zero before changing direction.

`brakeMotorA()` and `brakeMotorB()` set the motor-driver inputs to the braking
state. `RCStop()` applies braking to both motors.

### Vehicle movement functions

- `RCForward()` drives both motors clockwise using the calibrated forward PWM
  values.
- `RCBackward()` reverses both motors using the calibrated backward PWM values.
- `RCLeft()` and `RCRight()` drive the motors in opposite directions so the
  vehicle turns on the spot.

### `onDataReceived()`

ESP-NOW calls this function when data arrives. It rejects data if its length
does not match `ControlPacket`. A correctly sized packet is copied into
`incomingPacket`, and `newPacketAvailable` tells the main loop that a command is
ready.

### Receiver setup

The receiver starts Wi-Fi in station mode, selects ESP-NOW channel 1,
initialises ESP-NOW and registers the receive callback. It then configures the
TB6612FNG control pins as outputs and calls `enterStandby()` before normal
operation begins.

### Receiver main loop

When a packet is available, the loop clears the flag and uses a switch statement
to execute the command. Movement commands update `lastValidCommandTime_MS` and
set `vehicleMoving` to `true`. Stop brakes the motors and clears the movement
flag. Invalid commands also stop the vehicle.

### Communication-loss failsafe

While the vehicle is moving, the program compares `millis()` with the time of
the last valid command. If the elapsed time reaches 2000 ms, `RCStop()` brakes
both motors and the message `Wireless command timeout` is printed.

This prevents the last movement instruction from continuing indefinitely if
the controller loses power or wireless communication is interrupted.
