#include "so2.h"
void func_800317E0(S_800317E0 *o, int a, int b, int c)
{
	o->wFF0 = a;
	o->wFF4 = o->wFF0 + func_80011B98(D_8004EBE4, 3024);
	o->wFF8 = o->wFF4 + func_80011B98(D_8004EBE4, 3025);
	o->wFFC = b;
	o->w1000 = c;
	func_8003186C(o, 1);
}
