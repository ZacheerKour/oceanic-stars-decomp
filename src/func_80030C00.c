#include "so2.h"
void func_80030C00(S_80030CBC *o, int flags) {
	if (o->b34) D_8004DDA4->bC24 = 1;
	func_80030CBC(o);
	func_80030D24(o);
	if (!D_8004DDC4->b16) {
		func_8001FD70(func_80012108(D_8004EBE0, 14));
		func_800120D8(D_8004EBE0, 14);
	}
	D_8004DDC4 = 0;
	if (flags & 1) func_8001FD70(o);
}
