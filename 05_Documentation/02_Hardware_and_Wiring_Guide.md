# LumiTrack Hardware Wiring & Pin Connection Guide

## Target Board: STM32 NUCLEO-F446RE

---

## 1. STM32 NUCLEO-F446RE Pinout Summary

The STM32 NUCLEO-F446RE provides two sets of connectors:
1. Standard **Arduino Uno V3** headers (inner pins).
2. STMicroelectronics **Morpho** extension headers (outer pins).

All required project connections utilize the easily accessible Arduino Uno V3 headers:

| Peripheral / Signal | Nucleo MCU Pin | Arduino Header Pin | Connector / Pin # | Direction / Type | Electrical Characteristics |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **LDR Analog Input** | `PA0` | `A0` | CN8 Pin 1 | Analog Input | 0.0 V to 3.3 V max |
| **Potentiometer Input**| `PA1` | `A1` | CN8 Pin 2 | Analog Input | 0.0 V to 3.3 V max |
| **Servo PWM Signal** | `PA6` | `D12` | CN5 Pin 5 | Alternate Function (TIM3_CH1)| 3.3 V Logic Level, 50 Hz PWM |
| **User Status LED (LD2)**| `PA5` | `D13` | Onboard | Digital Output | Onboard Green LED |
| **User Button (B1)** | `PC13` | - | Onboard | Digital Input (Pull-up) | Onboard Blue Button (Active Low)|
| **USART2 TX (VCP)** | `PA2` | `D1` | Onboard / CN9 Pin 2 | ST-LINK VCP UART | 115200 Baud, 8N1 |
| **USART2 RX (VCP)** | `PA3` | `D0` | Onboard / CN9 Pin 1 | ST-LINK VCP UART | 115200 Baud, 8N1 |
| **Logic Supply (3.3V)**| `3V3` | `3.3V` | CN6 Pin 4 | Power Output | 3.3 V regulated DC |
| **Common Ground** | `GND` | `GND` | CN6 Pin 6 / 7 | Power Ground | Reference Ground (0 V) |

---

## 2. Sensor & Actuator Connection Diagrams

### 2.1. LDR Light Sensor Circuit (Arduino Header `A0`)
The Light-Dependent Resistor forms a voltage divider with a fixed $10\text{ k}\Omega$ pull-down resistor:

```
    Nucleo 3.3V (CN6-4)
          o
          |
         [ ]  LDR (Light Dependent Resistor)
          |
          +---------> Connected to Nucleo Pin A0 (PA0)
          |
         [ ]  10 kΩ Fixed Resistor (Brown-Black-Orange)
          |
          o
     Nucleo GND (CN6-6)
```
- In bright light: LDR resistance drops (~1 kΩ) $\rightarrow$ voltage at `A0` rises toward ~3.0 V.
- In darkness: LDR resistance rises (>100 kΩ) $\rightarrow$ voltage at `A0` drops toward ~0.2 V.

### 2.2. Potentiometer Circuit (Arduino Header `A1`)
A $10\text{ k}\Omega$ rotary potentiometer is used for manual override and setting the conservation target threshold:

```
    Nucleo 3.3V (CN6-4)
          o
          |
     [Terminal 1]
         [===]  10 kΩ Potentiometer
     [Terminal 2 (Wiper)] ---> Connected to Nucleo Pin A1 (PA1)
         [===]
     [Terminal 3]
          |
          o
     Nucleo GND (CN6-7)
```

### 2.3. SG90 / MG90S Servo Wiring (CRITICAL POWER NOTICE)

> [!CAUTION]
> **DO NOT POWER THE SERVO FROM THE NUCLEO 3.3V OR 5V PINS!**  
> Servos draw inductive peak currents up to 500mA–800mA during motor starts and reversals. Powering a servo from the Nucleo board will cause voltage brownouts, ST-LINK disconnects, or MCU resets.  
> **Always power the servo from an external 5V power supply (e.g., 4× AA batteries, 5V USB breakout, or a 5V 2A wall adapter) with a COMMON GROUND connected to the Nucleo.**

```
   EXTERNAL 5V POWER SUPPLY                     STM32 NUCLEO-F446RE
   +----------------------+                     +---------------------+
   |                      |                     |                     |
   |   5V (+) VCC --------+-----> RED Wire      |                     |
   |                      |       (Servo VCC)   |                     |
   |                      |                     |                     |
   |   GND (-) -----------+-----> BROWN/BLACK   |                     |
   |                      |       (Servo GND)   |                     |
   +----------+-----------+          |          |                     |
              |                      +--------->| GND Pin (CN6-6)     |
              |  (Common Ground Wire)           |                     |
              +-------------------------------->|                     |
                                                |                     |
                                  ORANGE/YELLOW |                     |
                                  (Servo Signal)+<--- Pin D12 (PA6)   |
                                                |     (TIM3_CH1 PWM)  |
                                                +---------------------+
```

---

## 3. Approved Angle Physical Alignment

Mount the servo horn with the LDR or shade louver on a stable fixture. Verify the physical sweep aligns with the 7 approved angles:

| Angle ID | Nominal Angle | Physical Orientation | Purpose in Variant 10.03 |
| :---: | :---: | :---: | :--- |
| `POS_0` | **15°** | Far Left Tilt | Angle checkpoint 0 (Deflection position for solar ray entering right) |
| `POS_1` | **40°** | Medium Left | Angle checkpoint 1 |
| `POS_2` | **65°** | Shallow Left | Angle checkpoint 2 |
| `POS_3` | **90°** | **Center / Safe Home** | **Default Safe Neutral / Viewing Orientation / Safe Home on link loss** |
| `POS_4` | **115°**| Shallow Right | Angle checkpoint 4 |
| `POS_5` | **140°**| Medium Right | Angle checkpoint 5 |
| `POS_6` | **165°**| Far Right Tilt | Angle checkpoint 6 (Deflection position for solar ray entering left) |
