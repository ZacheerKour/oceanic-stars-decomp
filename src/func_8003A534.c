#include "so2.h"
void func_8003A534(void *o) {
    int i, save;

    save = func_800340B0();
    for ( i = 0; i < 8; i++ ) {
        func_800340C0( i );
        if ( func_8003469C() ) func_8003A5AC( o );
    }
    func_800340C0( save );
}
