/* 49 bytes differ: last addu operand order (D + x*12) and a2 increment schedule; wall: register/schedule */
#include "so2.h"
void func_80042BCC()
{
    int i;
    int j;

    func_80061C58();
    D_8004EBFC = func_80012108( D_8004EBE0, 2 );
    i = 0;
    while ( 1 ) {
        if ( i >= 12 ) break;
        for ( j = 0; j < 12; j++ ) {
            D_8004EBFC->t4C[D_8004E32C[j]][D_8004E32C[i] - 1] = D_8004E338[j][i];
            D_8004EBFC->tDC[D_8004E32C[j]][D_8004E32C[i] - 1] = D_8004E3C8[j][i];
        }
        i++;
    }
}
