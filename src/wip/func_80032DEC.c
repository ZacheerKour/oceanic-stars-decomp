/* DIFF ~all bytes: non-rotated top-test loop (retail j-to-top, delay-slot increments) vs gcc-2.7.2 rotated loop. Wall: loop shape. skip n NUL-terminated strings (n short). Tried while/for(;;)/break/-O1. */
#include "so2.h"
char *func_80032DEC(unused, p, n) int unused; char *p; short n; { while ( n ) { if ( !*p++ ) n--; } return p; }
