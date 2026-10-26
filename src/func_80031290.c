#include "so2.h"
S_80031290 *func_80031290(S_80031290 *o) {
	int i, n;
	char *p;
	S_80031290e *q;
	func_800368EC(o);
	i = 0;
	while (1) {
		if (i >= 2) break;
		q = &o->e[i];
		p = func_8001FE30(3840);
		q->p0 = p;
		for (n = 3839; n >= 0; n--) *p++ = 0;
		q->h4 = 0;
		i++;
	}
	o->hFEC = 0;
	o->hFEE = 0;
	return o;
}
