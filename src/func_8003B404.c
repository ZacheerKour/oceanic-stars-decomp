#include "so2.h"
void func_8003B404( S_8003B404 *o, int a1 ) {
    if ( a1 < 0 ) {
        o->h4A = o->h4A - a1;
    } else {
        o->h4A = o->h4A + o->h4A * a1 / 100;
    }
    if ( o->h4A >= 256 ) o->h4A = 255;
}
