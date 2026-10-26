#include "so2.h"
void func_8003B0DC(S_8003B094 *o, int a) {
    if ( a < 0 ) {
        o->h22 = o->h22 - a;
    } else {
        o->h22 = o->h22 + o->h22 * a / 100;
    }
    if ( o->h22 >= 1000 ) o->h22 = 999;
}
