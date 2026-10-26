/* DIFF 90 bytes: induction giv shape (retail keeps 4048+12i in one reg and addu o each iteration; ours pointer-bumps). Wall: CFG/giv. Variant with k=i*12+4048 and (int)o+k cast fixes 80031324 but not this. */
#include "so2.h"
S_80031290 *func_80031290(S_80031290 *o) {
	int i = 0, n;
	char *p;
	int k;
	func_800368EC(o);
	while (1) {
		if (i >= 2) break;
		p = func_8001FE30(3840);
		k = i * 12 + 4048; ((S_80031290e *)((int)o + k))->p0 = p;
		for (n = 3839; n >= 0; n--) *p++ = 0;
		((S_80031290e *)((int)o + k))->h4 = 0;
		i++;
	}
	o->hFEC = 0;
	o->hFEE = 0;
	return o;
}
