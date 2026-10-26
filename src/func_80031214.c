#include "so2.h"
int func_80031214() { int mask = 0, i; unsigned short *p = func_80012108(D_8004EBE0, 3); for (i = 0; i < 8; i++) { unsigned short t = p[i * 0x30]; if ((unsigned)(t - 1) < 12) mask |= 1 << ((short)t - 1); } return mask; }
