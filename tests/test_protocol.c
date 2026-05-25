#include "test_runner.h"
#include "protocol.h"
#include <string.h>

static void test_round_trip(TestCtx *ctx)
{
    DrsPacket orig = {
        .magic         = DRS_MAGIC,
        .version       = DRS_VERSION,
        .msg_type      = MSG_SYNC_REQ,
        .flags         = FLAG_LEADER | FLAG_CALIBRATED,
        .seq           = 0x1234,
        .node_id       = 0xDEADBEEF,
        .election_term = 7,
        .t1 = 0x0102030405060708ULL,
        .t2 = 0x090A0B0C0D0E0F10ULL,
        .t3 = 0x1112131415161718ULL,
        .t4 = 0x191A1B1C1D1E1F20ULL,
    };

    uint8_t buf[DRS_PKT_SIZE];
    pkt_serialize(&orig, buf);

    DrsPacket got;
    int rc = pkt_deserialize(buf, &got);
    EXPECT(ctx, rc == 0);
    EXPECT(ctx, got.magic         == DRS_MAGIC);
    EXPECT(ctx, got.version       == DRS_VERSION);
    EXPECT(ctx, got.msg_type      == MSG_SYNC_REQ);
    EXPECT(ctx, got.flags         == (FLAG_LEADER | FLAG_CALIBRATED));
    EXPECT(ctx, got.seq           == 0x1234);
    EXPECT(ctx, got.node_id       == 0xDEADBEEF);
    EXPECT(ctx, got.election_term == 7);
    EXPECT(ctx, got.t1 == orig.t1);
    EXPECT(ctx, got.t2 == orig.t2);
    EXPECT(ctx, got.t3 == orig.t3);
    EXPECT(ctx, got.t4 == orig.t4);
}

static void test_packet_size(TestCtx *ctx)
{
    DrsPacket p = { .magic = DRS_MAGIC, .version = DRS_VERSION };
    uint8_t buf[DRS_PKT_SIZE];
    pkt_serialize(&p, buf);
    /* Packet is exactly 64 bytes — just verify no overwrite of sentinel */
    uint8_t sentinel[2] = {0xAB, 0xCD};
    uint8_t full[DRS_PKT_SIZE + 2];
    full[DRS_PKT_SIZE]     = 0xAB;
    full[DRS_PKT_SIZE + 1] = 0xCD;
    pkt_serialize(&p, full);
    EXPECT(ctx, full[DRS_PKT_SIZE]     == 0xAB);
    EXPECT(ctx, full[DRS_PKT_SIZE + 1] == 0xCD);
    (void)sentinel;
}

static void test_bad_magic(TestCtx *ctx)
{
    DrsPacket p = { .magic = DRS_MAGIC, .version = DRS_VERSION,
                    .msg_type = MSG_ANNOUNCE };
    uint8_t buf[DRS_PKT_SIZE];
    pkt_serialize(&p, buf);
    buf[0] ^= 0xFF; /* corrupt magic */
    DrsPacket out;
    EXPECT(ctx, pkt_deserialize(buf, &out) == -1);
}

static void test_bad_crc(TestCtx *ctx)
{
    DrsPacket p = { .magic = DRS_MAGIC, .version = DRS_VERSION,
                    .msg_type = MSG_ANNOUNCE };
    uint8_t buf[DRS_PKT_SIZE];
    pkt_serialize(&p, buf);
    buf[20] ^= 0x01; /* corrupt payload */
    DrsPacket out;
    EXPECT(ctx, pkt_deserialize(buf, &out) == -1);
}

static void test_endian(TestCtx *ctx)
{
    DrsPacket p = {
        .magic    = DRS_MAGIC,
        .version  = DRS_VERSION,
        .msg_type = MSG_ANNOUNCE,
        .node_id  = 0x01020304,
    };
    uint8_t buf[DRS_PKT_SIZE];
    pkt_serialize(&p, buf);
    /* node_id at offset 10, big-endian */
    EXPECT(ctx, buf[10] == 0x01);
    EXPECT(ctx, buf[11] == 0x02);
    EXPECT(ctx, buf[12] == 0x03);
    EXPECT(ctx, buf[13] == 0x04);
}

static void test_padding_zeroed(TestCtx *ctx)
{
    DrsPacket p = { .magic = DRS_MAGIC, .version = DRS_VERSION };
    uint8_t buf[DRS_PKT_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    pkt_serialize(&p, buf);
    for (int i = 54; i < DRS_PKT_SIZE; i++)
        EXPECT(ctx, buf[i] == 0x00);
}

static void test_crc32_known(TestCtx *ctx)
{
    /* CRC32 of "123456789" == 0xCBF43926 */
    const uint8_t data[] = "123456789";
    EXPECT(ctx, crc32_ieee(data, 9) == 0xCBF43926u);
}

void test_protocol_suite(TestCtx *ctx)
{
    printf("--- protocol ---\n");
    test_round_trip(ctx);
    test_packet_size(ctx);
    test_bad_magic(ctx);
    test_bad_crc(ctx);
    test_endian(ctx);
    test_padding_zeroed(ctx);
    test_crc32_known(ctx);
}
