#ifndef BATCH_PROTOCOL_H
#define BATCH_PROTOCOL_H

#include <stdint.h>

/* 两边都是小端。RK 每次固定读一整帧，用 length 取出有效数据。 */
#define BATCH_MAGIC 0x524B4442u
#define BATCH_MAX_PAYLOAD 1024u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t seq;
    uint32_t length;
    uint32_t crc32;
} batch_hdr_t;

#define BATCH_FRAME_SIZE (sizeof(batch_hdr_t) + BATCH_MAX_PAYLOAD)

static inline uint32_t batch_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    int b;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

/* CRC covers metadata too; crc32 itself is excluded. Protocol v2. */
static inline uint32_t batch_frame_crc(const batch_hdr_t *hdr, const uint8_t *payload)
{
    uint8_t bytes[12 + BATCH_MAX_PAYLOAD];
    uint32_t fields[3] = {hdr->magic, hdr->seq, hdr->length};
    uint32_t i;
    if (hdr->length > BATCH_MAX_PAYLOAD) return 0;
    for (i = 0; i < 12; ++i)
        bytes[i] = (uint8_t)(fields[i / 4] >> ((i % 4) * 8));
    for (i = 0; i < hdr->length; ++i) bytes[12 + i] = payload[i];
    return batch_crc32(bytes, 12 + hdr->length);
}
#endif
