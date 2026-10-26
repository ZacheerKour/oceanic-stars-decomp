/* DIFF 368B vs 380B: three 16-entry palette loops (src/so2.h S_80032EEC) match in structure; retail keeps the nibble-mask base t (a0) and the running mask m (v1) as two registers (extra move v1,a0 per loop, 3 insns) while ours merges them. Wall: CSE/copy-prop of m=t. Tried m=t; m=t|0xF0; if/else; ?: ; (-(x!=0))&0xF. Also c register (retail t0, ours a2). */
#include "so2.h"
void func_80032EEC(int unused, short n, int a, int b) {
	unsigned short *p, *q, *r;
	unsigned c, m, t;
	int i;

	p = D_8004ECF0[n].c;
	q = D_8004EDB0[n].c;
	r = D_8004EE70[n].c;
	c = a | a << 4 | a << 8 | a << 12;
	for (i = 0; i < 16; i++) {
		t = i & 8 ? 0xF : 0;
		if (i & 4) m = t | 0xF0; else m = t;
		if (i & 2) m |= 0xF00;
		if (i & 1) m |= 0xF000;
		p[i] = c & m;
	}
	c = b | b << 4 | b << 8 | b << 12;
	for (i = 0; i < 16; i++) {
		t = i & 8 ? 0xF : 0;
		if (i & 4) m = t | 0xF0; else m = t;
		if (i & 2) m |= 0xF00;
		if (i & 1) m |= 0xF000;
		q[i] = c & m;
	}
	c = 0xFFFF;
	for (i = 0; i < 16; i++) {
		t = i & 8 ? 0xF : 0;
		if (i & 4) m = t | 0xF0; else m = t;
		if (i & 2) m |= 0xF00;
		if (i & 1) m |= 0xF000;
		r[i] = ~(c & m);
	}
}
