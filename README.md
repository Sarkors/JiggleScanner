# JiggleScanner
Building a small formfactor spectrum analyzer, using an STM NUCLEO-C031C6, and learning tons of concepts while doing so, embedded programming, C, Event-driven Programming, DSP basics, PCB design, Electronics, etc. 
# jigglescanner

# jigglescanner

A real-time FFT spectrum analyzer built on the STM32C031C6 (NUCLEO-C031C6 dev board).  
Displays a live 0–4kHz frequency spectrum on a 1.8" ST7735 LCD.  
Built as a hands-on embedded systems learning project alongside Miro Samek's *Modern Embedded Systems Programming* course.

---

## Hardware

| Component | Part |
|---|---|
| MCU | STM32C031C6T6 (Cortex-M0+, 48MHz, 32KB flash, 12KB SRAM) |
| Dev board | NUCLEO-C031C6 |
| Display | ST7735 1.8" TFT LCD, 160×128, SPI |
| Input | Audio jack (AC-coupled, voltage-divided to ADC midpoint) |
| Test signal | TIM1_CH1 PWM, 1kHz 50% duty on PA8 |

## Pin Assignments

| Signal | Pin | Notes |
|---|---|---|
| SPI1_MOSI | PA12 | LCD data |
| SPI1_SCK | PA5 | LCD clock |
| LCD_CS | PA11 | Active low chip select |
| LCD_DC | PA4 | Low = command, High = data |
| LCD_RST | PA6 | Active low reset |
| LCD_BL | PB0 | Backlight enable |
| USART2_TX | PA2 | Debug UART (ST-Link VCP) |
| USART2_RX | PA3 | |
| ADC_IN0 | PA0 | Signal input channel A |
| ADC_IN1 | PA1 | Signal input channel B |
| ADC_IN7 | PA7 | Test signal input |
| TIM1_CH1 | PA8 | 1kHz PWM test signal output |
| LED_POWER | PB6 | |
| LED_ACTIVITY | PB5 | |
| LED_EXTRA | PB10 | |
| BTN_RUN_PAUSE | PB1 | EXTI falling edge |
| BTN_SCALE | PB2 | EXTI falling edge |
| BTN_SPAN_UP | PB3 | EXTI falling edge |
| BTN_SPAN_DOWN | PB4 | EXTI falling edge |
| SWDIO / SWCLK | PA13 / PA14 | SWD debug interface |

## Building and Flashing

1. Open in STM32CubeIDE
2. Build with `Project → Build All` (uses `-Os`, `--specs=nano.specs`)
3. Flash and debug with `Run → Debug` over ST-Link

> **Do not enable `-u _printf_float`** — it pulls in ~8KB of double formatting code and will overflow flash.  
> All float debug output uses integer-scaled equivalents: `printf("%d\n", (int)(val * 10000));`

## Firmware Architecture

The firmware uses a **cooperative event-driven main loop** — no RTOS, no blocking waits.

```
ISR (ADC DMA complete)
  └─> sets bit in event_flags

main loop dispatcher
  └─> sees bit set → calls handler → handler does one job → returns

Handlers:
  App_Handle_DSP()       ← windowing + FFT + power spectrum
  App_Handle_Display()   ← bar rendering via SPI DMA
  App_Handle_Button_*()  ← debounce + state update
```

## File Structure

```
Core/Src/
├── main.c                ← init, dispatcher loop only
├── dsp.c                 ← Hanning window, FFT256, power spectrum
├── spectrum_display.c    ← column-strip DMA display rendering
├── buttons.c             ← EXTI callbacks, debounce, state updates
└── st7735screen.c        ← SPI LCD driver (init, primitives, font)

Core/Inc/
├── app_state.h           ← shared app_state_t struct
├── events.h              ← all EVENT_FLAG_* bit definitions
├── dsp.h
├── spectrum_display.h
├── buttons.h
└── st7735screen.h
```

## Repository Structure

```
jigglescanner/
├── README.md
├── hardware/
│   └── WiggleScannerSchematic.kicad_sch   ← KiCad schematic (open with KiCad eeschema)
├── Core/Src/                              ← firmware source files
├── Core/Inc/                              ← headers
└── docs/                                  ← documentation
```

<img width="2000" height="2000" alt="image" src="https://github.com/user-attachments/assets/daa03705-ee57-4d17-a126-1353fed5e3ef" />

<img width="2000" height="2000" alt="image" src="https://github.com/user-attachments/assets/64392fe1-88ec-4494-b5df-18b453a1b3de" />




## Docs

- [Project Overview & Goals](docs/01_project_overview.md)
- [Hardware Design Decisions](docs/02_hardware.md)
- [Firmware Architecture](docs/03_firmware_architecture.md)
- [DSP Pipeline: ADC → FFT → Spectrum](docs/04_dsp_pipeline.md)
- [Display Driver & SPI DMA](docs/05_display_driver.md)
- [Lessons Learned & STM32C0 Gotchas](docs/06_lessons_learned.md)
- [Schematic & Hardware Design](docs/07_schematic.md)
