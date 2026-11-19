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
#include "freertos_tasks.h"
#include "_20250206_cm_not_oil_cooled_can_db.h"
#include <stdbool.h>

extern CAN_HandleTypeDef hcan1;

QueueHandle_t CAN_Command_q = NULL;
QueueHandle_t CAN_Communciation_q = NULL;
QueueHandle_t CAN_Mailbox_q = NULL;
uint8_t inv_counter = 0;

//volatile uint8_t mailbox_empty_flag = 0;

int16_t calculate_torque_from_raw_adc(float raw_val) {
	float pedal_percent = (raw_val - ADC_VALID_MIN) / (ADC_VALID_MAX - ADC_VALID_MIN);

	float target_torque = pedal_percent * PEAK_MOTOR_TORQUE;

	// let's assume for now torque is strictly positive (going forward)
	return _20250206_cm_not_oil_cooled_can_db_m192_command_message_vcu_inv_torque_command_encode(target_torque);
}

//.............................
// Convert using DBC file (raw adc value -> volts (or pedal %) -> torque)
// Then send via CAN at 500	kbps (we use this for now. using 1M bps requires reconfigring PCLK and other peripheral's prescalars.)
// Note we are using the EMRAX228 motor which has a peak motor torque of 240 N*m
//.............................
void vCAN_Inverter_Task(void* params) { // Only Torque for now
	(void)params;

	float raw_adc_val = 0.0;

	for (;;) {

		// Receive raw adc value from main control task
		if (xQueueReceive(CAN_Command_q, &raw_adc_val, portMAX_DELAY) == pdTRUE) {  // Block wait until main control task wakes it up

			// Lets create the command and then pack it (see below)
			struct _20250206_cm_not_oil_cooled_can_db_m192_command_message_t   inv_command;

			// Zero out struct
			_20250206_cm_not_oil_cooled_can_db_m192_command_message_init(&inv_command);

			int16_t torque = calculate_torque_from_raw_adc(raw_adc_val);
			inv_counter = (inv_counter + 1) & 0x0F;

			// Fill in the command message struct now that I have the target torque

			inv_command.vcu_inv_torque_command = torque;
			inv_command.vcu_inv_speed_command = 0;
			inv_command.vcu_inv_direction_command = FORWARD_TORQUE;
			inv_command.vcu_inv_inverter_enable = INVERTER_ON;
			inv_command.vcu_inv_inverter_discharge = DISCHARGE_DISABLE;
			inv_command.vcu_inv_speed_mode_enable = NO_CHANGE_TO_MODE;
			inv_command.vcu_inv_rolling_counter = inv_counter;
			inv_command.vcu_inv_torque_limit_command = 0;

			// Pack struct into CAN frame and send it using one of the three TX mailboxes

			// Note I am using a queue of size three to see which mailbox is open.
			uint32_t pTxMailbox;
			if (xQueueReceive(CAN_Mailbox_q, &pTxMailbox, portMAX_DELAY) == pdTRUE) {
				CAN_TxHeaderTypeDef can_frame;

				can_frame.StdId = _20250206_CM_NOT_OIL_COOLED_CAN_DB_M192_COMMAND_MESSAGE_FRAME_ID;
				can_frame.ExtId = 0;
				can_frame.IDE = CAN_ID_STD;
				can_frame.RTR = CAN_RTR_DATA;
				can_frame.DLC = _20250206_CM_NOT_OIL_COOLED_CAN_DB_M192_COMMAND_MESSAGE_LENGTH;
				can_frame.TransmitGlobalTime = DISABLE;

				// Create the payload buffer
				uint8_t can_payload_buf[_20250206_CM_NOT_OIL_COOLED_CAN_DB_M192_COMMAND_MESSAGE_LENGTH];

				// Check to see if I packed 8 bytes of data
				if (_20250206_cm_not_oil_cooled_can_db_m192_command_message_pack(can_payload_buf, &inv_command, _20250206_CM_NOT_OIL_COOLED_CAN_DB_M192_COMMAND_MESSAGE_LENGTH)
					!= _20250206_CM_NOT_OIL_COOLED_CAN_DB_M192_COMMAND_MESSAGE_LENGTH)
				{
					// Error Handler here
				}


				// Finally send the CAN message
				if (HAL_CAN_AddTxMessage(&hcan1, &can_frame, can_payload_buf, &pTxMailbox) != HAL_OK) {

					// Error Handler here
				}

				/**
				 * 	Stopped here.
				 * 	Write Error Handler. Do the RX part.
				 */
			}

		}
	}
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
