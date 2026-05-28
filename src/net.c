#include "net.h"
#include "clock.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int net_init(NetCtx *ctx, uint32_t node_id)
{
    ctx->node_id = node_id;
    ctx->sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctx->sock_fd < 0) return -1;

    int yes = 1;
    setsockopt(ctx->sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(MCAST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ctx->sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        goto fail;

    struct ip_mreq mreq = {0};
    inet_pton(AF_INET, MCAST_GROUP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(ctx->sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    int loop = 0;
    setsockopt(ctx->sock_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    return 0;
fail:
    close(ctx->sock_fd);
    ctx->sock_fd = -1;
    return -1;
}

void net_close(NetCtx *ctx)
{
    if (ctx->sock_fd >= 0) {
        close(ctx->sock_fd);
        ctx->sock_fd = -1;
    }
}

int net_send(NetCtx *ctx, const DrsPacket *pkt)
{
    uint8_t buf[DRS_PKT_SIZE];
    pkt_serialize(pkt, buf);

    struct sockaddr_in dst = {0};
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(MCAST_PORT);
    inet_pton(AF_INET, MCAST_GROUP, &dst.sin_addr);

    ssize_t n = sendto(ctx->sock_fd, buf, DRS_PKT_SIZE, 0,
                       (struct sockaddr *)&dst, sizeof(dst));
    return (n == DRS_PKT_SIZE) ? 0 : -1;
}

int net_recv(NetCtx *ctx, DrsPacket *pkt, int64_t *rx_ts_ns)
{
    uint8_t buf[DRS_PKT_SIZE];
    ssize_t n = recv(ctx->sock_fd, buf, sizeof(buf), MSG_DONTWAIT);
    if (n != DRS_PKT_SIZE) return -1;
    /* Timestamp immediately after recv so T2/T4 stay in CLOCK_MONOTONIC_RAW
     * domain, consistent with T1 and T3 captured via mono_raw_ns(). The RPi 4B
     * NIC has no hardware timestamping, so SO_TIMESTAMPING software fallback
     * would give CLOCK_REALTIME, which corrupts the 4-timestamp offset math. */
    *rx_ts_ns = mono_raw_ns();
    return pkt_deserialize(buf, pkt);
}
