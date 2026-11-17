/*
 * freertos_control_task.c
 *
 *  Created on: Nov 2, 2026
 *      Author: Harshdeep Singh
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "ring_buffer.h"
#include <freertos_tasks.h>
#include <stdbool.h>
#include <stdlib.h>
//#include <stdatomic.h>

// 4 State FSM
static FSM_States current_state = IDLE;

// ADC+DMA Buffer
volatile uint16_t adc_buf[NUM_CH];
volatile uint32_t seq;
frame_t current_frame;


// Moving Avg Filter
static Ring_Buffer_t moving_avg_filter;
static uint32_t moving_avg_buffer[MOV_AVG_BUFFER_SIZE];

// Fault Monitor and Flags
static uint8_t implausibility_tick = 0;
//atomic_bool ready_to_drive = false;

// Mutexes, Notifications, Semph
TaskHandle_t xMain_ControllerHandle = NULL;
SemaphoreHandle_t xIDLESemphr = NULL;
QueueHandle_t frame_q = NULL;

extern QueueHandle_t CAN_Command_q; // Used to wake up CAN task to start sending CAN message
extern QueueHandle_t CAN_Communciation_q;



static void send_to_MC(void) {
	uint32_t filter_val = 0;

	// Pick the smallest one (conservative one)
	if (current_frame.a > current_frame.b) filter_val = current_frame.b;
	else filter_val = current_frame.a;

	// Push it into the moving average filter
	if (rb_count(&moving_avg_filter) == MOV_AVG_FILTER_SIZE) {
		uint32_t temp;
		rb_pop(&moving_avg_filter, &temp);
	}
	rb_push(&moving_avg_filter, filter_val);


//	Every 10 samples (10 ms), calculate the moving avg and send value to CAN task via queue
	if (current_frame.seq_num % 10 == 0) {
		// Note we are using the CM200DZ inverter

		// Perform moving avg calculation
		float average = (float)rb_sum_elements(&moving_avg_filter) / (float)MOV_AVG_FILTER_SIZE;

		// Wake up CAN task
		xQueueOverwrite(CAN_Command_q, &average);

	}

}



static FSM_Events check_inputs() { // Read ADC values, check for implausiblity

	// If I don't get a value within 2ms, the ADC is not scanning
	if (xQueueReceive(frame_q, &current_frame, pdMS_TO_TICKS(2)) == pdTRUE) {

		// Check if missed frame
		if (current_frame.seq_num != seq) return FAILURE;

		// Implement short/open circuit check (values must be in between 0.5 - 3.0 V)
		// Note the values are raw ADC values
		if (!((620 < current_frame.a && current_frame.a < 3722) &&
			  (620 < current_frame.b && current_frame.b < 3722)))
		{
			return FAILURE;
		}


		// Check for 10% implausibility (note absolute 10% is 0.33V, which corresponds to an ADC value of 409)

		if (abs(current_frame.a - current_frame.b) > IMPLAUSIBILITY_THRESHOLD) {
			implausibility_tick++;
		}
		else {
			implausibility_tick = 0;
		}

	}
	else return FAILURE;

	// Implausibility check
	if (implausibility_tick > 100) return FAILURE;

	return OK;


}

// Keep sending 0 torque commands until an ACK is received. Then reset system (clear moving_avg_buffer and
// zero out implausibility_tick).
static void stop_system() {
	float zero_torque = 0.0;
	FSM_Events state;

	// If I don't get a response from the CAN task within 2 ms, resend the command.
	while (1) {
		xQueueOverwrite(CAN_Command_q, &zero_torque);
		if (xQueueReceive(CAN_Communciation_q, &state, pdMS_TO_TICKS(2)) == pdTRUE) {
			break;
		}
	}

	implausibility_tick = 0;
	rb_reset_buffer(&moving_avg_filter);
}




static void vMain_Controller (void* params) {
	(void)params;

	uint32_t ulNotificationValue;


	for (;;) {

        // Block and wait for a notification to start the task
        // pdTRUE: Clears the notification count back to 0 on exit (acts like a binary semaphore). Returns the value before the adjustment.
        // portMAX_DELAY: Wait indefinitely until notified
		ulNotificationValue = ulTaskNotifyTake(pdTRUE,portMAX_DELAY);

		if (ulNotificationValue > 0) {

			switch(current_state) {
				case IDLE: // If EXTI interrupt is present, move to READ_TO_DRIVE.
			        if (xSemaphoreTake(xIDLESemphr, portMAX_DELAY) == pdTRUE) {
			            // Safe to switch to READY_TO_DRIVE

			        	current_state = READY_TO_DRIVE;
			            xSemaphoreGive(xIDLESemphr);

			        }

					break;

				case STOP:
					// Wait for system to completely stop -> send 0 torque CAN message and wait to receive ACK (or
					// wait for some other signal before going to IDLE).
					stop_system();
					current_state = IDLE;

					break;

				case READY_TO_DRIVE:
					// Check some signals, like BMS voltage level, implement later...
					current_state = DRIVE;
					break;

				case DRIVE:
					if (check_inputs() == FAILURE) {
						current_state = STOP;
					}
					else {
						// Put into moving average filter, wake up CAN task every 10 samples
						send_to_MC();
					}
					// If everything is done, yield to other tasks (start at the for loop from the very beginning)
					break;

				default:
					// Safety catch
					break;
			}
		}
	}

}



void main_control_task_init(void) {
	rb_init(&moving_avg_filter, moving_avg_buffer, MOV_AVG_FILTER_SIZE);

	xTaskCreate(vMain_Controller,
				  "vMain_Controller",
				  4*configMINIMAL_STACK_SIZE,
				  NULL,
				  configMAX_PRIORITIES,  // Highest priority
				  &xMain_ControllerHandle // Attach the task handle
				  );

}

