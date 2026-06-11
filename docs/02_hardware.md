# 02 — Hardware Design Decisions

## The MCU: STM32C031C6

- **Core:** ARM Cortex-M0+, 48MHz
- **Flash:** 32KB (program storage — persists when unplugged)
- **SRAM:** 12KB (runtime memory — lost on power-off)
- **ADC:** 12-bit, up to 21 channels, max 2.5 MSPS
- **Peripherals:** SPI, USART, TIM, DMA, GPIO with EXTI

The C031 was chosen for prototype availability and affordability. Its constraints (especially 12KB SRAM) actively shaped the design — see [Project Overview](01_project_overview.md). The production design would upgrade to an STM32G474 or F446 for a larger FFT.

## Display: ST7735 vs ILI9341

The original display candidate was the ILI9341 (320×240). It was replaced with the ST7735 (160×128) for three reasons:

**1. Perfect bin-to-pixel mapping.**  
A 256-point FFT produces 128 usable frequency bins. The ST7735's 128-pixel width is an exact 1:1 match. No interpolation, no wasted pixels.

**2. RAM.** A full framebuffer for ILI9341 in RGB565 = 320 × 240 × 2 = 153,600 bytes. For ST7735 = 160 × 128 × 2 = 40,960 bytes. Neither fits in 12KB, but the ST7735 column-strip approach only needs 192 bytes at a time.

**3. Speed.** The ST7735 at 24MHz SPI achieves ~73 FPS theoretical. The ILI9341 at the same rate achieves ~20 FPS. Faster display = more responsive spectrum.

## SPI Configuration

SPI is used for the LCD because it's fast, simple, and one-directional for this use case (we only ever write to the display, never read from it). In CubeMX the SPI is configured as **Transmit Only Master**.

**Baud rate calculation:**  
`SPI baud = SYSCLK / prescaler = 48MHz / 4 = 12MHz`

The LCD has a few control lines beyond MOSI/SCK:

| Pin | Function |
|---|---|
| CS (Chip Select) | Active LOW — tells the LCD "this message is for you" |
| DC (Data/Command) | LOW = command (set orientation, clear screen), HIGH = pixel data |
| RST (Reset) | Active LOW hardware reset on startup |
| BL (Backlight) | GPIO high to turn on backlight |

SPI can address multiple peripherals on the same MOSI/SCK lines by toggling different CS pins. Only the device with CS pulled low listens.

## ADC Configuration

The ADC converts analog voltage (0–3.3V) into a 12-bit digital number (0–4095). That's `3.3V / 4096 = 0.806mV` per step.

**Why DMA instead of CPU polling?**  
Audio data arrives continuously. If the CPU read each ADC sample in a loop, it would spend nearly all its time just moving numbers from the ADC register into a buffer — leaving no time for FFT, display, or buttons. DMA is a hardware copy engine that runs independently of the CPU. It watches the ADC data register, and every time a new sample appears, it copies it to the next slot in the buffer. When the buffer is full, it fires a single interrupt. The CPU does nothing until that moment.

**Circular mode:**  
When the DMA fills the buffer, instead of stopping it wraps around and starts writing from the beginning again. This keeps samples flowing continuously without any CPU involvement between interrupts.

**Why only one ADC channel active at a time?**  
The STM32C031's ADC scans channels in ascending order when multiple channels are enabled in CHSELR. With DMA in circular mode, samples from different channels interleave in the buffer — channel 0, channel 7, channel 0, channel 7... The FFT then processes a mix of two signals instead of one clean signal. Enabling only the active input channel prevents this.

**Hardware triggering via TIM3:**  
The ADC is triggered by TIM3's TRGO (Trigger Output) event rather than being triggered by software. This matters for FFT quality.

If the ADC were triggered in software (e.g., inside a loop or interrupt), the time between samples would vary slightly depending on what else the CPU was doing. The FFT algorithm assumes samples are evenly spaced in time. Irregular spacing causes:
- Frequency peaks appearing at the wrong bins
- Phantom peaks at frequencies that don't exist in the signal (spectral leakage)

TIM3 fires at a precise hardware-determined rate regardless of CPU load. The ADC samples at exactly the right intervals every time.

**TIM3 configuration for 8kHz sample rate:**  
`Sample rate = SYSCLK / ((prescaler + 1) × (ARR + 1)) = 48MHz / (1 × 6000) = 8kHz`  
With prescaler = 0 and ARR = 5999... but the actual values in CubeMX were tuned to hit 8kHz exactly given the peripheral clock.

## Test Signal: TIM1_CH1 PWM

To test the DSP pipeline without needing an external audio source, TIM1 generates a 1kHz square wave on PA8. This gives a known frequency that should produce a visible peak at the 1kHz bin in the spectrum display.

**Formula:**  
`Frequency = SYSCLK / ((prescaler + 1) × (ARR + 1))`  
`= 48MHz / (48 × 1000) = 1000Hz`

With prescaler = 47 and ARR = 999. Pulse = ARR/2 = 499 for 50% duty cycle.

## Analog Front-End (Minimum Viable Prototype)

A phone audio output is AC-coupled (it swings above and below zero volts). The STM32 ADC only accepts 0–3.3V — it cannot measure negative voltages.

The minimum viable input circuit:
1. **10µF electrolytic capacitor (AC coupling)** — blocks the DC component of the audio signal, passing only the AC waveform
2. **100kΩ / 100kΩ voltage divider** — biases the signal to 1.65V (half of 3.3V), so the audio waveform swings between ~0V and ~3.3V instead of going negative

The firmware's `App_Handle_DSP()` subtracts the DC mean of the ADC buffer before windowing, which removes any residual DC offset automatically.

A proper production design would add an op-amp buffer (MCP6022) before the ADC for input protection, impedance matching, and active anti-aliasing filtering. For the prototype, the passive divider is sufficient.

## Buttons and LEDs

**Buttons (PB1–PB4):**  
Configured as GPIO inputs with internal pull-up resistors enabled. When idle, the pin is held HIGH by the pull-up. When the button is pressed, it connects to GND and the pin goes LOW. The firmware detects the HIGH→LOW transition (falling edge) via EXTI (external interrupt).

**Why pull-up and not pull-down?**  
Pull-up is the more common convention for buttons because it means "pressed = low, idle = high" which is electrically clean and what most hardware debounce circuits expect.

**LEDs (PB5, PB6, PB10):**  
GPIO outputs configured at low speed with no pull resistors. Low-speed GPIO produces slower rise/fall times on the output pin, which reduces high-frequency harmonic noise radiated from the PCB traces — important when ADC inputs are nearby.

## UART Debug Interface

USART2 is connected to the ST-Link VCP (Virtual COM Port) on the NUCLEO board, which appears as a COM port on the PC. Used for `printf`-style debug output at 115200 baud.

> **Critical NUCLEO-C031C6 gotcha:** The ST-Link VCP connects to **USART2** (PA2/PA3), not USART1. Most STM32 tutorials assume USART1. Using USART1 produces no output and wastes hours of debugging time.
