#include "so2.h"
int func_80036170(S_80036170 *o, int key, int add) {
	int i = 0;
	S_80036170e *e;

	while (1) {
		if (i >= o->w0C) break;
		e = &o->p8[i];
		if (e->w0 == key) { e->w4 += add; return 1; }
		i++;
	}
	return 0;
}
