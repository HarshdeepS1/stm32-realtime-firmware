/*
 * ring_buffer.h
 *
 *  Created on: Oct 14, 2026
 *      Author: Harshdeep Singh
 */

#ifndef MYLIBRARY_INC_RING_BUFFER_H_
#define MYLIBRARY_INC_RING_BUFFER_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct ring_buffer_tag {
	volatile unsigned int head;
	volatile unsigned int tail;
	unsigned int size; //remember size has to be power of 2
	uint32_t* buffer;
} Ring_Buffer_t;

void     rb_init (Ring_Buffer_t *r, uint8_t *storage, uint32_t size);
bool     rb_push (Ring_Buffer_t *r, uint8_t byte);
bool     rb_pop  (Ring_Buffer_t *r, uint8_t *byte);
uint32_t rb_count(const Ring_Buffer_t *r);

#endif /* MYLIBRARY_INC_RING_BUFFER_H_ */
