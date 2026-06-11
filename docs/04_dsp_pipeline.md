# 04 — DSP Pipeline: ADC → FFT → Spectrum

## What the Pipeline Does

The DSP pipeline takes a buffer of 256 raw ADC samples and produces 128 numbers representing how much energy is present at each frequency from 0Hz to 4kHz. Those 128 numbers then drive the 128-pixel-wide bar display.

This document explains every stage and — more importantly — why each stage exists.

## Stage 1: ADC Sampling at 8kHz

The ADC samples the input signal 8,000 times per second. Each sample is a 12-bit number from 0–4095, representing a voltage from 0V to 3.3V.

**Why 8kHz?**  
This comes from the **Nyquist theorem**: to accurately reconstruct a signal at frequency F, you must sample at least at 2F. We want to display frequencies up to 4kHz, so we need at minimum 8,000 samples per second. Sampling slower than Nyquist causes **aliasing** — low-frequency phantoms that don't exist in the real signal, caused by the math "folding" high frequencies back down into the measured range.

8kHz is the minimum. It's also a round number that maps cleanly to our FFT output: 8kHz / 2 = 4kHz maximum displayable frequency, divided into 128 bins = 31.25Hz per bin.

**Why hardware-triggered via TIM3?**  
FFT math assumes all samples are evenly spaced in time. If the ADC were triggered in software, the spacing would vary by microseconds depending on what else the CPU was doing. Those tiny variations inject phantom frequency peaks and smear real ones across adjacent bins. TIM3 fires at a rock-solid hardware rate regardless of CPU activity.

## Stage 2: DC Offset Removal

The firmware's first step in `App_Handle_DSP()` is computing the mean of the 256 samples and subtracting it from every sample:

```c
uint32_t sum = 0;
for (int i = 0; i < FFT_SIZE; i++) sum += adc_buf[i];
float mean = (float)sum / FFT_SIZE;
for (int i = 0; i < FFT_SIZE; i++) fft_buf[i].r = (float)adc_buf[i] - mean;
```

**Why?**  
The ADC measures 0–3.3V. A DC-biased signal (like our voltage-divided audio input) sits at 1.65V when quiet. That 1.65V bias appears in the FFT as a massive spike at 0Hz (DC bin). It doesn't represent signal content — it's just the bias voltage. Subtracting the mean removes it cleanly. This is also why the analog front-end doesn't need a precision voltage reference: the firmware corrects the bias in software.

## Stage 3: Hanning Window

After DC removal, each sample is multiplied by a corresponding value from the **Hanning window table**:

```c
for (int i = 0; i < FFT_SIZE; i++) {
    fft_buf[i].r *= hanning_window[i];
}
```

**Why?**  
The FFT algorithm assumes the 256-sample buffer is one period of an infinitely repeating signal. In reality, the signal doesn't repeat perfectly — there's almost certainly a discontinuity at the edges of the buffer where the last sample doesn't connect back to the first.

The FFT interprets that edge discontinuity as high-frequency content (a sharp jump looks like a high-frequency signal). This is called **spectral leakage** — energy from a real frequency "leaks" into neighboring bins, smearing the spectrum.

The Hanning window fixes this by multiplying samples near the edges by values close to zero, and samples in the middle by values close to one. It's a smooth bell curve:

```
hanning[i] = 0.5 * (1.0 - cos(2π * i / (N-1)))
```

The edges taper smoothly to zero, so no matter what the signal is doing, the buffer always starts and ends near zero. No discontinuity, no leakage.

**Why a precomputed table?**  
Computing `cos()` requires the C math library (`libm`), which adds ~15KB to the binary. The STM32C031 only has 32KB of flash. Including `libm` would leave almost no room for the rest of the program. Instead, the 256 Hanning values are computed once in Python and stored as a `static const float hanning_window[256]` array in flash. No math library needed at runtime.

## Stage 4: FFT (Cooley-Tukey, 256-point)

The FFT transforms the 256 time-domain samples into 256 complex frequency-domain values.

**The intuition:**  
A Fourier transform asks: "which combination of sine waves at different frequencies adds up to produce this signal?" The FFT is an efficient algorithm for computing that decomposition. For N = 256 samples, a naive DFT takes N² = 65,536 multiply-accumulate operations. The Cooley-Tukey FFT does it in N × log₂(N) = 256 × 8 = 2,048 operations. On a 48MHz Cortex-M0+, that's the difference between a 50ms computation and a ~1ms computation.

**The output:**  
The FFT produces 256 complex numbers (real + imaginary parts). For a real-valued input signal, the output is symmetric — bins 0–127 mirror bins 128–255. Only the first 128 bins contain unique information. Those 128 bins represent frequencies from 0Hz to 4kHz (one bin per 31.25Hz).

**Twiddle factors:**  
The FFT algorithm requires computing sin and cos at specific angles as it recursively combines bins. Again, these would require `libm`. The twiddle factors are precomputed in Python and stored as `static const float` arrays in flash alongside the Hanning table.

## Stage 5: Power Spectrum

The FFT output is complex numbers. The display needs a single magnitude per bin. The power at each bin is:

```c
magnitude[k] = (fft_buf[k].r * fft_buf[k].r) + (fft_buf[k].i * fft_buf[k].i);
```

This is the magnitude squared — intentionally, because computing the actual magnitude would require `sqrtf()`, which is expensive and unnecessary. We're comparing relative power levels across bins, not computing absolute amplitude values. The squared values work just as well for that purpose.

## Stage 6: Normalization and Scaling

Raw FFT power values depend on the signal amplitude and can span many orders of magnitude. Before displaying them as bar heights, they need to be normalized to a 0–1 range mapped to 0–127 pixels.

**Autoscale mode:**  
Find the peak magnitude across all 128 bins, then divide every bin by that peak. The tallest bar always reaches full height. Good for seeing weak signals, but the display shifts constantly as signal levels change.

**Fixed scale mode:**  
Divide by a constant `FIXED_SCALE_POW = 1.0e10f`. This constant is derived from ADC full-scale (4095 counts), the Hanning window's coherent gain (~0.5), and a 256-point FFT. Fixed scale means the bar heights are absolute — a loud signal at full ADC range reaches full screen height, a quiet signal shows short bars. More stable to watch.

A `NOISE_FLOOR_POW` clamp prevents very small values from showing as large bars — anything below the noise floor is clamped to zero. This lets you tune how much noise the display shows during silence.

## Peak Hold

After scaling, the firmware tracks the highest bar height ever seen at each bin in a `peak_hold[128]` array. A small decay value is subtracted from each peak every frame. The result is dots that jump up immediately when a frequency appears, then slowly fall over ~0.8 seconds when it disappears. Classic spectrum analyzer behavior.

## The Complete Numbers

| Parameter | Value | Derivation |
|---|---|---|
| Sample rate | 8,000 Hz | TIM3 TRGO, designed for Nyquist |
| FFT size | 256 points | SRAM constraint |
| Usable bins | 128 | FFT output is symmetric |
| Frequency resolution | 31.25 Hz/bin | 8kHz / 256 |
| Max displayable frequency | 4,000 Hz | Nyquist = sample rate / 2 |
| ADC resolution | 12 bits | 0–4095, 0.806 mV/step |
| Display columns | 160 | ST7735 width (32 columns are screen chrome) |
| Bins per column | ~1 at full span | 128 bins : ~128 data columns |
