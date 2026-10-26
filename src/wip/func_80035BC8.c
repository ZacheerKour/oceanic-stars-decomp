/* DIFF ~all bytes: non-rotated top-test loop (retail j-to-top, delay-slot increments) vs gcc-2.7.2 rotated loop. Wall: loop shape. strlen: while(*s++) n++. Tried while/for(;;)/break/-O1. */
#include "so2.h"
int func_80035BC8(unsigned char *s) { int n = 0; while ( *s++ ) n++; return n; }
