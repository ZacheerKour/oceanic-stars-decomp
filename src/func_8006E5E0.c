#include "so2.h"
void *func_8006E5E0(S_8006E5E0 *a0)
{
    int i;

    func_80038228(a0);
    a0->p4 = D_80074A78;
    a0->h32 = 0;
    a0->h34 = 0;
    a0->h60 = 5;
    func_8006E7E4(a0);
    a0->h74 = 0;
    a0->h72 = 0;
    a0->pC = func_80036658(func_8002FFE4()->wC);
    func_80036124(a0->pC->w10, a0, 2);
    a0->w58 = a0->pC->wC;
    for ( i = 0; i < 4; i++ ) {
        a0->w38[i] = D_8004EBFC->w30[i];
    }
    a0->h1C = 0;
    a0->w7C = 0;
    a0->w80 = 0;
    a0->h8 = 0;
    return a0;
}
