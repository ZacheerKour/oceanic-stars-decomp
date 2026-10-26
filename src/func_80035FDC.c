#include "so2.h"
typedef struct { char pad[0xC]; void *p; } Obj;
void func_80035FDC(Obj *o, int f) { func_8001FD70(o->p); if (f & 1) func_8001FD70(o); }
