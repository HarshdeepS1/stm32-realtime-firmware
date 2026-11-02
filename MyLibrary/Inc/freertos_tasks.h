/*
 * freertos_tasks.h
 *
 *  Created on: Nov 2, 2026
 *      Author: Harshdeep Singh
 */

#ifndef INC_FREERTOS_BLINKY_TASK_H_
#define INC_FREERTOS_BLINKY_TASK_H_

typedef struct {
	GPIO_TypeDef* GPIOx;
	uint32_t Delay;
	uint16_t GPIO_Pin;
} Pack_GPIO_Params_t;



#endif /* INC_FREERTOS_BLINKY_TASK_H_ */
