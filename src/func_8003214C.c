#include "so2.h"
S_80030CBC *func_8003214C(S_80030CBC *o) {
	int n;
	short i;
	int v;

	func_8006D424();
	o->hE8 = 30;
	o->hEA = 100;
	o->hEE = 1;
	o->u0.h[1] = 0;
	for (n = 49; n >= 0; n--) ((int *)o)[n + 2] = 0;
	func_80032320(o, D_8004ECE8, D_8004ECEC);
	o->pF0 = func_8001FD94(16);
	v = func_8002FFE4()->p4->w4;
	o->pF0->w4 = v;
	v = func_8002FFE4()->p4->w8;
	o->pF0->w8 = v;
	for (i = 0; i < 6; i++) func_80032EEC(o, i, i * 2 + 3, 1);
	o->p104 = &D_8004ECD0[1];
	o->p108 = &D_8004ED90[1];
	o->p10C = &D_8004EE50[1];
	return o;
}
