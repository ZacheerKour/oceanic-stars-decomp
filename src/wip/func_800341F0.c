/* DIFF 10 bytes: register (retail base in v1/off in v0; ours swapped); same wall as sibling getter func_80034110; needs offset casts (R4) anyway -> would be MATCH-PENDING. Wall: register. Tried off+base / base+off, char* base. */
#include "so2.h"
void func_800341F0(int a, int v) {
	int off = 0, k = a & 0x1F, f, base;
	func_800342D8(&off, k);
	f = (a & 0xF00) >> 8;
	if (D_8004B5D0[k] & 2) {
		int *p;
		if (D_8004B5D0[k] & 1) base = (int)D_8004DDD0; else base = (int)D_8004DDCC;
		p = (int *)(off + base);
		p[f] = v;
	} else {
		short *p;
		if (D_8004B5D0[k] & 1) base = (int)D_8004DDD0; else base = (int)D_8004DDCC;
		p = (short *)(off + base);
		p[f] = v;
	}
}
