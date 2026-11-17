#ifndef UART_INTERRUPT_DRIVER
#define UART_INTERRUPT_DRIVER

#include <stdint.h>

#define BUFFER_SIZE 32 // Make sure power of 2

typedef struct ring_buffer_tag Ring_Buffer_t;

void uart_irq_handler(void);
unsigned int uart_write(Ring_Buffer_t* txr, uint8_t* arr, uint32_t size);
void uart_read(Ring_Buffer_t* rxr, uint32_t* data);
void enable_uart_rx_interrupt(void);



#endif /* UART_INTERRUPT_DRIVER */
