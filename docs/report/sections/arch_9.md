# Lock-Free Clock Access

The virtual clock SHALL support lock-free concurrent readers.

The synchronization thread SHALL be the exclusive writer.

Reader access SHALL:

- avoid mutex blocking
- avoid dynamic allocation
- guarantee monotonic reads

Offset and Rate updates SHALL use:

- atomic 64-bit operations
- acquire/release memory ordering


