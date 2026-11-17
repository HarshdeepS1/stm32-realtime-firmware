/*
 * CAN_inverter.c
 *
 *  Created on: Nov 16, 2026
 *      Author: Harshdeep Singh
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

//#include "ring_buffer.h"
#include <freertos_tasks.h>
#include <stdbool.h>

QueueHandle_t CAN_Command_q = NULL;
QueueHandle_t CAN_Communciation_q = NULL;

void vCAN_Inverter_Task(void* params) {
	(void)params;

	//.............................
	// Convert using DBC file (adc -> volts (or pedal %) -> torque)
	//.............................



}


void can_inverter_task_init(void) {


	xTaskCreate(vCAN_Inverter_Task,
				  "vCAN_Inverter_Task",
				  2*configMINIMAL_STACK_SIZE,
				  NULL,
				  configMAX_PRIORITIES,  // highest priority
				  NULL
				  );

}
