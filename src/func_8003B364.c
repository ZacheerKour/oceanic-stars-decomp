#include "so2.h"

void func_8003B364(S_8003B024 *o, int a)
{
	o->h38 += o->h38 * a / 100;
	if ( o->h38 >= 10000 ) {
		o->h38 = 9999;
	}
	if ( o->h38 < 0 ) {
		o->h38 = 0;
	}
}
