# 05 — Display Driver & SPI DMA

## The ST7735

The ST7735 is a 1.8" TFT LCD, 160×128 pixels, communicating over SPI. Pixels are stored in RGB565 format — 5 bits red, 6 bits green, 5 bits blue — packed into 2 bytes per pixel.

The display has an internal controller that manages its own pixel memory. To draw something, you tell it a rectangular address window (x0, y0, x1, y1), then stream pixel data bytes. The controller writes incoming bytes sequentially into that window, left to right, top to bottom.

## Initialization Sequence

On power-up, the ST7735 needs a specific sequence of commands sent over SPI before it will display anything. This includes:
- Hardware reset (RST pin low → delay → high)
- Software reset command
- Sleep-out command + delay
- Color mode (RGB565 = 16 bits per pixel)
- MADCTL (Memory Access Control) = 0x68 for landscape orientation (160 wide, 128 tall)
- Display-on command

**MADCTL = 0x68:**  
This register controls which direction the controller scans when writing pixel data. Getting this wrong produces a mirrored or portrait-mode display. 0x68 was determined empirically — the datasheet gives the bit fields but the correct value for "landscape, origin top-left" was confirmed by testing.

**No offsets needed:**  
Some ST7735 modules have a pixel offset (the address window doesn't start at 0,0 on the physical panel). This module needed no offset correction — the coordinate system matched the physical pixel layout exactly.

## Drawing Primitives

The driver provides:

```c
ST7735_DrawPixel(x, y, color)
ST7735_DrawLine(x0, y0, x1, y1, color)
ST7735_FillRect(x0, y0, x1, y1, color)   // NOTE: endpoint coords, not width/height
ST7735_DrawRect(x0, y0, x1, y1, color)
ST7735_DrawChar(x, y, c, fg, bg, size)
ST7735_DrawString(x, y, str, fg, bg, size)
```

**Important:** `FillRect` takes endpoint coordinates `(x0, y0, x1, y1)`, not `(x, y, width, height)`. This caused a rendering bug early in development when the wrong convention was assumed.

## The Font

Characters are stored as a `static const uint8_t font5x7[96][5]` array in flash — 96 printable ASCII characters, each stored as 5 bytes representing 5 vertical columns of 7 pixels. Column-major encoding: each byte's bits represent the pixels in one column from top to bottom.

This stores the entire printable ASCII set in 480 bytes of flash, with zero RAM cost.

**Known bug in `ST7735_DrawChar`:**  
The Y coordinate calculation for multi-size rendering is missing a `* size` multiplication, causing characters to render incorrectly at sizes above 1. Size 1 (the only size currently used) is unaffected.

## Why SPI DMA for the Display

The naive approach to spectrum display is: calculate bar height → draw bar → calculate next bar → draw next bar → repeat. This means the CPU is blocked in SPI transfer loops for most of the frame time.

With SPI DMA, the CPU and SPI hardware run in parallel:

```
CPU calculates bar height for column N+1
  while
DMA is transferring column N's pixel data over SPI
```

The CPU is free to do work while bytes are flying out over SPI. This nearly doubles effective throughput.

## The Column-Strip Buffer

A full framebuffer (160 × 128 × 2 bytes = 40,960 bytes) is impossible in 12KB of SRAM. Instead, the display module uses a **192-byte column buffer** — enough for one 128-pixel column × 1.5 bytes per pixel average (it's actually sized for a worst-case column with room for the DMA alignment).

Wait — why 192 and not 128 × 2 = 256? The column buffer holds one vertical strip of pixels. Each pixel is 2 bytes (RGB565). A 128-pixel column = 256 bytes... but we only need one column at a time, and the buffer is sized for the actual rendered height including the frequency axis area at the bottom.

For each of the 160 columns (x = 0 to 159):
1. Calculate the bar height for this column based on the FFT magnitude
2. Fill the column buffer: bar color for pixels below bar height, background color for pixels above
3. Wait for the previous DMA transfer to complete (important — see below)
4. Set the display address window to this column
5. Assert CS, set DC high (data mode), kick off SPI DMA transfer
6. Move to next column immediately — don't wait

## The DMA Handshake: A Concurrency Problem

There's a subtle but critical issue: the CPU writes to `col_buf[]` while the DMA might still be reading from it to send the previous column's data over SPI.

If the CPU writes new pixel data while the DMA is still reading the old data, the DMA sends a mix of old and new bytes. The display shows corrupted columns — partial bars, wrong colors, visual glitches.

The solution is a completion flag:

```c
volatile uint8_t spi_dma_complete = 1;   // starts as "done"

// In the SPI DMA complete callback (ISR):
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    spi_dma_complete = 1;
}

// Before writing the next column:
while (!spi_dma_complete);        // wait for previous transfer to finish
spi_dma_complete = 0;             // mark as in-progress
// now safe to write col_buf and kick off next DMA
```

The `volatile` on `spi_dma_complete` is essential — without it, the compiler might cache the value in a register and the `while` loop would spin forever even after the ISR sets the flag.

This is the same pattern as `event_flags` — ISR sets a flag, main code reads it — just used here for tight synchronization between CPU writes and DMA reads on the same buffer.

## Display Layout

```
┌─────────────────────────────────────────┐  ← y=0
│                                         │
│         Spectrum bars (0–4kHz)          │  ← 112 pixels tall
│         with peak-hold dots             │
│                                         │
├─────────────────────────────────────────┤  ← y=112
│  0Hz        1kHz       2kHz       4kHz  │  ← frequency axis labels
└─────────────────────────────────────────┘  ← y=127
   x=0                               x=159
```

The frequency axis is drawn once at startup and not redrawn every frame (it doesn't change). Only the bar area updates on each DSP cycle.

## SPI Protocol Detail

Every display write follows this sequence:
1. `CS low` — tell the LCD "this is for you"
2. `DC low` — command mode (if sending a command byte)
3. Send command byte(s)
4. `DC high` — data mode (if sending pixel data)
5. Send data byte(s)
6. `CS high` — release the bus

Getting DC timing wrong (setting it high before the command byte is sent, or not setting it at all) was the source of a major early bug where the display showed nothing. The fix was adding explicit `DC_Low()` / `DC_High()` calls around every command/data boundary.
