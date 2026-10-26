#include "so2.h"
short func_8006DAC0(int a0, int a1, int a2, short a3, short a4) {
    S_8006DAC0 s;
    if ( a3 == 0 || a4 == 0 ) { return -1; }
    s.h0 = 2; s.h2 = a1; s.h4 = a2; s.h6 = a3; s.h8 = a4;
    return func_8006DB24(a0, &s);
}
