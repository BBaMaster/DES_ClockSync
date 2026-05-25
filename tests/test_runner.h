#pragma once
#include <stdio.h>

typedef struct {
    int passed;
    int failed;
} TestCtx;

#define EXPECT(ctx, cond) do { \
    if (cond) { \
        (ctx)->passed++; \
    } else { \
        (ctx)->failed++; \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)
