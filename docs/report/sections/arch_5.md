# Synchronization Mathematics & Control

## Offset Estimation

```text
θ = ((T2 − T1) + (T3 − T4)) / 2
```


## Round Trip Delay

```text
δ = (T4 − T1) − (T3 − T2)
```


## Min-Delay Filter

The synchronization engine SHALL maintain:

- rolling RTT window size = 10

Only samples within:

```text
minimum RTT + 10 µs
```

SHALL be accepted.

Rejected samples include:

- scheduler spikes
- interrupt storms
- retransmission artifacts
- congestion bursts


## Automated Calibration

Each node SHALL perform self-latency calibration before entering synchronization states.

Calibration SHALL:

- use SO_TIMESTAMPING loopback
- use minimum 50 samples
- use minimum-delay selection
- reject outliers >20 µs from minimum

Calibration output SHALL define:

```text
LatencyCorrection_ns
```

Calibration SHALL re-run:

- during startup
- after leader election
- after HOLDOVER expiration
- after synchronization fault recovery


## Dual-Loop PI Clock Discipline

### Step Mode

A hard phase step SHALL require:

```text
3 consecutive samples confirming |θ| > 1 ms
```

Only the virtual clock MAY step.

The host OS clock SHALL NEVER be modified.

### Slew Mode

If:

```text
|θ| ≤ 1 ms
```

then gradual frequency correction SHALL occur through PI control.


## PI Controller

```text
Rate_new = Rate_old + Kp·θ + Ki·∫θdt
```

Recommended defaults:

| Parameter | Value |
|---|---|
| Kp | 0.05 |
| Ki | 0.005 |
| Update Period | 50 ms |

Integrator SHALL be clamped to:

```text
±1000 ppm equivalent
```


## Slew Rate Limit

Maximum correction rate:

```text
1000 ppm
```

Equivalent to:

```text
50 µs per 50 ms cycle
```


