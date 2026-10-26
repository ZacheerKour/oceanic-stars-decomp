/* DIFF 92 bytes: logic and loop shape exact (r++ pointer bump, wave9); only register allocation differs (retail r=v1 a=a1 b=a0 q=a2 const8834=v0; ours r=a1 a=v1 q=v0 const=a2). Wall: register. 120 decl-order permutations and separate-pointer variants tried, no change. */
#include "so2.h"
int func_8003161C(S_8003161C *o) {
	S_8003161Ce *q = &o->s[1];
	short *r;
	int i, a, b;

	a = o->s[0].w[961];
	r = (short *)o->s[0].w;
	if (a == 8834) {
		for (i = 0; i < 15; i++) {
			if (r[968] > 0) return 2;
			r++;
		}
		return 1;
	}
	b = q->w[961];
	r = (short *)q->w;
	if (b == 8834) {
		for (i = 0; i < 15; i++) {
			if (r[968] > 0) return 2;
			r++;
		}
		return 1;
	}
	if (a == 8833 || b == 8833) return 3;
	if (a != 4417) return 0;
	return b == 4417 ? 3 : 0;
}
