#include "so2.h"
int func_8004A358(int a0, unsigned char *a1, int *a2)
{
    unsigned short c;
    unsigned short v;
    unsigned char *t;
    int n;

    a0 = 0;
    n = 0;
    t = D_8004EC20->b8;
    while ( 1 ) {
        v = *a1++;
        c = v;
        if ( c == 0 ) {
            break;
        }
        if ( c >= 128 ) {
            v = (v & 0x7F) | (*a1++ << 7);
        }
        *a2++ = a0;
        v--;
        a0 += t[v];
        n++;
    }
    *a2 = a0;
    return n;
}
