#include <stdint.h>
#include "../../src/bit_writer.h"
#include <assert.h>
#include <stdio.h>

void test1(void){
    size_t cap = 10;
    uint8_t buf[cap];
    bit_writer_t bw;

    bw_init(&bw, buf, cap);

    assert(bw_bytes_written(&bw) == 0);

    uint8_t nbits = 10;
    uint16_t val = (1 << nbits) - 1;

    bw_write_bits(&bw, val, nbits);
    assert(bw_bytes_written(&bw) == 2);
    assert(bw.pending_byte == 3);
    assert(bw.bits_filled == 2);
    printf("Passed test 1.\n");
}



int main(void){
    test1();
    return 0;
}