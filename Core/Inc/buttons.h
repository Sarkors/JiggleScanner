#ifndef BUTTONS_H
#define BUTTONS_H

#include "app_state.h"
#include "stm32c0xx_hal.h"

void App_Handle_Btn_RunPause(app_state_t *state);
void App_Handle_Btn_Scale   (app_state_t *state);
void App_Handle_Btn_SpanUp  (app_state_t *state);
void App_Handle_Btn_SpanDn  (app_state_t *state);

/* HAL weak override — called from HAL_GPIO_EXTI_IRQHandler */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin);

#endif /* BUTTONS_H */
