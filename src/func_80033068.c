#include "so2.h"
void func_80033068(S_80033068 *o, int a, int b, int c, int d)
{
	int i;

	for ( i = 0; i < 50; i++ ) {
		if ( o->p8[i] && !( o->p8[i]->h0 & 1 ) ) func_8003312C(o, (short)i, a, b, c, d);
	}
	func_8006DD10(o);
}
