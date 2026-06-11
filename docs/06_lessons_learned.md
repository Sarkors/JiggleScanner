# 06 — Lessons Learned & STM32C0 Gotchas

This document is the one that would help you rebuild from scratch. It captures the non-obvious things — the bugs that took hours to find, the STM32C031-specific traps that differ from every tutorial, and the principles that emerged from doing this the hard way.

---

## STM32C031-Specific Gotchas

These are things that are **different on the C031** compared to other STM32 families and will not be obvious from tutorials or AI-generated code.

### 1. The EXTI Callback Has a Different Name

On most STM32 families (F4, G0, L4, etc.), the falling-edge GPIO interrupt callback is:
```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
```

On STM32C0 specifically, it is:
```c
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
```

If you use the wrong name, the function compiles without errors (it's just an unused function from the linker's perspective), but your button interrupts silently never fire. This took hours of register-level debugging to find.

**How it was found:** Checked `FPR1`, `IMR1`, `FTSR1`, and `EXTICR` registers directly in the CubeIDE memory view, confirmed the hardware was configured correctly, then realized the callback name was the only remaining explanation.

### 2. EXTI Port Routing Uses a Different Register

On most STM32 families, external interrupt port routing is configured in `SYSCFG->EXTICR[]`.

On STM32C0:
```c
EXTI->EXTICR[]    // correct
SYSCFG->EXTICR[]  // doesn't exist on C031
```

CubeMX handles this automatically. If you're configuring EXTI manually or debugging at the register level, look in the EXTI peripheral registers, not SYSCFG.

### 3. The ST-Link VCP Connects to USART2, Not USART1

Every STM32 tutorial uses USART1 for debug output. The NUCLEO-C031C6 ST-Link Virtual COM Port is wired to **USART2** (PA2/PA3).

Using USART1 produces no output and no error. You just see a blank terminal and start questioning everything else.

### 4. No `%f` in printf with nano.specs

The project uses `--specs=nano.specs` for a smaller C library footprint. The nano variant of the standard library does not include `%f` (float) formatting by default.

There is a linker flag `-u _printf_float` that re-enables it, but it pulls in ~8KB of double-precision formatting code — a significant fraction of the 32KB flash budget.

**Do not enable `-u _printf_float`.** Instead, convert all float debug output to integer-scaled equivalents:

```c
// Wrong:
printf("val = %f\n", some_float);

// Right:
printf("val = %d.%04d\n", (int)some_float, (int)(some_float * 10000) % 10000);
// or simply:
printf("val_x10000 = %d\n", (int)(some_float * 10000));
```

### 5. Only One ADC Channel Active in CHSELR

The STM32C031 ADC scans channels in ascending order when multiple channels are enabled in CHSELR. With DMA in circular mode, samples from different channels interleave in the buffer: channel 0, channel 7, channel 0, channel 7...

If you enable multiple channels, the FFT receives an interleaved mix of signals. This produces a completely incorrect spectrum with phantom peaks.

**Always have only the desired input channel active in CHSELR at runtime.**

---

## C Fundamentals — Bugs That Were Teaching Moments

### Block Scope

Variables declared inside an `if` block are invisible outside it:

```c
if (condition) {
    int bin_start = calculate_start();  // lives only inside this block
}
for (int x = 0; x < 160; x++) {
    // bin_start doesn't exist here — compiler error or undefined behavior
    use(bin_start);
}
```

Fix: declare variables before the block that initializes them.

### `=` vs `==`

```c
if (state->scale_mode = 1)   // assigns 1 to scale_mode, always true
if (state->scale_mode == 1)  // compares, correct
```

The first version silently sets the variable instead of checking it, and always evaluates as true. Enable compiler warnings (`-Wall`) and this will be caught.

### `else (condition)` vs `else if (condition)`

```c
// Wrong:
if (x == 0) { ... }
else (x == 1) { ... }   // this is an else followed by an expression statement — syntax error

// Right:
if (x == 0) { ... }
else if (x == 1) { ... }
```

### Defensive `else` Clauses

When writing if/else if chains over an enumerated value, always include a final `else` to handle out-of-range cases:

```c
if (state->span == 0) { span_width = 128; }
else if (state->span == 1) { span_width = 64; }
else if (state->span == 2) { span_width = 32; }
else if (state->span == 3) { span_width = 16; }
else { span_width = 128; }   // defensive: should never happen, but don't leave undefined
```

Without the final `else`, an out-of-range value leaves `span_width` uninitialized.

---

## Architecture Lessons

### Debounce Timestamps Belong in the Handler, Not the ISR

Early implementation put the debounce timestamp update inside the ISR:

```c
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == BTN_PAUSE_Pin) {
        last_pause_time = HAL_GetTick();   // wrong place
        event_flags |= EVENT_BTN_PAUSE;
    }
}
```

The ISR runs at interrupt time. The main-loop handler runs later. If debounce checking happens in the handler but the timestamp was set in the ISR, you're measuring time from when the interrupt fired, not from when the handler last processed the button. The timing logic becomes inconsistent.

**Correct pattern:** ISR sets the flag only. Handler checks the timestamp, decides whether to act, then updates the timestamp.

### ISRs Should Do the Minimum Possible

An ISR that runs for a long time blocks other interrupts. An ISR that reads sensors, runs calculations, or updates displays will cause timing problems elsewhere in the system.

The rule: ISRs set flags. Main loop handlers do the work.

### event_flags Is the Dispatcher's Input, Not Its State

A common mistake is treating event flags as persistent state — leaving a flag set to record "this thing happened." The flag should be cleared as soon as it's processed. Its only job is to say "handle this now."

---

## Flash/Memory Gotchas

### libm Costs ~15KB

Including `<math.h>` and calling `cosf()`, `sinf()`, or `sqrtf()` links the math library, which adds ~15KB to the binary. On a 32KB device that leaves almost nothing else.

**Solution:** Precompute all trig values you need in Python and store them as `static const float` arrays in flash. The Hanning window and FFT twiddle factors were both handled this way.

### `static const float` Lives in Flash, Not RAM

```c
static const float hanning_window[256] = { ... };   // stays in flash ✓
float hanning_window[256] = { ... };                  // copied to RAM on startup ✗
```

The `static const` qualifier keeps the array in flash (read-only data section). Without it, the startup code copies the array from flash to RAM, consuming 1KB of your 12KB budget for something that never changes.

### CubeMX Deletes Code Outside USER CODE Tags

Any code you write outside the `/* USER CODE BEGIN */` / `/* USER CODE END */` comment blocks will be deleted when CubeMX regenerates the project. This silently removed the EXTI callback function in early development — the code compiled fine, the function just wasn't there at runtime.

**Always put application code inside USER CODE tags.**

---

## Debugging Workflow That Worked

The workflow that caught every bug in this project, in order:

1. **UART printf first.** Before trusting any visual output, confirm the pipeline with prints. `printf("DSP done, peak_mag = %d\n", (int)(peak * 10000));` at every stage boundary.

2. **Register-level inspection second.** When behavior is wrong and prints look correct, check the hardware directly. CubeIDE's memory/register view shows `FPR1`, `IMR1`, `EXTICR`, `FTSR1` live. The hardware is usually configured correctly by CubeMX; the bug is usually in the software response to hardware events.

3. **Visual output last.** Only trust what you see on the display after UART confirms the numbers are correct. A blank or corrupt display might be a display bug, or it might be a DSP bug producing values outside the expected range.

This order prevents spending hours debugging display code when the real problem was a wrong ADC channel configuration three layers deeper.

---

## Things to Do Differently Next Time

- **Scope the MCU to the FFT size needed, not the cheapest available.** The C031's 12KB SRAM was a useful constraint for learning but would be limiting for a real product.
- **Write the event bit definitions in events.h from day one.** The duplicate bit assignment bug (two events sharing the same bit, silently overwriting each other) happened because flags were scattered across files.
- **Commit working states to git before every major change.** Several hours were lost to changes that broke working code with no easy way to diff against the last known-good version.
- **Read the board user manual, not just the chip datasheet.** USART2 vs USART1 is in the NUCLEO user manual. The chip datasheet says nothing about which USART the ST-Link is wired to, because that's a board-level decision.
