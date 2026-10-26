#include "so2.h"
void func_80046E64(S_80046E64 *a0, int a1)
{
    switch ( D_8004EB18 ) {
    case 1:
        if ( func_800491D0(2675) ) {
            func_8001A650(D_8004EBF8);
            func_80018FF4(D_8004EBF8, &a0->b[a0->h.w0], 0, 0);
            D_8004EB18 = D_8004EB18 + 1;
        }
        break;
    case 2:
        func_800121A8(D_8004EBE8, &a0->h.w4, a1, 0);
        D_8004EB18 = D_8004EB18 + 1;
        break;
    case 3:
        if ( (func_80018F04(D_8004EBF8) ^ 1) != 0 ) {
            D_8004EB18 = D_8004EB18 + 1;
        }
        break;
    case 4:
        func_8001A5D8(D_8004EBF8, a1);
        D_8004EB18 = 255;
        break;
    }
}
