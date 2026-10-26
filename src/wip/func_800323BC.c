/* DIFF 16 bytes: register alloc only (retail o=s0, &p8[i]=s1; ours swapped). Wall class register. Tried pointer-to-slot local (worse). */
#include "so2.h"
void func_800323BC(S_800323BC *o, short i)
{
	func_8006DCD4(o, o->p8[i]->h1E);
	func_8006DCD4(o, o->p8[i]->h20);
	if ( o->p8[i]->w24 ) func_8003244C(o);
	func_8001FD70(o->p8[i]);
	o->p8[i] = 0;
}
