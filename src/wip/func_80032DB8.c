/* DIFF ~all bytes: non-rotated top-test loop (retail j-to-top, delay-slot increments) vs gcc-2.7.2 rotated loop. Wall: loop shape. strcat: seek end then copy. Tried while/for(;;)/break/-O1. */
#include "so2.h"
void func_80032DB8(int unused, char *d, char *s) { for ( ; ; d++ ) { if ( !*d ) break; } for ( ; ; ) { char c = *s++; *d++ = c; if ( !c ) break; } }
