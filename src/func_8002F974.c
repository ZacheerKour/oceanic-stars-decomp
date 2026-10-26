#include "so2.h"
int func_8002F974(int a)
{
	a &= 0xFFF;
	if ( a < 1024 ) return D_8004D58C[1024 - a];
	if ( a < 2048 ) return -D_8004D58C[a - 1024];
	if ( a < 3072 ) return -D_8004D58C[3072 - a];
	return D_8004D58C[a - 3072];
}
