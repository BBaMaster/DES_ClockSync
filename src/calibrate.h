#pragma once
#include <stdint.h>

/* Measure this node's send+receive self-latency over the loopback interface
 * and return half of the min-delay value (the one-way correction).
 *
 * Sends ≥50 UDP probes to a freshly-bound loopback socket; min-delay
 * selection; rejects samples >20 µs from the minimum.
 *
 * Returns 0 and writes *out_correction_ns on success, -1 on failure
 * (socket setup, insufficient accepted samples, etc.).
 *
 * Required by spec §5.4. */
int calibrate_loopback(int64_t *out_correction_ns);
