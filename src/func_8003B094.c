#include "so2.h"
void func_8003B094(S_8003B094 *o, int a) {
    int v = o->w18 - o->w18 * a / 100;

    o->w18 = v;
    if ( v <= 0 ) o->w18 = 1;
}
