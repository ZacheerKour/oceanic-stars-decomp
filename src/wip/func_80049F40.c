/* 64 bytes differ: logic matches, register assignment only (retail code=a0 next-char=a3, v1=code+1 kept distinct from code); wall: register */
#include "so2.h"
extern unsigned char D_8004EAFC[];
void func_80049F40( unsigned code, unsigned char *src, unsigned char *dst )
{
    int mode;
    unsigned c;
    unsigned d;
    unsigned n;

    mode = 0;
    while ( 1 ) {
        c = *src++;
        if ( c == 0 ) {
            break;
        }
        if ( c == 64 ) {
            mode = 1 - mode;
            continue;
        }
        if ( mode && c - 166 < 56 ) {
            code = D_8004EAFC[c] + 56;
        } else if ( c - 97 < 26 ) {
            code = c - 32;
        } else {
            code = D_8004EAFC[c];
        }
        d = *src;
        if ( d == 222 ) {
            if ( mode == 0 ) {
                if ( c == 179 ) {
                    code = 179;
                } else if ( c - 182 < 5 || c - 187 < 5 || c - 192 < 5 ) {
                    code = c - 2;
                } else if ( c - 202 < 5 ) {
                    code = c - 7;
                }
            } else {
                if ( c - 182 < 5 || c - 187 < 5 || c - 192 < 5 ) {
                    code = c + 23;
                } else if ( c - 202 < 5 ) {
                    code = c + 18;
                }
            }
            src++;
        } else if ( d == 223 ) {
            if ( mode == 0 ) {
                code = c - 2;
            } else {
                code = c + 23;
            }
            src++;
        }
        n = code + 1;
        code = n;
        if ( ( code & 255 ) < 128 ) {
            *dst++ = code;
        } else {
            *dst++ = n | 128;
            *dst++ = ( code & 255 ) >> 7;
        }
    }
    *dst = 0;
}
