#include "so2.h"
void func_80030A60(int a, int b)
{
	unsigned x = func_80031214(a);
	int lo = ((x & 0x7C) >> 1) + 3111;
	int hi = ((int)(x & 0xF80) >> 7) + 3175;
	func_80031070(a, lo);
	func_80031070(b, hi);
}
