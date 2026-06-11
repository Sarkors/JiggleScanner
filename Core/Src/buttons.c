#include "buttons.h"
#include "main.h"       /* pin defines: BTN_RUN_PAUSE_Pin etc   */
#include <stdio.h>      /* printf    */
#include "events.h"
extern volatile uint32_t event_flags;
//we need it so the ISR call back can set event bits

/* Event bit definitions — duplicated from main.c.
 * A cleaner approach would be a shared events.h, which we can add later.
 * For now keep them in sync manually. */


#define BTN_DEBOUNCE_MS 40U
//debounce time, if our button is pressed in between this time from our first press thats just the button going up and down

/* --- Debounce timestamps — private to this file -------------------------
 * Written and read only in HAL_GPIO_EXTI_Falling_Callback (ISR context).
 * volatile because they're read and written in interrupt context. (can change) */
static volatile uint32_t btn_run_last_tick    = 0;
static volatile uint32_t btn_scale_last_tick  = 0;
static volatile uint32_t btn_span_up_last_tick = 0;
static volatile uint32_t btn_span_dn_last_tick = 0;




//multiple EXTI lines share one IRQ vector, GPI_Pin is a bitmask that tells us which pin triggered
//trying to keep Interrupts as short as possible,
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{

	if (GPIO_Pin == BTN_RUN_PAUSE_Pin)  event_flags |= EVENT_BTN_RUN;
	    else if (GPIO_Pin == BTN_SCALE_Pin)      event_flags |= EVENT_BTN_SCALE;
	    else if (GPIO_Pin == BTN_SPAN_UP_Pin)    event_flags |= EVENT_BTN_SPAN_UP;
	    else if (GPIO_Pin == BTN_SPAN_DOWN_Pin)  event_flags |= EVENT_BTN_SPAN_DN;

}



//functions called by the dispatcher in main loop, after interrupt sets bit

//XOR flips bit if on -> off, off -> on LED on if running, off if paused
	 void App_Handle_Btn_RunPause(app_state_t *state)
	{

		 uint32_t now = HAL_GetTick();
		 if((now - btn_run_last_tick) < BTN_DEBOUNCE_MS) return;
		 btn_run_last_tick = now;

	state->running ^= 1;
	HAL_GPIO_WritePin(LED_ACTIVITY_GPIO_Port, LED_ACTIVITY_Pin, state->running ? GPIO_PIN_SET : GPIO_PIN_RESET);

	printf(" BTN: %s\r\n", state->running ? "RUN" : "PAUSE");

	}

	 void App_Handle_Btn_Scale(app_state_t *state)
	{
		 uint32_t now = HAL_GetTick();
		 		 if((now - btn_scale_last_tick) < BTN_DEBOUNCE_MS) return;
		 		btn_scale_last_tick = now;

		state->scale_mode = (state->scale_mode + 1) % 2;  //cycles through 0 -> 1 -> 0 just like x ^= 1
		printf("BTN: scale_mode=%u\r\n", state->scale_mode);
	}

	//span 0 is minimum 3 is the max, span feature not implemented yet
	 void App_Handle_Btn_SpanUp(app_state_t *state)
	{
		 uint32_t now = HAL_GetTick();
		 		 if((now - btn_span_up_last_tick) < BTN_DEBOUNCE_MS) return;
		 		btn_span_up_last_tick = now;
		if ((state->span >= 0) && (state->span < 3)) state->span++;
		printf("BTN: span=%u\r\n", state->span);
		state->needs_redraw = 1;
	}

	 void App_Handle_Btn_SpanDn(app_state_t *state)
		{
		 uint32_t now = HAL_GetTick();
		 		 if((now - btn_span_dn_last_tick) < BTN_DEBOUNCE_MS) return;
		 		btn_span_dn_last_tick = now;
			if (state->span > 0) state->span--;
			printf("BTN: span=%u\r\n", state->span);
			state->needs_redraw = 1;
		}
