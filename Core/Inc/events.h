/*
 * events.h
 *
 *  Created on: Jun 9, 2026
 *      Author: Dyllan
 */

#ifndef INC_EVENTS_H_
#define INC_EVENTS_H_

#define EVENT_ADC_READY   (1U << 0)
#define EVENT_BTN_RUN     (1U << 1)
#define EVENT_BTN_SCALE   (1U << 2)
#define EVENT_BTN_SPAN_UP (1U << 3)
#define EVENT_BTN_SPAN_DN (1U << 4)
#define EVENT_DSP_READY   (1U << 5)
#define EVENT_SPI_IDLE    (1U << 6)

#endif /* INC_EVENTS_H_ */
