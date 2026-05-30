# Architectural Constraints

The following are explicitly prohibited:

- modifying kernel clocks
- custom kernel modules
- TCP transport
- Wi-Fi synchronization operation
- distributed consensus protocols
- mutex blocking in hot path
- filesystem I/O in synchronization loop
- floating-point exceptions in RT thread


