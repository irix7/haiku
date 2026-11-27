# The cross build of libgpg-error needs the lock object of the target. Haiku
# has no such file for mips. pthread_mutex_t is 20 bytes: PTHREAD_MUTEX_INITIALIZER
# is { 0, 0, -42, -1, 0 } of 32 bit words, big endian here.
s|^\tMAKEINFO=true runConfigure \./configure$|\tprintf 'typedef struct\\n{\\n  long _vers;\\n  union {\\n    volatile char _priv[20];\\n    long _x_align;\\n    long *_xp_align;\\n  } u;\\n} gpgrt_lock_t;\\n\\n#define GPGRT_LOCK_INITIALIZER {1,{{0,0,0,0,0,0,0,0,255,255,255,214,255,255,255,255,0,0,0,0}}}\\n' > src/syscfg/lock-obj-pub.haiku.h\n\tMAKEINFO=true runConfigure ./configure|
