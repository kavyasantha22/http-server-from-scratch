#include "bit_writer.h"

void bw_init(bit_writer_t *bw, uint8_t *buf, int capacity){
    bw->buf = buf;
    bw->capacity = capacity;
    bw->pos = 0;
    bw->pending_byte = 0;
    bw->bits_filled = 0;
}

int bw_write_bits(bit_writer_t *bw, uint16_t value, uint8_t nbits){
    uint8_t cur_filled;
    uint8_t cur_written;
    uint8_t bits_written = 0;

    while (nbits > 8 - bw->bits_filled){
        if (bw->pos == bw->capacity) return bits_written;
        cur_filled = 8 - bw->bits_filled;
        cur_written = (value & ((1 << cur_filled) - 1));

        bw->pending_byte |= (cur_written << bw->bits_filled);
        bw->bits_filled = 8;
        bw_flush(bw);

        value = value >> cur_filled;
        nbits -= cur_filled;

        bits_written += cur_filled;
    }

    bw->pending_byte = bw->pending_byte | (value << bw->bits_filled);
    bw->bits_filled += nbits;
    bits_written += nbits;

    return bits_written;
}

int bw_flush(bit_writer_t *bw){
    if (bw->bits_filled == 0) return 0;

    bw->buf[bw->pos] = bw->pending_byte;
    bw->pos++;
    uint8_t bits_written = bw->bits_filled;
    bw->pending_byte = 0;
    bw->bits_filled = 0;
    return bits_written;
}

size_t bw_bytes_written(const bit_writer_t *bw){
    return bw->pos + (bw->bits_filled > 0);
}


