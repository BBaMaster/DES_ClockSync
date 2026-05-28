#pragma once
#include <stdint.h>

#define DRS_MAGIC     0x44525354u
#define DRS_VERSION   1
#define DRS_PKT_SIZE  66

#define MSG_ANNOUNCE  0x01
#define MSG_SYNC_REQ  0x02
#define MSG_SYNC_RESP 0x03

#define FLAG_LEADER     (1u << 0)
#define FLAG_HOLDOVER   (1u << 1)
#define FLAG_CALIBRATED (1u << 2)
#define FLAG_FAULT      (1u << 3)

/* Wire layout (66 bytes, big-endian):
 * [0-3]   magic       uint32
 * [4]     version     uint8
 * [5]     msg_type    uint8
 * [6]     flags       uint8
 * [7]     reserved    uint8  (always 0)
 * [8-9]   seq         uint16
 * [10-13] node_id     uint32
 * [14-17] election_term uint32
 * [18-25] t1          uint64
 * [26-33] t2          uint64
 * [34-41] t3          uint64
 * [42-49] t4          uint64
 * [50-53] crc32       uint32  (covers bytes 0-49)
 * [54-65] padding     uint8[12] (always 0)
 */
typedef struct {
    uint32_t magic;
    uint8_t  version;
    uint8_t  msg_type;
    uint8_t  flags;
    uint16_t seq;
    uint32_t node_id;
    uint32_t election_term;
    uint64_t t1;
    uint64_t t2;
    uint64_t t3;
    uint64_t t4;
    uint32_t crc32;
} DrsPacket;

/* Serialize pkt into buf (exactly DRS_PKT_SIZE bytes). CRC computed here. */
void     pkt_serialize(const DrsPacket *pkt, uint8_t buf[DRS_PKT_SIZE]);

/* Deserialize buf into pkt. Returns 0 on success, -1 on magic/version/crc error. */
int      pkt_deserialize(const uint8_t buf[DRS_PKT_SIZE], DrsPacket *pkt);

uint32_t crc32_ieee(const uint8_t *data, uint32_t len);
