#ifndef SPECTRUM_DISPLAY_H
#define SPECTRUM_DISPLAY_H

#include "app_state.h"
#include "stm32c0xx_hal.h"

void App_Handle_Display(app_state_t *state);

//HAL weak override, fire when SPI DMA Transfer completes
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);

#endif

