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

//#include <stdatomic.h>

// 4 State FSM
static FSM_States current_state = IDLE;

// ADC+DMA Buffer
volatile uint16_t adc_buf[N_FRAMES * NUM_CH];
unsigned int adc_group_sequence[NUM_CH] = {1,4};
volatile Voltages_t converted_voltage_vals[NUM_CH * N_FRAMES];

//volatile atomic_bool adc_conver_ready_buf1 = false;
//volatile atomic_bool adc_conver_ready_buf2 = false;


// Moving Avg Filter
static Ring_Buffer_t moving_avg_filter;
static uint8_t* moving_avg_buffer[MOV_AVG_BUFFER_SIZE];

// Fault Monitor and Flags
static uint8_t implausibility_tick = 0;
//atomic_bool ready_to_drive = false;

// Mutexes, Notifications, Semph
TaskHandle_t xMain_ControllerHandle = NULL;
extern SemaphoreHandle_t xCAN_TorqueSemphr; // Used to wake up CAN task to start sending CAN message
SemaphoreHandle_t xIDLESemphr = NULL;
SemaphoreHandle_t xADC_ConvSemphr1 = NULL;
SemaphoreHandle_t xADC_ConvSemphr2 = NULL;



static vMain_Controller (void) {
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
						break;
					}
					else {
						// Put into moving average filter, wake up CAN task every 10 samples
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

static FSM_Events check_inputs() { // Read ADC values, check for implausiblity
	if (implausibility_tick > 100) return FAILURE;

	// Convert ADC values to voltages
	// If I don't get a value within 1ms, the ADC is not scanning
	if (xSemaphoreTake(xADC_ConvSemphr1, pdMS_TO_TICKS(1)) == pdTRUE) {

		if (converted_voltage_vals[0].voltage > converted_voltage_vals[1].voltage) {
			if ((converted_voltage_vals[0].voltage - converted_voltage_vals[1].voltage) > (0.1 * ADC_INPUT_VOLT)) {
				implausibility_tick++;
			}

		}


		xSemaphoreGive(xADC_ConvSemphr1);

	}
	else return FAILURE;

	// Wait for 2nd half of DMA buffer to be finished populating
	// If i don't get values within 1ms, then ADC stopped halfway
	if (xSemaphoreTake(xADC_ConvSemphr2, pdMS_TO_TICKS(1)) == pdTRUE) {



		xSemaphoreGive(xADC_ConvSemphr2);

	}
	else return FAILURE;

	if


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

