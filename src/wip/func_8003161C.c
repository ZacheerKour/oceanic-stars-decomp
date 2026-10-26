/* DIFF 136 bytes: logic exact; loop pointer shape (retail bumps the base pointer itself, ours copies it; slot stride 1996 at o+12). Wall: giv/CFG. */
#include "so2.h"
int func_8003161C(S_8003161C *o) {
	S_8003161Ce *q = &o->s[1];
	short *r;
	int i, a, b;

	a = o->s[0].w[961];
	r = (short *)o->s[0].w;
	if (a == 8834) {
		for (i = 0; i < 15; i++) {
			if (r[968 + i] > 0) return 2;
		}
		return 1;
	}
	b = q->w[961];
	r = (short *)q->w;
	if (b == 8834) {
		for (i = 0; i < 15; i++) {
			if (r[968 + i] > 0) return 2;
		}
		return 1;
	}
	if (a == 8833 || b == 8833) return 3;
	if (a != 4417) return 0;
	return b == 4417 ? 3 : 0;
}
