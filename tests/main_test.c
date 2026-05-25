#include <stdio.h>
#include "test_runner.h"

extern void test_protocol_suite(TestCtx *ctx);
extern void test_clock_suite(TestCtx *ctx);
extern void test_sync_suite(TestCtx *ctx);
extern void test_election_suite(TestCtx *ctx);

int main(void)
{
    TestCtx ctx = {0};

    test_protocol_suite(&ctx);
    test_clock_suite(&ctx);
    test_sync_suite(&ctx);
    test_election_suite(&ctx);

    printf("\n%d passed, %d failed\n", ctx.passed, ctx.failed);
    return ctx.failed ? 1 : 0;
}
