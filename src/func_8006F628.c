#include "so2.h"
unsigned short func_8006F628(S_8006F1A0 *a0, int a1)
{
    char src[128];
    char dst[256];
    char *d;
    char *g;
    int save;
    int i;
    unsigned char c;
    int r;

    save = func_80032374(a0->p54);
    func_8003236C(a0->p54, 1);
    func_80038354(a1, src);
    d = dst;
    i = 0;
    while ( 1 ) {
        if ( i >= 128 ) break;
        if ( src[i] == 0 ) break;
        g = func_80032380(a0->p54, src[i]);
        c = *g++;
        if ( c < 128 ) {
            *d = c;
        } else {
            *d++ = c;
            *d = *g;
        }
        d++;
        i++;
    }
    *d = 0;
    r = func_8006F730(a0, dst);
    func_8003236C(a0->p54, save);
    return r;
}
