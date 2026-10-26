/* 2.7.2-psx: 12 insns of 64 differ, body+regs match; wall: gcc puts la in beq slot / unfilled jal slot where retail splits lui/addiu around jal, epilogue slot fill (retail jr ra; addiu sp); R4 absolute-address casts *(T*)336/340 needed */
#include "so2.h"
typedef struct { char *p0; char pad4[0x30]; void *p34; char pad38[0x18]; } S_8004B270e;
extern void *D_8004EFE4;
extern char D_8004EFEC[];
void func_8004B270( int *a0, int a1, int a2 )
{
    S_8004B270e *e;
    S_8004B270e *tbl;
    unsigned n;
    void *fn;

    if ( a0[0] == 0 ) {
        a0[0] = 1;
    }
    n = *(unsigned *)340 / 80;
    tbl = *(S_8004B270e **)336;
    fn = D_8004EFE4;
    for ( e = tbl; e < tbl + n; e++ ) {
        if ( e->p0 != 0 && func_8004B374( e->p0, D_8004EFEC ) == 0 ) {
            e->p34 = fn;
            break;
        }
    }
    ( (void (*)())D_8004EFE4 )( a0, a1, a2 );
}
