/*
 * Optimized LZ77 decompressor for ARM/AArch64.
 * Replaces the original byte-at-a-time implementation in vfs.c.
 *
 * LZ format: control byte (8 bits, LSB first)
 *   bit=1: literal (1 byte input→output)
 *   bit=0: match (2 bytes: 12-bit position + 4-bit length+3)
 * Ring buffer: 4096 bytes, initial write position 0xFEE
 */

#include <stdint.h>
#include <string.h>

static uint8_t lz_ring[0x1000];

void LZ_UNCOMPRESS_OPT(uint8_t *restrict input, uint32_t input_length,
                       uint8_t *restrict output, uint32_t *output_length) {
    memset(lz_ring, 0, sizeof(lz_ring));

    uint8_t *restrict in_end = input + input_length;
    uint8_t *restrict out_start = output;
    uint32_t rinp = 0xFEE;

    while (input < in_end) {
        uint32_t code = *input++ | 0x100;

        while (code != 1) {
            if (code & 1) {
                /* Literal: copy 1 byte */
                uint8_t b = *input++;
                *output++ = b;
                lz_ring[rinp] = b;
                rinp = (rinp + 1) & 0xFFF;
            } else {
                /* Match: read 2 bytes */
                if (input + 1 >= in_end) goto done;

                uint32_t d = input[0] | (input[1] << 8);
                input += 2;

                uint32_t pos = (d & 0xFF) | ((d >> 4) & 0xF00);
                uint32_t len = ((d >> 8) & 0xF) + 3;

                /* Fast path: no wrap, no overlap → batch copy */
                uint32_t dist = (rinp >= pos) ? (rinp - pos) : (pos - rinp);
                if (len <= 8 &&
                    pos + len <= 0x1000 && rinp + len <= 0x1000 &&
                    dist >= len) {
                    for (uint32_t i = 0; i < len; i++) {
                        uint8_t b = lz_ring[pos + i];
                        output[i] = b;
                        lz_ring[rinp + i] = b;
                    }
                    output += len;
                    rinp = (rinp + len) & 0xFFF;
                } else {
                    /* Slow path: wrap or overlap → sequential copy */
                    while (len--) {
                        uint8_t b = lz_ring[pos];
                        *output++ = b;
                        lz_ring[rinp] = b;
                        pos = (pos + 1) & 0xFFF;
                        rinp = (rinp + 1) & 0xFFF;
                    }
                }
            }
            code >>= 1;
        }
    }

done:
    if (output_length) *output_length = (uint32_t)(output - out_start);
}
