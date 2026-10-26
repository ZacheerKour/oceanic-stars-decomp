#include "so2.h"
typedef struct { int pad0; int pad4; void *p8; char pad[0xE]; signed char b1A; } Obj;
void func_80030CBC(Obj *o) { func_8001FD70(D_8004DDB8); D_8004DDB8 = 0; if (o->b1A) { func_80031008(); o->b1A = 0; } func_8001FD70(o->p8); o->p8 = 0; }
