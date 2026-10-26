#include "so2.h"
void func_800323BC(S_80032270 *o, short i)
{
	Node *n;

	func_8006DCD4(o, o->p8[i]->h1E);
	func_8006DCD4(o, o->p8[i]->h20);
	n = o->p8[i]->p24;
	if ( n ) func_8003244C(o, n);
	func_8001FD70(o->p8[i]);
	o->p8[i] = 0;
}
