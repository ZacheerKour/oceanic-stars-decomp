/* 68 differing bytes; wall: register/schedule (p wants a0, code a2; guard branch target). Loop is rotated like retail. */
#include "so2.h"
short func_80032E18(unsigned char *p, unsigned char *s)
{
	unsigned c;
	unsigned short code;
	int x;

	x = *s;
	if ( x == 0 ) return -1;
	c = x & 0xFF;
	p = s + 1;
	while ( c ) {
		code = c;
		if ( c >= 128 ) {
			c = ( c & 0x7F ) | ( *p++ << 7 );
			code = c;
		}
		if ( code >= 0x4000 && code == 0x4000 ) break;
		c = *p++;
	}
	if ( !c ) p--;
	return p - s;
}
