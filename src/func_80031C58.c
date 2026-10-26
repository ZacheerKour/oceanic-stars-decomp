#include "so2.h"
typedef struct { char pad[0x54]; unsigned short mode; } Obj;
void func_80031C58(Obj *o) { switch (o->mode) { case 0: func_80031CC8(o); break; case 1: func_80031E94(o); break; } func_8006AB68(o); }
