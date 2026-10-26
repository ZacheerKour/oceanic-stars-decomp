#include "so2.h"
int func_80031070(S_80031070 *o, int key)
{
	int i;
	void *r;

	for ( i = 0; i < 16; i++ ) {
		if ( D_8004DDD4[i].p0 && D_8004DDD4[i].w4 == key ) return 1;
	}
	for ( i = 0; i < 16; i++ ) {
		if ( !D_8004DDD4[i].p0 ) break;
	}
	r = func_80012154(D_8004DD9C, o, 0);
	D_8004DDD4[i].p0 = r;
	func_800121A8(D_8004DD9C, o, r, 0);
	D_8004DDD4[i].w4 = key;
	D_8004DDD4[i].w8 = o->w8;
	return 1;
}
