#include "so2.h"
typedef struct { char pad[0x10]; unsigned short idx; char pad2[0xA]; struct { unsigned short v; short pad[3]; } ent[1]; } Obj;
void func_80035AFC(Obj *o) { int a; o->idx -= 2; if (o->idx == 0xFFFF) { o->idx = 0; a = 0x14; } else a = o->ent[o->idx].v; func_80035AA4(o, a); }
