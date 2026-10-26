/* DIFF 97 bytes: reg alloc + arg schedule (retail loads o->wE0 straight into a1, then lw 4(a1), addu a1,a1,v0; g kept in v1/$3). Loop shape now matches via while(1)+break. Needs int->int* cast. */
#include "so2.h"
void func_80033FD0(S_80030CBC *o, unsigned char *dst, char *str) {
	char *buf, *p;
	int *t;
	unsigned char *g;
	unsigned char c;

	buf = func_8001FE30(func_80035BC8(str) * 3 + 1);
	func_80038354(str, buf);
	p = buf;
	while (1) {
		c = *p;
		if (!c) break;
		p++;
		t = (int *)o->wE0;
		g = func_80032DEC(o, (int)t + t[1], c);
		c = *g++;
		if (c < 128) {
			*dst++ = c;
		} else {
			*dst++ = c;
			*dst++ = *g;
		}
	}
	*dst = 0;
	func_8001FD70(buf);
}
