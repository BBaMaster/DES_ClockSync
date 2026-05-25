#pragma once
#include <stdint.h>
#include "protocol.h"

#define MCAST_GROUP "239.192.88.100"
#define MCAST_PORT  47200

typedef struct {
    int sock_fd;
    uint32_t node_id;
} NetCtx;

int  net_init(NetCtx *ctx, uint32_t node_id);
void net_close(NetCtx *ctx);

/* Send packet via multicast. Returns 0 on success. */
int  net_send(NetCtx *ctx, const DrsPacket *pkt);

/* Receive packet. Fills pkt and rx_ts_ns (kernel timestamp or software fallback).
 * Returns 0 on success, -1 on error/invalid packet. */
int  net_recv(NetCtx *ctx, DrsPacket *pkt, int64_t *rx_ts_ns);
