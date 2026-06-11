# 03 — Firmware Architecture

## The Problem with a Super-Loop

The simplest embedded firmware looks like this:

```c
while (1) {
    if (adc_ready) { run_fft(); }
    if (fft_ready) { update_display(); }
    check_buttons();
}
```

This is called a **super-loop** or polling loop. It works for simple programs, but it has a fundamental problem: every task has to check whether it should run on every pass through the loop. As the program grows, the loop gets longer, latency grows unpredictably, and tasks start interfering with each other in subtle ways.

There's a deeper problem too: what if `run_fft()` takes 5ms but a button press happens during that time? The button won't be seen until the loop comes back around. On an MCU with no OS, you're in charge of scheduling.

jigglescanner uses a **cooperative event-driven architecture** instead.

## The Event-Driven Model

The core idea: **nothing runs unless something happened.**

```
Hardware event occurs (DMA complete, button pressed, timer fires)
    └─> ISR runs (fast — just sets a bit, nothing else)
    
Main loop checks event_flags
    └─> dispatches to the appropriate handler
    └─> handler runs, does its job, returns
    └─> loop checks again
```

The main loop is a dispatcher. ISRs are event producers. Handlers are workers. Each layer has exactly one job.

## event_flags: The Heartbeat of the System

`event_flags` is a `volatile uint32_t` — a 32-bit integer where each bit represents a different event. It lives in `events.h` as the single source of truth:

```c
// events.h
#define EVENT_ADC_READY    (1U << 0)   // DMA filled ADC buffer
#define EVENT_DSP_READY    (1U << 1)   // FFT and power spectrum complete
#define EVENT_BTN_PAUSE    (1U << 2)   // Run/pause button pressed
#define EVENT_BTN_SCALE    (1U << 3)   // Scale button pressed
#define EVENT_BTN_SPAN_UP  (1U << 4)   // Span up button pressed
#define EVENT_BTN_SPAN_DN  (1U << 5)   // Span down button pressed
```

**Why `volatile`?**  
`event_flags` is written by ISRs and read by the main loop. The compiler doesn't know about ISRs — it might see that `event_flags` never changes within the main loop body and "optimize" by caching its value in a CPU register, never re-reading from memory. `volatile` tells the compiler: *this variable can change at any time, always read it fresh from RAM.*

**Why a bitmask instead of separate variables?**  
One register read checks all pending events at once. Bit operations (`|=`, `&=`, `&`) are single-cycle on Cortex-M0+. And it's atomic-friendly: setting a single bit with `|=` is a read-modify-write, but on this architecture that's acceptable for flag communication between ISR and main loop.

**Why `1U << n` instead of just a number?**  
`1U` is an unsigned 1. The `U` prevents signed integer overflow when shifting into the high bits. `(1U << 31)` is valid; `(1 << 31)` is technically undefined behavior in C because it overflows a signed int.

## The Dispatcher

The main loop in `main.c` looks like this:

```c
while (1) {
    if (event_flags & EVENT_ADC_READY) {
        event_flags &= ~EVENT_ADC_READY;   // clear the flag
        App_Handle_DSP(&state);
    }
    if (event_flags & EVENT_DSP_READY) {
        event_flags &= ~EVENT_DSP_READY;
        App_Handle_Display(&state);
    }
    if (event_flags & EVENT_BTN_PAUSE) {
        event_flags &= ~EVENT_BTN_PAUSE;
        App_Handle_Button_Pause(&state);
    }
    // ... other buttons
}
```

Notice: the flag is **cleared before** the handler runs. This is intentional. If the flag were cleared after, and an ISR fired during the handler and set the flag again, the second event would be silently lost when the handler cleared it on return.

## The Signal Flow

Here's the complete path from audio input to pixels on screen:

```
Audio signal → ADC → DMA fills adc_buf[256]
                              │
                    HAL_ADC_ConvCpltCallback() (ISR)
                              │ sets EVENT_ADC_READY
                              ▼
                    App_Handle_DSP()
                      • subtract DC mean
                      • apply Hanning window
                      • run FFT256
                      • compute power spectrum → magnitude[128]
                      │ sets EVENT_DSP_READY
                              ▼
                    App_Handle_Display()
                      • for each column x (0–159):
                        • map x → FFT bin
                        • scale magnitude → bar height
                        • fill 192-byte column buffer
                        • wait for previous DMA transfer
                        • kick off SPI DMA for this column
```

Each stage only starts when the previous one has finished. The CPU is free between stages — if a button event fires during `App_Handle_DSP`, the button handler runs on the next pass through the dispatcher.

## Module Breakdown

### `main.c` — Init and Dispatcher Only
Initializes all HAL peripherals, starts TIM3 (ADC trigger), starts ADC with DMA, starts TIM1 PWM (test signal), then enters the dispatcher loop. `main.c` contains no application logic — it only routes events to handlers.

### `dsp.c / dsp.h` — Signal Processing
Contains the Hanning window table, twiddle factor tables, `FFT256()`, power spectrum computation, and `App_Handle_DSP()`. All float tables are `static const` — they live in flash, not RAM.

### `spectrum_display.c / spectrum_display.h` — Display Rendering
Contains the 192-byte column buffer, peak-hold state, bar rendering logic, frequency axis labels, and `App_Handle_Display()`. Manages the SPI DMA handshake.

### `buttons.c / buttons.h` — Input Handling
Contains `HAL_GPIO_EXTI_Falling_Callback()` (the ISR — sets flags only), and the four `App_Handle_Button_*()` functions which do debounce checking and state updates.

### `app_state.h` — Shared State
Defines `app_state_t`, the struct that all modules read from and write to. Acts as the single source of truth for application state (running/paused, scale mode, span level, peak hold array).

### `events.h` — Event Definitions
All `EVENT_FLAG_*` bit definitions. Every module that sets or checks a flag includes this header. Having one file as the source of truth prevents duplicate bit assignments (which caused a real bug early in development).

### `st7735screen.c / st7735screen.h` — LCD Driver
Low-level SPI LCD driver: pin control, init sequence, drawing primitives (pixel, line, rect, fill), character rendering, and the `ST7735_DrawSpectrumBar()` function used by the display module.

## Why Cooperative Instead of Preemptive?

A preemptive system (like an RTOS) can interrupt a running task to switch to a higher-priority one. That's powerful but adds complexity: you need mutexes, task stacks, careful thinking about shared state.

Cooperative means each handler runs to completion and voluntarily returns. Nothing interrupts it mid-execution. This is simpler and appropriate here because:

1. Each handler is short — DSP takes a few milliseconds, display rendering is DMA-assisted
2. Shared state (`app_state_t`) is only touched by one handler at a time, so no mutex needed
3. The Cortex-M0+ has limited hardware support for RTOS primitives anyway

The tradeoff: a slow handler will delay everything else. If a handler blocked for 100ms, buttons would feel unresponsive. The design keeps handlers fast and non-blocking by design.
