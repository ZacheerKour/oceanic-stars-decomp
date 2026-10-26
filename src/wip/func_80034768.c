/* 33 differing bytes; wall: control flow. Retail loop is NOT rotated (test at top, j back); gcc rotates while/for(;;) here (same class as 0x800361CC / 0x800386FC). */
#include "so2.h"
void func_80034768(int *a, int *b, int c, int d)
{
	int t, diff;

	*b -= *a;
	t = *a;
	if ( c != t + *b ) {
		diff = c - t;
		*b = diff;
		if ( diff < 0 ) {
			*a += diff;
			*b = 0;
		}
		while ( d < *b ) {
			a++;
			b--;
		}
	}
	*b += *a;
}
