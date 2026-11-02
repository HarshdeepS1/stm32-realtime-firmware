/*
 * freertos_control_task.c
 *
 *  Created on: Nov 2, 2026
 *      Author: Harshdeep Singh
 */

#include "FreeRTOS.h"
#include "task.h"
#include <freertos_tasks.h>


static vMain_Controller (void) {


}


void main_control_task_init(void) {
	  xTaskCreate(vMain_Controller,
			  	  "vMain_Controller",
				  512,
				  NULL,
				  56,  // Highest priority
				  NULL
			  	  );

}
