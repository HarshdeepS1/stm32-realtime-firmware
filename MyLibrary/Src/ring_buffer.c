/*
 * ring_buffer.c
 *
 *  Created on: Oct 14, 2026
 *      Author: Harshdeep Singh
 */

#include "ring_buffer.h"

// Producer -> head, consumer- > tail
void     rb_init (Ring_Buffer_t *r, uint8_t *storage, uint32_t size) {
	r->size = size;
	r->buffer = storage;
	r->head = 0;
	r->tail = 0;
}

bool     rb_push (Ring_Buffer_t *r, uint8_t byte) {   // false if full, increment head
	if (r->head - r->tail == r->size) return 0; // need to read tail index

	r->buffer[(r->head) & (r->size - 1)] = byte; // remember size has to be power of 2, same as doing modulo

	++(r->head);

	return 1;
}


bool     rb_pop  (Ring_Buffer_t *r, uint8_t *byte){  // false if empty
	if (r->head == r->tail) return 0; // need to read head index

	*byte = r->buffer[(r->tail) & (r->size - 1)];

	++(r->tail);

	return 1;

}

uint32_t rb_count(const Ring_Buffer_t *r) {
	return r->head - r->tail; // naturally wraps around (finds absolute distance between the indices)
}
