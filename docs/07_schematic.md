# 07 — Schematic & Hardware Design

The `hardware/` folder contains the KiCad schematic for the production PCB design.  
The prototype runs on a NUCLEO-C031C6 dev board — the schematic represents the next step: a standalone PCB with the same MCU, proper analog front-end, and all connectors in one place.

---

## How to Open the Schematic

1. Install [KiCad](https://www.kicad.org/) (free, open source)
2. Open `hardware/WiggleScannerSchematic.kicad_sch` in the KiCad Schematic Editor (eeschema)

The schematic uses standard KiCad libraries plus a custom symbol library (`wigglescannerlib`) for the ST7735 LCD module and documentation symbols. If KiCad reports missing symbols on open, point it at the `hardware/wigglescannerlib` folder.

---

## Block Diagram

```
USB-C (J5)
    │ +5V
    ▼
AMS1117-3.3 (U44)          ← LDO voltage regulator
    │ +3V3
    ├──► STM32C031C6 (U1)  ← MCU: all firmware runs here
    ├──► ST7735 LCD (J3)   ← SPI display
    └──► MCP6022 (U2)      ← dual op-amp analog front end
    
Signal inputs:
  BNC connector (J1) ──► BAT54S protection (D5) ──► MCP6022 ──► ADC1_IN0
  Audio jack (J6)    ──► BAT54S protection (D6) ──► MCP6022 ──► ADC1_IN1
  TIM1_CH1 (PA8)     ──────────────────────────────────────► ADC1_IN7 (test)

User interface:
  4× push buttons (SW3–SW6) ──► PB1–PB4 (EXTI)
  3× LEDs (D1–D3)            ──► PB5, PB6, PB10
  Potentiometer (RV1, 10k)   ──► future analog control
  
Debug:
  SWD header (J4, 4-pin)     ──► SWDIO/SWCLK/3V3/GND
  UART header (J4 or test point TP1)
```

---

## Component Reference

### Power: USB-C Input + LDO Regulation

| Ref | Value | Purpose |
|---|---|---|
| J5 | USB_C_Receptacle_PowerOnly_6P | Power input connector |
| R16, R17 | 5.1kΩ | USB-C CC1/CC2 pull-downs — required to negotiate 5V from USB-C chargers |
| U44 | AMS1117-3.3 | LDO regulator: 5V → 3.3V, 1A max |
| C12, C13 | 10µF | AMS1117 input and output bulk capacitors (stability + hold-up) |

**Why 5.1kΩ on CC1/CC2?**  
USB-C chargers use the CC (Configuration Channel) pins to detect what's connected. A charger sees 5.1kΩ pull-downs on both CC pins and knows to supply 5V at up to 900mA. Without these resistors, many USB-C chargers will supply 0V or enter an error state. This is a required part of the USB-C spec for power-only devices.

**Why an LDO instead of running directly from 5V?**  
The STM32C031 and ST7735 are both 3.3V devices. Running them from 5V would destroy them. The AMS1117-3.3 drops the 5V USB supply to a stable 3.3V regardless of how much current is drawn (up to 1A, far more than this circuit needs).

---

### MCU: STM32C031C6T6

| Ref | Value | Purpose |
|---|---|---|
| U1 | STM32C031C6Tx | Main microcontroller |
| C1, C4, C10, C11 | 100nF | Decoupling capacitors, one per VDD pin |
| C2, C5 | 10µF | Bulk decoupling on power entry |
| C3 | 1µF | VREF+ filtering |

**Why decoupling capacitors on every power pin?**  
When the MCU's internal logic switches (millions of times per second), it draws brief spikes of current. Without a capacitor nearby, that current spike travels all the way back to the power supply, causing a small voltage droop on the VDD rail. That droop can cause the MCU to malfunction or reset. The 100nF capacitor sits right next to the pin and supplies the spike locally — the power supply never even sees it.

The 10µF bulk capacitors handle slower, larger variations. 100nF handles fast glitches, 10µF handles sustained load changes. Both are needed.

---

### Analog Front End: MCP6022 + Protection

| Ref | Value | Purpose |
|---|---|---|
| U2 | MCP6022 | Dual rail-to-rail op-amp |
| D5, D6 | BAT54S | Dual Schottky diode clamp (input protection) |
| R2, R3 | 10kΩ | Voltage divider for AC bias (BNC channel) |
| R4, R5 | 10kΩ | Voltage divider for AC bias (audio channel) |
| C6, C7 | 1nF | Anti-aliasing RC filter caps (C0G dielectric) |
| R10, R11 | 330Ω | Anti-aliasing RC filter resistors |
| C8, C9 | 10µF | AC coupling capacitors |
| R18 | 1MΩ | DC bias path resistor |

**Signal path (audio jack → ADC):**

```
Audio jack (AC signal, may go negative)
    │
    C8 (10µF) ──── AC coupling: blocks DC, passes AC signal
    │
    R4/R5 divider ─── biases signal to 1.65V midpoint (VCC/2)
    │
    BAT54S (D6) ──── clamps to 0V–3.3V range (input protection)
    │
    MCP6022 channel ─── buffers signal, prevents ADC loading
    │    R11 + C7 ──── anti-aliasing low-pass filter (~482kHz cutoff)
    │
    ADC1_IN1 (PA1) ── 12-bit conversion
```

**Why AC coupling (C8/C9)?**  
Audio signals are AC — they swing above and below zero volts. The STM32 ADC only accepts 0–3.3V. Connecting audio directly would drive the ADC pins negative on negative swings, which can permanently damage the MCU. The 10µF capacitor blocks the DC component and passes only the AC waveform.

**Why the voltage divider (R4/R5)?**  
After AC coupling, the signal is centered around 0V — it still goes negative. The 100kΩ/100kΩ divider creates a 1.65V DC bias point. With the signal AC-coupled onto this bias, it now swings between approximately 0V and 3.3V instead of going negative.

**Why the BAT54S (D5/D6)?**  
The voltage divider provides the correct bias under normal conditions, but the ADC pins are still vulnerable to overvoltage from large transients (plugging in a loud source, static discharge). The BAT54S is a dual Schottky diode in a single package — one diode clamps to GND (can't go below 0V), the other clamps to +3V3 (can't go above 3.3V). It's a last line of defense before the ADC.

**Why the MCP6022 op-amp buffer?**  
The ADC has a non-trivial input impedance — when it samples, it briefly connects an internal capacitor to the pin and charges it. If the source impedance is high, the ADC doesn't charge fully in time and reads the wrong value. The op-amp buffer has very high input impedance (doesn't load the voltage divider) and very low output impedance (can drive the ADC sample capacitor quickly). The MCP6022 is rail-to-rail — its output can swing all the way from 0V to 3.3V, critical since our signal spans that full range.

**Why C0G/NP0 for the anti-aliasing filter caps (C6/C7)?**  
The anti-aliasing filter needs a predictable, stable capacitance value to set a precise cutoff frequency. X7R and X5R ceramic capacitors change capacitance significantly with DC bias voltage and temperature. C0G (NP0) dielectric is stable across voltage and temperature — the capacitance stays close to the rated value regardless of conditions. For filter components, use C0G.

**Anti-aliasing filter cutoff:**  
`f_c = 1 / (2π × R × C) = 1 / (2π × 330 × 1e-9) ≈ 482kHz`

This is intentionally much higher than the 4kHz audio range. The purpose is to block radio-frequency interference and very high frequency noise that the ADC might alias down into the audio band, not to limit the audio bandwidth.

---

### Signal Inputs

| Ref | Value | Purpose |
|---|---|---|
| J1 | Conn_Coaxial (BNC) | RF/instrument signal input |
| J6 | AudioJack4 | 3.5mm audio input (stereo + mic wired) |

The BNC connector is for instrument/RF signals (oscilloscope probe, function generator). The audio jack is for consumer audio (phone, headphone output). Both go through the same protection and conditioning circuit.

---

### Display Interface

| Ref | Value | Purpose |
|---|---|---|
| J3 | Conn_01x04 | ST7735 LCD connector (SPI: MOSI, SCK, CS, DC, RST, BL) |

The ST7735 module connects via a pin header. All SPI signals, control signals, and power are routed to this connector.

---

### User Interface

| Ref | Value | Purpose |
|---|---|---|
| SW3 | SW_Push | BTN_RUN_PAUSE (PB1) |
| SW4 | SW_Push | BTN_SCALE (PB2) |
| SW5 | SW_Push | BTN_SPAN_UP (PB3) |
| SW6 | SW_Push | BTN_SPAN_DOWN (PB4) |
| RV1 | 10kΩ potentiometer | Future analog control (not yet implemented in firmware) |
| R12–R15 | 10kΩ | Button pull-up resistors |
| D1 | LED | LED_POWER (PB6) |
| D2 | LED | LED_ACTIVITY (PB5) |
| D3 | LED | LED_EXTRA (PB10) |
| R7–R9 | 330Ω | LED current-limiting resistors |

**LED current calculation:**  
`I = (VCC - V_LED) / R = (3.3 - 2.0) / 330 ≈ 4mA`  
The STM32 GPIO spec characterizes drive capability at 8mA. Running at 4mA gives a good safety margin and is still plenty bright for an indicator LED.

---

### Debug Interface

| Ref | Value | Purpose |
|---|---|---|
| J4 | Conn_01x03 | SWD debug header: SWDIO, SWCLK, GND |
| R1 | 4.7kΩ | SWDIO pull-up (keeps line in defined state during normal operation) |
| TP1 | TestPoint | UART / general test access |

The SWD (Serial Wire Debug) header lets an external ST-Link programmer/debugger flash and debug the chip on the standalone PCB, without needing the NUCLEO dev board. On the NUCLEO, the onboard ST-Link handles this. On a custom PCB, you need this header.

---

## What Was Learned Designing This Schematic

**ERC (Electrical Rules Check) debugging in KiCad** is a real skill. The errors encountered and resolved during this design:

- `power_pin_not_driven` on VSS — KiCad's false positive because the STM32 symbol uses VSS internally, but it's electrically connected to GND. Fixed with the ERC "ignore this violation" feature rather than hacking the symbol.
- Errant GND symbol on the VBUS/+5V junction — a schematic short that would have destroyed hardware if fabbed. Caught by ERC before anything was printed.
- `lib_symbol_mismatch` warnings — resolved by editing the symbol in the library rather than the schematic instance (editing the instance leaves the library out of sync, causing the warning to re-appear).
- USB-C CC pull-down placement — the `PWR_FLAG` symbol placement relative to C13 mattered for ERC to correctly identify which net was +5V.

**The anti-aliasing filter topology** had both channels wired as unity-gain buffers with the R and C disconnected from each other in the first version. The correct topology has R in series from input to the inverting input, and C from inverting input to output (feedback RC network) for one channel (low-pass filter), and the second channel as a true unity-gain buffer for the other input. Understanding why the topology matters — and what "active anti-aliasing filter" actually means — required going back to basics on op-amp feedback.
