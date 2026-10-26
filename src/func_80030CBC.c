#include "so2.h"
void func_80030CBC(S_80030CBC *o) {
	func_8001FD70(D_8004DDB8);
	D_8004DDB8 = 0;
	if (o->b1A) { func_80031008(); o->b1A = 0; }
	func_8001FD70(o->p8);
	o->p8 = 0;
}
