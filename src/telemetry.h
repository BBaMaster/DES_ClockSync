#pragma once
#include <stdint.h>
#include "election.h"

#define TELEM_PORT      4242
#define TELEM_BUF_SIZE  64   /* power of 2 */
#define TELEM_BUF_MASK  (TELEM_BUF_SIZE - 1)

typedef struct {
    int64_t   timestamp_ns;
    int32_t   state;
    int64_t   offset_ns;
    int64_t   rtt_ns;
    int64_t   rate_q32;
} TelemRecord;

typedef struct {
    TelemRecord  buf[TELEM_BUF_SIZE];
    _Atomic int  head; /* written by RT thread */
    _Atomic int  tail; /* read by drain thread */
    int          drain_fd;
    uint32_t     dest_ip;
} TelemCtx;

/* dest_ip_str: Windows machine IP as string, e.g. "10.0.0.1" */
int  telem_init(TelemCtx *ctx, const char *dest_ip_str);
void telem_write(TelemCtx *ctx, const TelemRecord *rec);
void telem_drain(TelemCtx *ctx); /* call from non-RT thread */
void telem_close(TelemCtx *ctx);
