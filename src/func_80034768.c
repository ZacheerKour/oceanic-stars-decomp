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
		while ( 1 ) {
			if ( d >= *b ) {
				break;
			}
			a++;
			b--;
		}
	}
	*b += *a;
}
