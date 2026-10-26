#include "so2.h"
void func_80031324(S_80031290 *o, int flags) {
	int i = 0;
	S_80031290e *q;

	while (1) {
		if (i >= 2) break;
		q = &o->e[i];
		func_8001FD70(q->p0);
		i++;
	}
	func_80036ACC(o, 2);
	if (flags & 1) func_8001FD70(o);
}
