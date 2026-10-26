/* wip: 3 bytes (register only): retail keeps the o->pE0 table base in a1 (lw a1,224(s2); lw v0,4(a1); addu a1,a1,v0), ours puts the base in v0. Tried: operand order, local table ptr, local offset, char* base. Class: register/schedule. */
#include "so2.h"
void func_80033FD0(S_80032270 *o, unsigned char *dst, char *str) {
	char *buf, *p;
	unsigned char *g;
	unsigned char c, d;

	buf = func_8001FE30(func_80035BC8(str) * 3 + 1);
	func_80038354(str, buf);
	p = buf;
	while (1) {
		c = *p;
		if (!c) break;
		p++;
		g = func_80032DEC(o, o->pE0->w[1] + o->pE0->b, c);
		d = *g++;
		if (d < 128) {
			*dst++ = d;
		} else {
			*dst++ = d;
			*dst++ = *g;
		}
	}
	*dst = 0;
	func_8001FD70(buf);
}
