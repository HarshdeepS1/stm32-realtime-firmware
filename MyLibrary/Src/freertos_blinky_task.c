/*
 * freertos_tasks.c
 *
 *  Created on: Nov 2, 2026
 *      Author: Harshdeep Singh
 */

#include "FreeRTOS.h"
#include "task.h"
#include <freertos_tasks.h>


static Pack_GPIO_Params_t PC2 = {.GPIOx = GPIOC, .GPIO_Pin = GPIO_PIN_2, .Delay = 500};
static Pack_GPIO_Params_t PC3 = {.GPIOx = GPIOC, .GPIO_Pin = GPIO_PIN_3, .Delay = 1000};
static uint32_t busy_task_delay = 6000;

extern uint32_t get_cycle_count(void);


// FreeRTOS function
static void vToggle_GPIO(void* params) {
	Pack_GPIO_Params_t* config = (Pack_GPIO_Params_t*)params;

	for(;;) { // FreeRTOS starts from here next time
		HAL_GPIO_TogglePin(config->GPIOx,config->GPIO_Pin);
		vTaskDelay(pdMS_TO_TICKS(config->Delay));
	}
}

// FreeRTOS function
static void vBusy_Wait(void* params) { // The function stack is created once only
	uint32_t delay = *(uint32_t*)params;

	uint32_t ms_to_cycles = delay * (SystemCoreClock / 1000); // this can overflow for > 23s

	for(;;) {
		uint32_t start = get_cycle_count();
		while((get_cycle_count() - start) < ms_to_cycles);
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}


void blinky_task_init(void) {
	  xTaskCreate(vToggle_GPIO,
			  	  "toggle_pin_PC2_fast",
				  configMINIMAL_STACK_SIZE, // make the stack size much bigger (measured in words)
				  (void*)&PC2,
				  1,
				  NULL
			  	  );

	  xTaskCreate(vToggle_GPIO,
			  	  "toggle_pin_PC3_slow",
				  configMINIMAL_STACK_SIZE,
				  (void*)&PC3,
				  1,
				  NULL
			  	  );

	//   The function vBusy_Wait had a bug in it
	  xTaskCreate(vBusy_Wait, // in milliseconds
			  	  "task_hog",
				  configMINIMAL_STACK_SIZE,
				  (void*)&busy_task_delay,
				  2,
				  NULL
			  	  );

}



