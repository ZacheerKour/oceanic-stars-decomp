#include "so2.h"
int func_800361CC(S_80036170 *o, int key) {
	int i = 0;
	S_80036170e *e;

	while (1) {
		if (i >= o->w0C) break;
		e = &o->p8[i];
		if (e->w0 == key) {
			e->w8 |= 0x10;
			func_800367B4(o);
			return 1;
		}
		i++;
	}
	return 0;
}
