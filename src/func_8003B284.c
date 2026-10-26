#include "so2.h"

void func_8003B284(S_8003B024 *o, int a)
{
	o->h3E += o->h3E * a / 100;
	if ( o->h40 >= 10000 ) {
		o->h40 = 9999;
	}
	if ( o->h40 < 0 ) {
		o->h40 = 0;
	}
}
