//spectrum bar rendering via SPI DMA


#include "spectrum_display.h"
#include "ST7735screen.h"
#include "dsp.h"             /* magnitude[], FFT_BINS  */
#include "main.h"            /* hspi1                  */
#include <string.h>          /* not used yet but handy */
#include <stdio.h>
#include "events.h"
//geometry of screen
//spectrum display gemometry
#define SPEC_Y_TOP 20u   //first row of spectrum area
#define SPEC_Y_BOT 115u  //last row
#define SPEC_HEIGHT 96u //spec_y_bot - spec_y_top + 1 pxel
#define SPEC_COLS 160u // one column per screen pixel


#define COL_BUF_BYTES (SPEC_HEIGHT * 2u) //column buffer in bytes, one DMA transfer sends one vertical strip

//peakhold time and decay rate
#define PEAK_HOLD_FRAMES 25 //frames to hold before decaying
#define PEAK_DECAY_RATE 1


extern SPI_HandleTypeDef hspi1;
extern volatile uint32_t event_flags;



//96 pix * 2 bytes = 192bytes fits in uint8_t
//state file scope so array lives for entire program lifetime
 //DMA reads from here while CPU calc next col
static uint8_t col_buf[COL_BUF_BYTES];


//SPI DMA state flag
//written in spi call back, read in app_handle_display
 //1 DMA transfer in progress ... 0 DMA idle safe to write col_buf start next transfer
// says that magnitude[] has fresh data for display handler
static volatile uint8_t spi_dma_busy = 0;


static uint8_t peak_hold[SPEC_COLS];
static uint8_t peak_hold_timer[SPEC_COLS];


//HAL callback, when spi finish filling in column
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi->Instance == SPI1)
	{
		LCD_CS_High();
		spi_dma_busy = 0;
		event_flags |= EVENT_SPI_IDLE;

	}
}



 void App_Handle_Display(app_state_t *state) {




		  //spectrum bar display
		  //we map 127 FFT bins to 160 LCD pixels
		  //bar_h is in range of 0 to 95  vertical

		  //this way tallest bin always fills display weaker bin relative to tallest
		  //tune noise floor pow, raise to suppress more noise, lower it to see more noise


		  //float effective_peak = (peak_pow > NOISE_FLOOR_POW) ? peak_pow : NOISE_FLOOR_POW;
	 float scale;
	 if(state->scale_mode == 0)
	 {
		  scale = 95.0f / state->peak_pow; //if scale event_flag == 0, scale based on peak_pow
	 } else
	 {
		  scale = 95.0f / FIXED_SCALE_POW;  //else have a fixed scale

	 }

	 	 	 	  int bin_start = 1;
				  int bin_end;

				  if (state->span ==  0) {
					  //spread bins 1 through 127 across 160px full nyquist range 0-4kHz
					  bin_end = 127;

				  } else if(state->span == 1)
				  {

					  bin_end = 64;  // 0 - 2kHZ
				  }
				  else if(state->span == 2)
				  {
					  bin_end = 32; // 0 - 1 kHz
				  }
				  else
				  {

					  bin_end = 16;   // 0-500 Hz
				  }


		  for (int x = 0; x < (int)SPEC_COLS; x++)
		  {


			  int span_width = bin_end - bin_start; //how many bins to spread

			  // when state->span = 0, bin_start 1, bin_end 127, width = 126 ... x = 0 -> bin = 1 + (0 *126)/159 = 1 (31 Hz)
			  // x = 1259 -> bin = 1 + (159 * 126)/159 = 127 (3969 Hz)
			  int bin = bin_start + (x * span_width) / (SPEC_COLS - 1);

			  //safety, division can produce bin_end exactly, cap at bin_end - 1
			  if (bin >= bin_end) bin = bin_end - 1;

			  float power = magnitude[bin];  //power = re{k}^2 + im{k}^2
			  //scaling the bar_h
			  int bar_h = (int)(power * scale);
			  if (bar_h > 95) bar_h = 95;
			  if (bar_h < 0) bar_h = 0;


			  //if this frames bar is taller than the stored peak , reset peak else count down the hold timer and start decaying
			  if ((uint8_t)bar_h >= peak_hold[x])
			  {
				  peak_hold[x] = (uint8_t)bar_h;
				  peak_hold_timer[x] = PEAK_HOLD_FRAMES;
			  }
			  else
			  {
				  if (peak_hold_timer[x] > 0)
				  {
					  peak_hold_timer[x]--; //still holding dont decay
				  }
				  else if (peak_hold[x] > 0) //hold ended start decaying
				  {
					  peak_hold[x] -= PEAK_DECAY_RATE;
				  }
			  }

			  //col_buf holds SPEC_HEIGHT pixels for one colun top to bottom
			  //top section bg... bottom section bar color... peak dot one white pixel where the peak hold is
			  uint8_t bar_hi = ST7735_GREEN >> 8;
			  uint8_t bar_lo = ST7735_GREEN & 0XFF;
			  uint8_t bg_hi = ST7735_BLACK >> 8;
			  uint8_t bg_lo = ST7735_BLACK & 0XFF;
			  uint8_t pk_hi = ST7735_WHITE >> 8;
			  uint8_t pk_lo = ST7735_WHITE & 0XFF;

              //SPEC_HEIGHT screen column height,
			  uint8_t empty = (uint8_t)(SPEC_HEIGHT - (uint8_t)bar_h);
			  int dot_row = (int)SPEC_HEIGHT - 1 - (int) peak_hold[x];

			  for (int row=0; row < (int)SPEC_HEIGHT; row++)
			  {
				  uint8_t hi, lo;
				  if (row == dot_row && peak_hold[x] > (uint8_t)bar_h)
				  {
					  hi = pk_hi; lo = pk_lo; //peak hold dot
				  }
				  else if (row < (int)empty)
				  {
					  hi = bg_hi; lo = bg_lo; //baackground above bar
				  }
				  else
				  {
					  hi = bar_hi; lo = bar_lo; //bar
				  }
				  col_buf[row *2] = hi;
				  col_buf[row *2 + 1] = lo;
			  }

			  //buffer hazard gaurd, if prev col DMA still reading col_buf dont overwrite yet
			  //dma asynchronus, spinning on flag rather than checking once
			  while (spi_dma_busy) { }

			  //setting window for this column and start dma
				  ST7735_SetWindow(x, SPEC_Y_TOP, x, SPEC_Y_BOT);

				  spi_dma_busy = 1;

				  ST7735_StartColumnDMA();
				  HAL_SPI_Transmit_DMA(&hspi1, col_buf, COL_BUF_BYTES); //returns immediately after setting up DMA
				  //we raise CS in the call back because DMA still sending
		  }
		  //waiting for the last column DMA to finish before drawing freq axis overlay,
		  while (spi_dma_busy) {}

		  char status_buf[32];
		  		  snprintf(status_buf, sizeof(status_buf), "sample count:%lu", state->frame_count);  //writes data into a char buffer
		  		  ST7735_DrawString(10, 10, status_buf, ST7735_YELLOW, ST7735_BLACK, 1);
		  		  if (state->needs_redraw)
		  		  {
		  		  Draw_FreqAxis(state->span);
		  		  state->needs_redraw = 0;
		  		  }
}
