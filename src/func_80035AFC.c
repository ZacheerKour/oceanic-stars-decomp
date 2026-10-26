#include "so2.h"
void func_80035AFC(S_80035AFC_obj *o) {
	int a;
	o->h10 -= 2;
	if (o->h10 == 0xFFFF) { o->h10 = 0; a = 0x14; } else a = o->ent[o->h10].v;
	func_80035AA4(o, a);
}
