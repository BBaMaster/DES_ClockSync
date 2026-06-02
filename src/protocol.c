#include "protocol.h"
#include <string.h>
#include <arpa/inet.h>

static uint32_t s_crc_table[256];
static int      s_crc_ready;

static void crc_table_init(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        s_crc_table[i] = c;
    }
    s_crc_ready = 1;
}

uint32_t crc32_ieee(const uint8_t *data, uint32_t len)
{
    if (!s_crc_ready) crc_table_init();
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++)
        crc = s_crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static void put64be(uint8_t *b, uint64_t v)
{
    b[0] = (uint8_t)(v >> 56);
    b[1] = (uint8_t)(v >> 48);
    b[2] = (uint8_t)(v >> 40);
    b[3] = (uint8_t)(v >> 32);
    b[4] = (uint8_t)(v >> 24);
    b[5] = (uint8_t)(v >> 16);
    b[6] = (uint8_t)(v >>  8);
    b[7] = (uint8_t)(v      );
}

static uint64_t get64be(const uint8_t *b)
{
    return ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
           ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
           ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
           ((uint64_t)b[6] <<  8) |  (uint64_t)b[7];
}

void pkt_serialize(const DrsPacket *pkt, uint8_t buf[DRS_PKT_SIZE])
{
    memset(buf, 0, DRS_PKT_SIZE);

    uint32_t magic_be = htonl(pkt->magic);
    memcpy(buf + 0, &magic_be, 4);
    buf[4] = pkt->version;
    buf[5] = pkt->msg_type;
    buf[6] = pkt->flags;
    /* buf[7] = 0 (reserved) */
    uint16_t seq_be = htons(pkt->seq);
    memcpy(buf + 8, &seq_be, 2);
    uint32_t nid_be = htonl(pkt->node_id);
    memcpy(buf + 10, &nid_be, 4);
    uint32_t term_be = htonl(pkt->election_term);
    memcpy(buf + 14, &term_be, 4);
    put64be(buf + 18, pkt->t1);
    put64be(buf + 26, pkt->t2);
    put64be(buf + 34, pkt->t3);
    put64be(buf + 42, pkt->t4);
    /* bytes 54-63: padding, already zeroed */

    uint32_t crc = crc32_ieee(buf, DRS_PKT_SIZE);
    uint32_t crc_be = htonl(crc);
    memcpy(buf + 50, &crc_be, 4);
}

int pkt_deserialize(const uint8_t buf[DRS_PKT_SIZE], DrsPacket *pkt)
{
    uint32_t magic_be;
    memcpy(&magic_be, buf, 4);
    if (ntohl(magic_be) != DRS_MAGIC) return -1;
    if (buf[4] != DRS_VERSION)        return -1;

    uint32_t crc_be;
    memcpy(&crc_be, buf + 50, 4);
    uint8_t tmp[DRS_PKT_SIZE];
    memcpy(tmp, buf, DRS_PKT_SIZE);
    memset(tmp + 50, 0, 4);
    if (crc32_ieee(tmp, DRS_PKT_SIZE) != ntohl(crc_be)) return -1;

    pkt->magic         = DRS_MAGIC;
    pkt->version       = buf[4];
    pkt->msg_type      = buf[5];
    pkt->flags         = buf[6];
    uint16_t seq_be;
    memcpy(&seq_be, buf + 8, 2);
    pkt->seq = ntohs(seq_be);
    uint32_t nid_be;
    memcpy(&nid_be, buf + 10, 4);
    pkt->node_id = ntohl(nid_be);
    uint32_t term_be;
    memcpy(&term_be, buf + 14, 4);
    pkt->election_term = ntohl(term_be);
    pkt->t1   = get64be(buf + 18);
    pkt->t2   = get64be(buf + 26);
    pkt->t3   = get64be(buf + 34);
    pkt->t4   = get64be(buf + 42);
    pkt->crc32 = ntohl(crc_be);
    return 0;
}
