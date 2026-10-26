/* wall register: body matches; loop 1 keeps a separate biv (extra move t1,a0) and param copy in t3 vs retail t2; short n gets the move t1,v0 */
#include "so2.h"
void func_8006DDC4(int a0, short a1)
{
    int i;
    short n;
    S_8006D588 *t, *e;
    unsigned short flags;

    n = D_80074BD4;
    for ( i = 0; i < n; i++ ) {
        e = D_80074BCC + i;
        t = D_80074BCC + a1 - 1;
        flags = e->h0;
        if ( flags & 2 ) {
            if ( t->h4 == e->h4 ) {
                if ( t->h8 == e->h8 ) {
                    if ( t->h2 + t->h6 == e->h2 ) {
                        e->h0 = flags & ~3;
                        t->h6 += e->h6;
                    }
                    if ( e->h2 + e->h6 == t->h2 ) {
                        e->h0 &= ~3;
                        t->h2 = e->h2;
                        t->h6 += e->h6;
                    }
                }
            }
        }
    }
    n = D_80074BD4;
    for ( i = 0; i < n; i++ ) {
        e = D_80074BCC + i;
        t = D_80074BCC + a1 - 1;
        flags = e->h0;
        if ( flags & 2 ) {
            if ( t->h2 == e->h2 ) {
                if ( t->h6 == e->h6 ) {
                    if ( t->h4 + t->h8 == e->h4 ) {
                        e->h0 = flags & ~3;
                        t->h8 += e->h8;
                    }
                    if ( e->h4 + e->h8 == t->h4 ) {
                        e->h0 &= ~3;
                        t->h4 = e->h4;
                        t->h8 += e->h8;
                    }
                }
            }
        }
    }
}
