/*
 * uart_driver.c
 * Interrupt driver uart
 *  Created on: Oct 14, 2026
 *      Author: Harshdeep Singh
 *      Make sure source file matches header file
 */

#include "uart_driver.h"
#include "ring_buffer.h"
#include "stm32f4xx.h"

// Could put global variables in another file, so multiple files can access them
extern Ring_Buffer_t tx_buffer;
extern Ring_Buffer_t rx_buffer;

extern volatile bool RX_receive_ready;
extern volatile bool RX_overrun_error;
extern volatile bool RX_buffer_overflow;

void uart_irq_handler(void) {
	// Check if TXEIE is on -> if tx buffer is not empty, do TX interrupt
	if ((USART2->CR1 & USART_CR1_TXEIE) && (USART2->SR & USART_SR_TXE)) { // TX interrupt will happen whenever TXE = 1 and TXEIE = 1


		uint32_t data; // Data will be popped from stack but its ok since it will be copied into DR

		if (rb_count(&tx_buffer) != 0) { // As long as TX buffer is not empty

//			// Checking to see data has been sent to shift register
//			while (!(USART2->SR & USART_SR_TXE)) {}	// USART peripheral is volatile

			rb_pop(&tx_buffer,(uint8_t*)&data);
			USART2->DR = (data & USART_DR_DR); // Put into data register to be shifted out, MSB of DR sets parity

		}
		else { // TX buffer is empty so disable interrupt
			USART2->CR1 &= ~USART_CR1_TXEIE; // disable TXEIE
		}

	}

	// If ready to read, push into rx buffer. Note RX interrupt is always enabled
	if (USART2->SR & USART_SR_RXNE) { // BUG HERE 10/18: I used CR1 instead of SR
		USART2->SR &= ~(USART_SR_RXNE); // Clear RXNE flag

		uint8_t received_data = ((uint8_t)USART2->DR);

		if(rb_count(&rx_buffer) <= rx_buffer.size) { // If receive buffer is full, throw error

			rb_push(&rx_buffer, received_data);

			__disable_irq();
			RX_receive_ready = true;
			__enable_irq();

		}
		else {
			__disable_irq();
			RX_buffer_overflow = true;
			__enable_irq();
		}

	}

	// If overrun error occurs, read to the USART_SR register followed
	// by a read to the USART_DR register to clear it.
	// Note RX interrupt is always enabled.
	if (USART2->SR & USART_SR_ORE) {
		USART2->SR;
		USART2->DR;

		__disable_irq();
		RX_overrun_error = true;
		__enable_irq();
	}
}

unsigned int uart_write(Ring_Buffer_t* txr, uint8_t* arr, uint32_t size) {
	unsigned int data_sent = 0;

	for (size_t i=0; i<size; i++) { // may have to take into account race conditions
		if (rb_count(txr) == txr->size) { // there is no more empty space
			break;
		}
		rb_push(txr, arr[i]);
		data_sent++;
	}

	// Need to enable TX interrupt
	__disable_irq();
	USART2->CR1 |= USART_CR1_TXEIE;
	__enable_irq();

	return data_sent;
}

void uart_read(Ring_Buffer_t* rxr, uint8_t* data) {
	// Make sure to enable RX interrupt in the main code
	rb_pop(rxr, data);
}

void enable_uart_rx_interrupt(void) {
	__disable_irq();
	USART2->CR1 |= USART_CR1_RXNEIE;
	__enable_irq();
}


