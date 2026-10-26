#include "so2.h"
void func_8006F53C(S_8006F1A0 *a0, int a1, int a2)
{
    char buf[256];
    int first;
    int p;
    short n;
    void *w;
    char *b;

    func_8006F218(a0);
    p = func_80032380(a0->p54, a1);
    first = 1;
    w = a0->p54;
    func_80032498(w, a2);
    while ( 1 ) {
        n = func_80032E18(a0->p54, p);
        if ( n == -1 ) {
            break;
        }
        b = buf;
        func_80023ED0(b, p, n);
        b[n] = 0;
        func_8006F730(a0, b);
        if ( first ) {
            func_80032498(w, 1);
            first = 0;
        }
        p += n;
    }
}
