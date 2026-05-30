#include "net.h"
#include "clock.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

/* Find the first non-loopback, multicast-capable, UP IPv4 interface.
 * Returns INADDR_ANY if none found (falls back to kernel default). */
static struct in_addr find_mcast_iface(void)
{
    struct in_addr result = { .s_addr = htonl(INADDR_ANY) };
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) != 0) return result;
    for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK)   continue;
        if (!(ifa->ifa_flags & IFF_UP))       continue;
        if (!(ifa->ifa_flags & IFF_MULTICAST)) continue;
        result = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        break;
    }
    freeifaddrs(ifaddr);
    return result;
}

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

    struct in_addr iface = find_mcast_iface();
    char iface_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &iface, iface_str, sizeof iface_str);
    fprintf(stderr, "net: multicast interface %s\n", iface_str);

    ctx->mcast_iface = iface;
    inet_pton(AF_INET, MCAST_GROUP, &ctx->mcast_group);

    /* Bind both send and receive to the same interface so multicast
     * traffic doesn't accidentally go out via WiFi when no 224/4 route
     * is set in the routing table. */
    struct ip_mreq mreq = {0};
    mreq.imr_multiaddr = ctx->mcast_group;
    mreq.imr_interface = iface;
    setsockopt(ctx->sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    setsockopt(ctx->sock_fd, IPPROTO_IP, IP_MULTICAST_IF,  &iface, sizeof(iface));

    int loop = 0;
    setsockopt(ctx->sock_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    return 0;
fail:
    close(ctx->sock_fd);
    ctx->sock_fd = -1;
    return -1;
}

void net_rejoin_mcast(NetCtx *ctx)
{
    struct ip_mreq mreq = {0};
    mreq.imr_multiaddr = ctx->mcast_group;
    mreq.imr_interface = ctx->mcast_iface;
    /* Drop first (no-op if already gone), then re-add. Recovers membership
     * lost when the Ethernet interface went down and came back up. */
    setsockopt(ctx->sock_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    setsockopt(ctx->sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,  &mreq, sizeof(mreq));
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
