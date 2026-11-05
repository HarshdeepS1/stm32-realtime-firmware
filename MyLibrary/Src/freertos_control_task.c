/*
 * freertos_control_task.c
 *
 *  Created on: Nov 2, 2026
 *      Author: Harshdeep Singh
 */

#include "FreeRTOS.h"
#include "task.h"

#include "ring_buffer.h"
#include <freertos_tasks.h>
#include <stdbool.h>

//#include <stdatomic.h>

// 4 State FSM
static FSM_States current_state = IDLE;

// ADC+DMA Buffer
volatile uint16_t adc_buf[NUM_CH];
volatile uint32_t seq;
volatile frame_t current_frame;


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
extern QueueHandle_t CAN_Torque_q; // Used to wake up CAN task to start sending CAN message



static void vMain_Controller (void) {
	uint32_t ulNotificationValue;


	for (;;) {

        // Block and wait for a notification to start the task
        // pdTRUE: Clears the notification count back to 0 on exit (acts like a binary semaphore)
        // portMAX_DELAY: Wait indefinitely until notified
		ulNotificationValue = ulTaskNotifyTake(pdTRUE,portMAX_DELAY);

		if (ulNotificationValue > 0) {

			switch(current_state) {
				case IDLE: // If EXTI is present, move to READ_TO_DRIVE.
			        if (xSemaphoreTake(xIDLESemphr, portMAX_DELAY) == pdTRUE) {
			            // Safe to switch to READY_TO_DRIVE

			        	current_state = READY_TO_DRIVE;
			            xSemaphoreGive(xMySemaphore);

			        }

					break;

				case STOP:
					// Wait for system to completely stop -> send 0 torque CAN message and wait to receive ACK (or
					// wait for some other signal before going to IDLE). Use notifications
					if (stop_system() == OK) {
						current_state = IDLE;
					}
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

// Don't need this function
void start_task(BaseType_t *pxHigherPriorityTaskWoken) { // Will be used by TIM2 to trigger the task
	if (xMain_ControllerHandle != NULL) {
		vTaskNotifyGiveFromISR(xMain_ControllerHandle, &pxHigherPriorityTaskWoken);
	}
}

static bool send_to_MC(void) {
	uint32_t filter_val = 0;

	// Pick the smallest one (conservative one)
	if (current_frame.a > current_frame.b) filter_val = current_frame.b;
	else filter_val = current_frame.a;

	// Push it into the moving average filter
	if (rb_push(&moving_avg_filter, filter_val) == 0) return false;

	// Every 10 samples wake up the CAN task to send the converted value
	if (current_frame.seq_num == 10) {
		//.............................
		// Convert using DBC file (adc -> volts (or pedal %) -> torque)
		//.............................


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

static FSM_Events stop_system() {
	// ...

}



void main_control_task_init(void) {
	ring_init(&moving_avg_filter, moving_avg_buffer, MOV_AVG_FILTER_SIZE);

	xTaskCreate(vMain_Controller,
				  "vMain_Controller",
				  512,
				  NULL,
				  56,  // Highest priority
				  &xMain_ControllerHandle // Attach the task handle
				  );

}

