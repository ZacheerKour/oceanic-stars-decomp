#include "so2.h"
unsigned char *func_8004A234(int a0, int a1)
{
    unsigned short *p;
    unsigned short v;
    unsigned char *r;

    p = D_8004EC1C->h;
    while ( 1 ) {
        v = *p++;
        if ( v == 0xFFFF ) {
            r = 0;
            break;
        }
        if ( v == a1 ) {
            r = D_8004EC1C->b + *p;
            break;
        }
        p++;
    }
    return r;
}
