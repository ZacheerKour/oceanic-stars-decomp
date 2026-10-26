#include "so2.h"
void func_8003B154(S_8003B094 *o, int a) {
    o->h22 = o->h22 - o->h22 * a / 100;
    if ( o->h22 < 0 ) o->h22 = 0;
}
