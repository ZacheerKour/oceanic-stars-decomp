#include "so2.h"

void func_8003B024(S_8003B024 *o, int a)
{
	if ( a < 0 ) {
		o->w18 -= a;
	} else {
		o->w18 += o->w18 * a / 100;
	}
	if ( o->w18 >= 10000 ) {
		o->w18 = 9999;
	}
}
