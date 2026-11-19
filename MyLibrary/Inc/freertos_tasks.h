/*
 * freertos_tasks.h
 *
 *  Created on: Nov 2, 2026
 *      Author: Harshdeep Singh
 */

#ifndef INC_FREERTOS_BLINKY_TASK_H_
#define INC_FREERTOS_BLINKY_TASK_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

// For ADC+DMA Buffering
#define NUM_CH   					2
#define N_FRAMES 					2 // Should always be
#define ADC_INPUT_VOLT  			3.3
#define ADC_VALID_RANGE_MIN			0.5
#define ADC_VALID_RANGE_MAX			3.0

#define ADC_MAX_VALUE				4095
#define IMPLAUSIBILITY_THRESHOLD	(unsigned int)(0.1 * (double)ADC_MAX_VALUE)
#define ADC_VALID_MIN				(unsigned int)((ADC_VALID_RANGE_MIN / ADC_INPUT_VOLT) * ADC_MAX_VALUE)
#define ADC_VALID_MAX				(unsigned int)((ADC_VALID_RANGE_MAX / ADC_INPUT_VOLT) * ADC_MAX_VALUE)

typedef struct {
	uint32_t seq_num;
	uint32_t t_cycles;
	uint16_t a,b; // ADC channels a and b, they give raw ADC values
} frame_t;







// Moving Average Filter
#define MOV_AVG_BUFFER_SIZE 16 // has to be a power of 2 (the actual buffer size)
#define MOV_AVG_FILTER_SIZE 10






// For the blinky tasks (or if trying to pass GPIO info to tasks)
typedef struct {
	GPIO_TypeDef* GPIOx;
	uint32_t Delay;
	uint16_t GPIO_Pin;
} Pack_GPIO_Params_t;









// State machine (4-state FSM)
typedef enum {
	STOP,
	IDLE,
	READY_TO_DRIVE,
	DRIVE,
	NUM_OF_STATES,
} FSM_States;

typedef enum {
	EMERGENCY,
	FAILURE,
	OK,
} FSM_Events;


#endif /* INC_FREERTOS_BLINKY_TASK_H_ */
