#include "so2.h"

void func_8003B2F4(S_8003B024 *o, int a)
{
	o->h44 += o->h44 * a / 100;
	if ( o->h44 >= 10000 ) {
		o->h44 = 9999;
	}
	if ( o->h44 < 0 ) {
		o->h44 = 0;
	}
}
