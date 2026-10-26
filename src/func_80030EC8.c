#include "so2.h"
void func_80030EC8()
{
	int v;
	int a;
	int b;
	int h;

	v = func_80031214();
	a = ( (unsigned char)( v & 0x7C ) >> 1 ) + 3111;
	b = ( ( v & 0xF80 ) >> 7 ) + 3175;
	h = func_80012FEC(func_80011B98(D_8004EBE4, a));
	func_80011ABC(D_8004EBE4, h, a, 0);
	func_80031070(h, a);
	func_80013254(h);
	h = func_80012FEC(func_80011B98(D_8004EBE4, b));
	func_80011ABC(D_8004EBE4, h, b, 0);
	func_80031070(h, b);
	func_80013254(h);
}
