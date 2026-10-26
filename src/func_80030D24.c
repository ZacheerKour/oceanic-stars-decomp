#include "so2.h"
typedef struct { char pad[0x19]; signed char flag; } Obj;
void func_80030D24(Obj *o) { if (o->flag) { func_80030FC8(o); o->flag = 0; } }
