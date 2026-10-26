/* DIFF 1 byte: beqz $3 initial guard targets the post-loop "if(!c)" test in retail (0x80032e84); ours jump-threads it to the p-- (0x80032e8c). Wall class CFG (jump threading). Tried for/while/!=0, p -= (c==0), break-based p--, nested ifs. Key lever found: "unsigned code = c & 0xFFFF" (not ushort). */
#include "so2.h"
short func_80032E18(unsigned char *p, unsigned char *s)
{
	unsigned c; unsigned code; int x;
	x = *s;
	if ( x == 0 ) return -1;
	c = x & 0xFF;
	p = s + 1;
	while ( c ) {
		if ( c >= 128 ) { c = ( c & 0x7F ) | ( *p++ << 7 ); }
		code = c & 0xFFFF;
		if ( code >= 0x4000 && code == 0x4000 ) break;
		c = *p++;
	}
	if ( !c ) {
		p--;
	}
	return p - s;
}
