#include "so2.h"
void func_8006E7AC(S_8006E7AC *o, int a1) { if ( a1 & 0x8000 ) { o->h1c &= ~( a1 & 0x7FFF ); } else { o->h1c |= a1; } }
