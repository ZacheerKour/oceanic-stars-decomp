#include "so2.h"
void func_80032C58(S_80032270 *o, int n)
{
	char buf[32]; char *p; int w; short total; int pos; int t; int d;
	p = o->pE0->b + o->pE0->w[0];
	w = 0; t = n / 10; buf[0] = 0;
	if ( t ) w = func_80032D48(o, buf, p, (short)t);
	total = w + func_80032D48(o, buf, p, (short)( n - t * 10 ));
	pos = func_80032984(o);
	d = total - 12;
	func_80032950(o, pos - d);
	func_800339F8(o, buf);
}
