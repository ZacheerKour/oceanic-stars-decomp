#include "so2.h"

void func_8003B214(S_8003B024 *o, int a)
{
	o->h32 += o->h32 * a / 100;
	if ( o->h32 >= 10000 ) {
		o->h32 = 9999;
	}
	if ( o->h32 < 0 ) {
		o->h32 = 0;
	}
}
