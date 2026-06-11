#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>

//this is the shared application state

typedef struct {
	uint8_t running; /* 1 = sampling active, 0 = paused          */
	uint8_t scale_mode; /* 0 = auto-scale, 1 = fixed scale           */
	uint8_t span;  /* frequency span index 0-3                  */
	uint8_t needs_redraw; /* 1 = display should repaint                */
	float peak_pow; /* peak magnitude from DSP, used for scaling */ //holds value from 1e8 to 1e10
	uint32_t frame_count;
}app_state_t;

#endif
