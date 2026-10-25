/*
 * i2c.h
 *
 *  Created on: Oct 23, 2026
 *      Author: Harshdeep Singh
 */

#ifndef INC_I2C_H_
#define INC_I2C_H_

#include <stdint.h>
#include "stm32f4xx_hal.h" // put this file in header if using HAL funcs in source


#define I2C_TIMEOUT_MS 10
#define I2C_NUM_VALID_ADDR 70

typedef enum {
	BUS_OK = 0,
	BUS_NACK,
	BUS_ERROR,
	BUS_TIMEOUT,
	BUS_PARAM
} bus_status_t;

bus_status_t i2c_read_reg (uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len);
bus_status_t i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len);
bus_status_t i2c_probe    (uint8_t addr7);   // BUS_OK if a device ACKs
uint8_t      i2c_scan     (uint8_t *found, uint8_t max_found); // returns count

#endif /* INC_I2C_H_ */
