/* DIFF 124 bytes: loop shape (retail non-rotated top test, j-to-top) + reg alloc (o=s2, buf=s3, walker reuses s0). Needs offset cast (R4). Tried while/for(;;), reuse str. */
#include "so2.h"
void func_80033FD0(S_80030CBC *o, unsigned char *dst, char *str)
{
	char *buf;
	unsigned char *g;
	unsigned char c;

	buf = func_8001FE30(func_80035BC8(str) * 3 + 1);
	func_80038354(str, buf);
	str = buf;
	for ( ; ; ) {
		c = *str++;
		if ( !c ) break;
		g = func_80032DEC(o, o->wE0 + ((S_80033FD0p *)o->wE0)->w4, c);
		c = *g++;
		if ( c < 128 ) {
			*dst++ = c;
		} else {
			*dst++ = c;
			*dst++ = *g;
		}
	}
	*dst = 0;
	func_8001FD70(buf);
}
