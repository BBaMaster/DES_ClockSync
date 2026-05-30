#include "telemetry.h"
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int telem_init(TelemCtx *ctx, const char *dest_ip_str)
{
    memset(ctx, 0, sizeof(*ctx));
    atomic_store(&ctx->head, 0);
    atomic_store(&ctx->tail, 0);

    ctx->drain_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctx->drain_fd < 0) return -1;

    inet_pton(AF_INET, dest_ip_str, &ctx->dest_ip);
    return 0;
}

void telem_write(TelemCtx *ctx, const TelemRecord *rec)
{
    int head = atomic_load_explicit(&ctx->head, memory_order_relaxed);
    int next = (head + 1) & TELEM_BUF_MASK;
    int tail = atomic_load_explicit(&ctx->tail, memory_order_acquire);

    if (next == tail) return; /* ring full — drop */

    ctx->buf[head] = *rec;
    atomic_store_explicit(&ctx->head, next, memory_order_release);
}

void telem_drain(TelemCtx *ctx)
{
    int tail = atomic_load_explicit(&ctx->tail, memory_order_relaxed);
    int head = atomic_load_explicit(&ctx->head, memory_order_acquire);

    while (tail != head) {
        const TelemRecord *r = &ctx->buf[tail];

        /* Encode as fixed-size binary: 8+4+8+8+8+4 = 40 bytes */
        uint8_t pkt[40];
        memcpy(pkt,      &r->timestamp_ns, 8);
        memcpy(pkt +  8, &r->state,        4);
        memcpy(pkt + 12, &r->offset_ns,    8);
        memcpy(pkt + 20, &r->rtt_ns,       8);
        memcpy(pkt + 28, &r->rate_q32,     8);
        memcpy(pkt + 36, &r->node_id,      4);

        struct sockaddr_in dst = {0};
        dst.sin_family      = AF_INET;
        dst.sin_port        = htons(TELEM_PORT);
        dst.sin_addr.s_addr = ctx->dest_ip;
        sendto(ctx->drain_fd, pkt, sizeof(pkt), 0,
               (struct sockaddr *)&dst, sizeof(dst));

        tail = (tail + 1) & TELEM_BUF_MASK;
        atomic_store_explicit(&ctx->tail, tail, memory_order_release);
    }
}

void telem_close(TelemCtx *ctx)
{
    if (ctx->drain_fd >= 0) {
        close(ctx->drain_fd);
        ctx->drain_fd = -1;
    }
}
