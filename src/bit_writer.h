#ifndef BIT_WRITER_H
#define BIT_WRITER_H
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    uint8_t *buf;
    size_t capacity;
    size_t pos;

    uint8_t pending_byte;
    uint8_t bits_filled;
} bit_writer_t;

void bw_init(bit_writer_t *bw, uint8_t *buf, int capacity);

int bw_write_bits(bit_writer_t *bw, uint16_t value, uint8_t nbits);

int bw_flush(bit_writer_t *bw);

size_t bw_bytes_written(const bit_writer_t *bw);

#endif