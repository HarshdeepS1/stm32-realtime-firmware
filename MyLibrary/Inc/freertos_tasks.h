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
#define NUM_CH   2
#define N_FRAMES 2

// Moving Average Filter
#define MOV_AVG_BUFFER_SIZE 16 // has to be a power of 2
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
	FAILURE,
	OK,
} FSM_Events;

// For the ADC calculations
typedef struct {
	int integer_part;
	int fractional_part;
} Voltages_t;


#endif /* INC_FREERTOS_BLINKY_TASK_H_ */
