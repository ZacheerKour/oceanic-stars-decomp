#include "so2.h"
void func_800329B0(S_80030CBC *o) {
	S_80032840e *e;
	S_8006E1A0 r, r2;

	e = o->p8[o->hF4];
	e->h22 = 31313;
	if (e->h8 == 0) {
		r.x = func_8006D588(o, e->h1E);
		r.y = func_8006D5B4(o, e->h1E);
		r.w = e->h6 >> 2;
		r.h = e->hA;
		e->h1A = (( (unsigned short)r.y >> 4 ) & 0x10) | (( r.x & 0x3FF ) >> 6) | (( (unsigned short)r.y & 0x200 ) << 2);
		func_8006E01C(&r, o->pF8);
	} else {
		r.x = func_8006D588(o, e->h1E);
		r.y = func_8006D5B4(o, e->h1E);
		r.w = e->h6 >> 2;
		r.h = e->hA;
		e->h1A = (( (unsigned short)r.y >> 4 ) & 0x10) | (( r.x & 0x3FF ) >> 6) | (( (unsigned short)r.y & 0x200 ) << 2);
		r2.x = func_8006D588(o, e->h20);
		r2.y = func_8006D5B4(o, e->h20);
		r2.w = e->h8 >> 2;
		r2.h = e->hA;
		e->h1C = (( (unsigned short)r2.y >> 4 ) & 0x10) | (( r2.x & 0x3FF ) >> 6) | (( (unsigned short)r2.y & 0x200 ) << 2);
		func_8006E1A0(&r, &r2, o->pF8);
	}
}
