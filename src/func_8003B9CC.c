#include "so2.h"
int func_8003B9CC(S_8003B9CC *o, int a) { int r = func_8003B944(o, a, (~a & 0xF) ^ (o->b824[a] >> 4)); return o->b824[a] == (r & 0xFF); }
