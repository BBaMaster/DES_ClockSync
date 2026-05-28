#include "net.h"
#include "clock.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef PLATFORM_rpi
#include <linux/net_tstamp.h>
#endif

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

#ifdef PLATFORM_rpi
    int flags = SOF_TIMESTAMPING_RX_HARDWARE |
                SOF_TIMESTAMPING_RX_SOFTWARE |
                SOF_TIMESTAMPING_SOFTWARE;
    setsockopt(ctx->sock_fd, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof(flags));
#endif

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

#ifdef PLATFORM_rpi
    char cmsg_buf[256];
    struct iovec iov = { .iov_base = buf, .iov_len = sizeof(buf) };
    struct msghdr msg = {
        .msg_iov        = &iov,
        .msg_iovlen     = 1,
        .msg_control    = cmsg_buf,
        .msg_controllen = sizeof(cmsg_buf),
    };
    ssize_t n = recvmsg(ctx->sock_fd, &msg, MSG_DONTWAIT);
    if (n != DRS_PKT_SIZE) return -1;

    *rx_ts_ns = mono_raw_ns(); /* fallback */
    for (struct cmsghdr *cm = CMSG_FIRSTHDR(&msg); cm;
         cm = CMSG_NXTHDR(&msg, cm)) {
        if (cm->cmsg_level == SOL_SOCKET &&
            cm->cmsg_type  == SO_TIMESTAMPING) {
            struct timespec *ts = (struct timespec *)CMSG_DATA(cm);
            /* Index 0: hw, 1: hw transformed, 2: sw */
            struct timespec *best = &ts[0];
            if (best->tv_sec == 0 && best->tv_nsec == 0) best = &ts[2];
            *rx_ts_ns = (int64_t)best->tv_sec * 1000000000LL + best->tv_nsec;
        }
    }
#else
    ssize_t n = recv(ctx->sock_fd, buf, sizeof(buf), MSG_DONTWAIT);
    if (n != DRS_PKT_SIZE) return -1;
    *rx_ts_ns = mono_raw_ns();
#endif

    return pkt_deserialize(buf, pkt);
}
