# Failure Recovery

## Retry Storm Protection

Failed nodes SHALL reduce multicast rate to:

```text
5 Hz
```

after:

```text
5 consecutive synchronization failures
```


## Filter Reset Conditions

Synchronization filters SHALL reset on:

- reboot
- sequence discontinuity
- leader change
- timestamp overflow
- phase correction >5 ms


## Sequence Wraparound

Sequence numbers SHALL use unsigned 16-bit rollover semantics.

A sequence transition:

```text
65535 → 0
```

SHALL be treated as valid wraparound.

Backward jumps exceeding:

```text
1024 sequence values
```

SHALL invalidate synchronization state.


