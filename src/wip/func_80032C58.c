/* DIFF 8 bytes: retail does ext(total), addiu -12 in place in s16 then v0 - s16; ours folds to (v0+12) - ext or uses a second reg. Wall class register/expression shape. Tried (short)total-12 inline, short total, pos temp, total -= 12. */
#include "so2.h"
void func_80032C58(S_80030CBC *o, int n)
{
	char buf[32];
	int p;
	int w;
	short total;
	int pos;
	int t;

	p = o->wE0 + ((S_80033FD0p *)o->wE0)->w0;
	w = 0;
	t = n / 10;
	buf[0] = 0;
	if ( t ) w = func_80032D48(o, buf, p, (short)t);
	total = w + func_80032D48(o, buf, p, (short)( n - t * 10 ));
	pos = func_80032984(o);
	func_80032950(o, pos - ( total - 12 ));
	func_800339F8(o, buf);
}
