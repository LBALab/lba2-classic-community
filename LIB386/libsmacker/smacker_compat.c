/**
 * Compatibility stubs for game code that still calls RAD Smacker API names.
 * When using libsmacker, these are no-ops (libsmacker does not use registered memory).
 */
#ifdef __cplusplus
extern "C" {
#endif

void SmackRegisterMemory(void *ptr, unsigned long size) {
  (void)ptr;
  (void)size;
}

void SmackResetMemory(void) {
}

#ifdef __cplusplus
}
#endif
