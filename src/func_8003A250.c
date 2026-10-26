#include "so2.h"
S_80038228 *func_8003A250(S_80038228 *o) {
    int i, save, hit;

    o->p4 = D_8004BCB4;
    save = func_800340B0();
    for ( i = 0; i < 8; i++ ) {
        func_800340C0( i );
        if ( func_8003469C() ) {
            hit = 0;
            D_8004DDCC->h26 = D_8004DDCC->h28;
            if ( func_80034B6C( 5 ) == 152 || func_80034B6C( 6 ) == 152 ) hit = 1;
            if ( hit ) {
                if ( D_8004DDCC->h26 < 255 ) D_8004DDCC->h26++;
            }
        }
    }
    func_800340C0( save );
    o->w0 = 0;
    if ( !D_8004DDB8 ) func_8003A3C4( o );
    return o;
}
