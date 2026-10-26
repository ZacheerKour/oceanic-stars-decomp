/* DIFF 3 bytes: register order (retail addu v0,v0,a0 ; ours addu a0,a0,v0). Wall: register. D_8004DDD0 typed unsigned char* shared; likely a struct pointer with signed char b5C[] in the original. */
#include "so2.h"
int func_80034BAC(int i) { int v = (signed char)D_8004DDD0[i + 0x5C]; if (v < 0) v = -v; return v; }
