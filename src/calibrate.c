#include "calibrate.h"
#include "clock.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CALIB_SAMPLES        64
#define CALIB_MIN_ACCEPTED   50      /* spec §5.4: minimum 50 samples */
#define CALIB_OUTLIER_NS     20000LL /* spec §5.4: reject >20 µs from min */
#define CALIB_PROBE_BYTES    64
#define CALIB_RECV_TIMEOUT_S 1

int calibrate_loopback(int64_t *out_correction_ns)
{
    if (!out_correction_ns) return -1;

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(0);             /* kernel picks a free port */
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&addr, sizeof addr) < 0) goto fail;

    socklen_t addrlen = sizeof addr;
    if (getsockname(s, (struct sockaddr *)&addr, &addrlen) < 0) goto fail;

    uint8_t buf[CALIB_PROBE_BYTES] = {0};
    int64_t samples[CALIB_SAMPLES];
    int     collected = 0;

    /* Allow up to 2× the target sample count to absorb the occasional
     * scheduler-delayed probe; if too many fail we just give up. */
    for (int i = 0; i < CALIB_SAMPLES * 2 && collected < CALIB_SAMPLES; i++) {
        int64_t t0 = mono_raw_ns();
        if (sendto(s, buf, sizeof buf, 0,
                   (struct sockaddr *)&addr, sizeof addr) != (ssize_t)sizeof buf)
            continue;

        /* Bounded wait — never block startup forever if loopback drops. */
        fd_set rfds; FD_ZERO(&rfds); FD_SET(s, &rfds);
        struct timeval tv = { .tv_sec = CALIB_RECV_TIMEOUT_S, .tv_usec = 0 };
        if (select(s + 1, &rfds, NULL, NULL, &tv) <= 0) continue;

        ssize_t r = recv(s, buf, sizeof buf, 0);
        int64_t t1 = mono_raw_ns();
        if (r != (ssize_t)sizeof buf) continue;

        samples[collected++] = t1 - t0;
    }
    close(s);

    if (collected < CALIB_MIN_ACCEPTED) return -1;

    int64_t min_rtt = samples[0];
    for (int i = 1; i < collected; i++)
        if (samples[i] < min_rtt) min_rtt = samples[i];

    /* Average the samples within the 20 µs slack window for stability;
     * raw min-only can latch onto an unrepresentative low outlier. */
    int64_t sum = 0;
    int     n   = 0;
    for (int i = 0; i < collected; i++) {
        if (samples[i] <= min_rtt + CALIB_OUTLIER_NS) {
            sum += samples[i];
            n++;
        }
    }
    if (n == 0) return -1;

    /* Loopback RTT covers send+receive; one-way correction is half. */
    *out_correction_ns = (sum / n) / 2;
    return 0;

fail:
    close(s);
    return -1;
}
