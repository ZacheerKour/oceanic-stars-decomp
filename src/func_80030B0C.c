#include "so2.h"
S_80030CBC *func_80030B0C(S_80030CBC *o) {
	int a, b;

	D_8004DDC4 = o;
	func_80023F10(o, 0, 56);
	func_80013428(&a, &b);
	o->u0.w = b;
	D_8004EBFC = func_80012108(D_8004EBE0, 2);
	D_8004DDA4 = func_80012108(D_8004EBE0, 6);
	D_8004DDA8 = func_80012108(D_8004EBE0, 3);
	D_8004DDAC = func_80012108(D_8004EBE0, 4);
	if (!func_80012108(D_8004EBE0, 14)) func_80012090(D_8004EBE0, func_8001FE30(1088), 14);
	func_80030D64(o);
	D_8004DDB8 = 0;
	return o;
}
