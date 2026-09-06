# Pin connections

## Handheld controller ESP32

The joystick is powered from 3.3 V so its analogue outputs remain safe for the ESP32 ADC inputs.

| Joystick pin | ESP32 connection | Purpose |
| --- | --- | --- |
| VCC | 3V3 | Joystick supply |
| GND | GND | Common reference |
| Xout | GPIO34 | Horizontal ADC input |
| Yout | GPIO35 | Vertical ADC input |
| Sel | GPIO32 | Active-low push switch |

## Vehicle ESP32 to TB6612FNG

| TB6612FNG pin | ESP32 GPIO | Purpose |
| --- | --- | --- |
| PWMA | 25 | Motor A PWM |
| AIN1 | 26 | Motor A direction |
| AIN2 | 27 | Motor A direction |
| STBY | 23 | Driver enable/standby |
| PWMB | 13 | Motor B PWM |
| BIN1 | 14 | Motor B direction |
| BIN2 | 33 | Motor B direction |

## Power arrangement

- The controller ESP32 is powered from its USB power bank.
- The vehicle ESP32 is powered from its USB power bank.
- The motor supply connects to TB6612FNG `VM`, not through the ESP32.
- TB6612FNG logic supply connects to the appropriate ESP32 logic supply.
- Vehicle ESP32, TB6612FNG and motor supply grounds must share a common ground.
- Motor A connects to `AO1/AO2`; Motor B connects to `BO1/BO2`.

Never connect a motor directly to an ESP32 GPIO pin.
