#include "so2.h"

void func_8003B1A4(S_8003B024 *o, int a)
{
	o->h2C += o->h2C * a / 100;
	if ( o->h2C >= 10000 ) {
		o->h2C = 9999;
	}
	if ( o->h2C < 0 ) {
		o->h2C = 0;
	}
}
