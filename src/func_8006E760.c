#include "so2.h"
void func_8006E760(S_8006E5E0 *a0, int *a1)
{
    int i;

    for ( i = 0; i < 4; i++ ) {
        if ( a1 == 0 ) {
            a0->w38[i] = D_8004EBFC->w30[i];
        } else {
            a0->w38[i] = a1[i];
        }
    }
}
