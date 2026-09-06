# Code walkthrough

## System overview

Two ESP32 boards communicate directly using ESP-NOW. The handheld controller
reads a two-axis analogue joystick, converts its readings into a discrete
command and transmits a `ControlPacket`. The vehicle receives the packet and
controls two TT motors through a TB6612FNG dual H-bridge.

## Shared data definitions

`VehicleCommand` lists the only commands the vehicle should execute. The
underlying type is `uint8_t`, so the command occupies one byte.

`ControlPacket` contains:

- `command`: requested movement;
- `speed`: PWM value from 0 to 255;
- `sequence`: a number incremented for each transmission and printed during
  testing to make packet flow visible.

The packet definition must remain identical on both ESP32 boards. A
`static_assert` catches an unexpected layout change during compilation.

## Controller sender

### Pin and threshold constants

GPIO34 and GPIO35 are ADC1 inputs. ADC1 was selected because it remains
available while Wi-Fi is active. GPIO32 reads the joystick's active-low Select
switch using the ESP32's internal pull-up resistor.

The joystick was measured near the centre of the 12-bit ADC range. Thresholds
create a deliberately wide neutral region, preventing natural ADC variation or
small accidental stick movements from driving the vehicle.

### `readJoystick()`

Samples both analogue axes and converts the Select pin into a meaningful
Boolean: `true` means pressed.

### `determineDirection()`

Gives the push-button Stop command first priority. It then recognises the four
cardinal directions. If the stick is centred, diagonal or outside the accepted
bands, the function returns Stop. This fail-safe default prevents an ambiguous
reading from becoming movement.

### `convertDirection()`

Separates the physical controller vocabulary from the vehicle's communication
protocol. This makes either side easier to change later.

### `prepareControlPacket()`

Stores the command and speed, then increments the diagnostic sequence number.

### ESP-NOW setup and send callback

The controller starts Wi-Fi in station mode, selects channel 1, initialises
ESP-NOW and registers the vehicle's station MAC address as a peer. The callback
reports whether the radio delivered each queued packet successfully.

### Main loop

The main loop reads the joystick, determines a direction, converts it into a
vehicle command, assigns speed zero to Stop/Invalid and transmits a packet every
50 ms.

## Vehicle receiver

### Safe startup

The motor GPIO pins are configured before the radio. `enterStandby()` disables
the TB6612FNG and sets both PWM outputs to zero, so a wireless setup failure
cannot accidentally leave the motors energised.

### Motor primitives

The clockwise and counter-clockwise functions set each H-bridge input pair and
then apply PWM. PWM is first set to zero while direction changes, reducing harsh
electrical/mechanical transitions.

### Vehicle movements

Forward and backward use independently calibrated left/right PWM values to
compensate for unequal open-loop motors. Turning drives the two motors in
opposite directions using the speed received from the controller.

### Receive callback

ESP-NOW calls `onDataReceived()` when a packet arrives. It rejects packets of
the wrong size. A FreeRTOS critical section protects the shared packet so the
main loop cannot read it halfway through an update.

### `applyPacket()`

Rejects unsupported enum values and defaults to stopping. Valid commands update
the last-command time and call the matching movement function.

### Communication-loss failsafe

While the vehicle is moving, the main loop checks how long it has been since a
valid packet arrived. If the interval reaches 2000 ms, it brakes both motors and
clears the moving state. This prevents the last movement command from continuing
indefinitely when the controller loses power or radio contact.

## Utilities

- `JoystickBenchTest.ino` measures and verifies joystick behaviour without the
  wireless system.
- `MacAddressReader.ino` obtains an ESP32 station MAC address for peer setup.
- `PacketMonitorReceiver.ino` verifies packet delivery without operating any
  motors.
