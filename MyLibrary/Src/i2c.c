/*
 * i2c.c
 *
 *  Created on: Oct 23, 2026
 *      Author: Harshdeep Singh
 */


#include "i2c.h"

extern I2C_HandleTypeDef hi2c1;

// This func casts a net to see what devices are connected to MCU and returns how many are found to be active
// It will put these devices into memory
// Only memory addresses 0x08-0x77 will be tested (the others are forbidden per I2C specifications)
uint8_t      i2c_scan (uint8_t *found, uint8_t max_found) {

	// Check each valid addr using HAL_I2C_IsDeviceReady
	uint8_t count = 0;
	for (uint8_t i=0x08; i<0x78; i++) {
		if (HAL_I2C_IsDeviceReady(&hi2c1, (i << 1U), 1, I2C_TIMEOUT_MS) == HAL_OK) {
			found[count++] = i;
		}
	}

	return count;
}

bus_status_t i2c_probe (uint8_t addr7) {   // BUS_OK if a device ACKs
	if (HAL_I2C_IsDeviceReady(&hi2c1, (addr7 << 1U), 1, I2C_TIMEOUT_MS) == HAL_OK) {
		return BUS_OK;
	}

	return BUS_TIMEOUT;
}

bus_status_t i2c_read_reg (uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len) {
	HAL_StatusTypeDef state = HAL_I2C_Mem_Read(&hi2c1, (addr7 << 1U), reg, I2C_MEMADD_SIZE_8BIT, buf, len, I2C_TIMEOUT_MS);

	if (state == HAL_OK) {
		return BUS_OK;
	}
	else if (state == HAL_ERROR) {
		return BUS_ERROR;
	}

	return BUS_TIMEOUT;

}

bus_status_t i2c_write_reg (uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len) {
	HAL_StatusTypeDef state = HAL_I2C_Mem_Write(&hi2c1, (addr7 << 1U), reg, I2C_MEMADD_SIZE_8BIT, buf, len, I2C_TIMEOUT_MS);

	if (state == HAL_OK) {
		return BUS_OK;
	}
	else if (state == HAL_ERROR) {
		return BUS_ERROR;
	}

	return BUS_TIMEOUT;

}
