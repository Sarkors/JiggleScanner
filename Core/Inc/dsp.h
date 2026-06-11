#ifndef DSP_H
#define DSP_H

#include "app_state.h"
#include <stdint.h>

#define ADC_BUF_LEN 256 //adc buffer length, number of ADC samples per frame
#define ADC_FS_HZ 8000 //sampling rate


//magnitude has one entry per usable FFT bin
#define FFT_BINS 128

#define FIXED_SCALE_POW  1e10f
#define NOISE_FLOOR_POW 1.9e8f

//magnitude[] written by App_handle_dsp and read by app_handle_display
//spectrum_display needs this so we declare it as extern
extern float magnitude[FFT_BINS];


void App_Handle_DSP(app_state_t *state);

#endif
