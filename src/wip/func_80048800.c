/* 30 bytes differ: frame is 80 with buf at sp+24 (24 unexplained bytes below buf); wall class: frame/layout */
#include "so2.h"
typedef struct { char pad0[0x20]; int w20; char pad24[0xC]; char s30[7][6]; } S_80048800;
int func_80048800(S_80048800 *a0, char *a1)
{
    char buf[42];
    char *p;
    char *q;
    char c;
    char d;
    int k;

    buf[0] = a0->s30[0][0];
    p = buf + 1;
    if ( buf[0] != 0 ) {
        q = &a0->s30[0][1];
        do {
            c = *q++;
            *p++ = c;
        } while ( c != 0 );
    }
    for ( k = 1; k < 7; k++ ) {
        q = a0->s30[k];
        p--;
        do {
            c = *q++;
            *p++ = c;
        } while ( c != 0 );
    }
    p = buf;
    q = a1;
    if ( *p != 0 ) {
        do {
            c = *p;
            if ( c == '@' && p[1] == c ) {
                p += 2;
            } else {
                d = *p;
                p++;
                *q++ = d;
            }
        } while ( *p != 0 );
    }
    *q = 0;
    if ( a1[0] == 0 ) return 0;
    if ( a0->w20 == 0 ) {
        if ( !( a1[0] == 'C' && a1[1] == 'l' && a1[2] == 'a' && a1[3] == 'u' && a1[4] == 'd' && a1[5] == 'e' && a1[6] == 0 ) )
            D_8004EBFC->b42 = 1;
    }
    if ( a0->w20 == 1 ) {
        if ( !( a1[0] == 'R' && a1[1] == 'e' && a1[2] == 'n' && a1[3] == 'a' && a1[4] == 0 ) )
            D_8004EBFC->b43 = 1;
    }
    return 1;
}
