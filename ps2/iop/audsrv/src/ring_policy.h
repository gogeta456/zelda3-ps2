#ifndef Z3_AUDSRV_RING_POLICY_H
#define Z3_AUDSRV_RING_POLICY_H
/* Keep one word vacant so full and empty never have the same head positions.
 * An underrun emits silence without moving the consumer into unwritten data. */
static inline int z3_ring_queued(int read, int write, int size) {
  return write >= read ? write - read : size - (read - write);
}
static inline int z3_ring_available(int read, int write, int size) {
  return size - 4 - z3_ring_queued(read, write, size);
}
static inline int z3_ring_can_read(int read, int write, int size, int fragment) {
  return z3_ring_queued(read, write, size) >= fragment;
}
#endif
