# 01 — Project Overview & Goals

## What is jigglescanner?

jigglescanner is a real-time FFT spectrum analyzer — a device that listens to an audio signal and shows you a live graph of which frequencies are present and how strong they are.

Point it at music and you'll see the bass frequencies on the left, the highs on the right, and the spectrum dancing in real time. It's the kind of display you've seen on audio equipment for decades, and building one from scratch means touching almost every layer of embedded engineering: hardware design, peripheral configuration, signal processing, firmware architecture, and display drivers.

## Why build this?

The project was inspired by hoff_world *Silly Scope* YouTube series and built as a deliberate learning exercise alongside Miro Samek's *Modern Embedded Systems Programming* course. The two tracks were interleaved so that theory arrived just before it was needed in code — Samek's lessons on interrupts landed before the ADC/DMA work, his architecture chapters landed before the event-driven refactor.

The goal was never just a working device. It was to understand **why** every decision was made — why DMA instead of CPU polling, why a Hanning window, why event-driven instead of a super-loop, why this MCU can only do a 256-point FFT. A device you built without understanding is just a box. A device you can explain is a foundation.

## Hardware constraints that shaped everything

The STM32C031C6 was chosen as the prototype MCU because it was available on an inexpensive NUCLEO dev board and covered the fundamentals well. But it comes with a hard constraint: **12KB of SRAM**.

12,000 bytes is very little. That constraint drove a cascade of design decisions:

- **256-point FFT** — a 1024-point FFT would need ~8KB for the sample buffer alone, leaving almost nothing for the rest of the program. 256 points uses 1KB for the float buffer and leaves room to breathe.
- **ST7735 display (160×128) instead of ILI9341 (320×240)** — a 256-point FFT produces exactly 128 usable frequency bins. The ST7735's 128-pixel width is a perfect 1:1 match, meaning no interpolation math and no wasted pixels. The ILI9341 would have required stretching 128 bins across 320 pixels.
- **Column-strip DMA rendering instead of a framebuffer** — a full 160×128 framebuffer in RGB565 would be 40KB. That's more than three times the entire SRAM. Instead, the firmware renders one 128-pixel column at a time into a 192-byte buffer, then DMA-transfers it while the CPU calculates the next column.

Every one of those decisions was a direct consequence of 12KB. Understanding the constraint is understanding the design.

## What this project covers

| Domain | What was built |
|---|---|
| Hardware design | KiCad schematic, BOM, analog front-end design |
| Peripheral config | SPI, ADC, DMA, TIM, UART, GPIO, EXTI — all in CubeMX |
| Signal processing | Hanning window, Cooley-Tukey FFT, power spectrum, peak hold |
| Firmware architecture | Cooperative event-driven loop, ISR → flag → handler pattern |
| Display driver | ST7735 SPI driver from scratch, SPI DMA column rendering |
| Debugging | UART printf at every stage, register-level inspection in CubeIDE |

## What's next

The prototype runs on the NUCLEO dev board. The longer-term plan is to:
- Design and fabricate a custom PCB (schematic already drafted in KiCad)
- Upgrade to an STM32G474 or F446 for larger SRAM, enabling a 1024-point FFT and finer frequency resolution
- Add the analog front-end op-amp circuit for better input conditioning
