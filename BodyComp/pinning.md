
# Pinning

## Weight Modules
|load cell | HX711|
|-|-|
| Red | E+ |
| Black | E- |
| White | A- |
| Green | A+ |

|HX711|ESP32|
|-|-|
|GND|GND|
|VCC|Vin|
|DT| GPIO21 [D21]|
|SCK| GPIO 22 [D22]|


## Height Module

|HC-SR04|ESP32|
|-|-|
|VCC|VIN|
|Trig|GPIO18 [D18]|
|Echo|GPIO19 [D19]|
|GND|GND|

## BIA Module

50 kHz sine injection + differential voltage sense via AD620.

| Net | ESP32 pin | Notes |
|-|-|-|
| DAC sine out | GPIO25 [D25] | ESP32 DAC1 (only GPIO25/26 are DAC-capable). Drives R1 (10 kΩ) into LM358 IN+ |
| AD620 OUT sense | GPIO34 [D34] | ADC1_CH6, input-only pin. AC-coupled around 2.5 V virtual GND |

| Analog node | Connection |
|-|-|
| LM358 V2I out → R3 (1 kΩ) | Red electrode (right wrist, current inject) |
| AD620 IN− (pin 2) | Yellow electrode (left ankle, voltage sense −) |
| AD620 IN+ (pin 3) | Teal electrode (right ankle, voltage sense +) |
| AD620 Rg (pins 1↔8) | R6 = 2 kΩ → gain ≈ 25.7× |
| AD620 REF (pin 5) | 2.5 V virtual GND (R4/R5 = 10 kΩ divider from +5 V) |
| AD620 V+ / V− | +5 V / GND (single supply) |

**Calibration note:** `INJECTION_CURRENT_RMS_A` in [bia.c](main/bia.c) is a theoretical
~117 µA based on V_DAC_AC/R2. Calibrate by replacing the body with a **1 kΩ
reference resistor** between the red electrode and the AD620 inputs, then scale
`INJECTION_CURRENT_RMS_A` until the reported `|Z|` reads 1000 Ω.

