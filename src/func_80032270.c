#include "so2.h"
void func_80032270(S_80032270 *o, int a)
{
	int i;

	func_8001FD70(o->pF0);
	for ( i = 0; i < 50; i++ ) {
		if ( o->p8[i] ) func_800323BC(o, i);
	}
	func_8006D4C8(o, a);
}
