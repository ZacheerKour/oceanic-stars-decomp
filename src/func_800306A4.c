#include "so2.h"
void func_800306A4(S_800306A4 *o) {
	S_800306A4s *s;

	if (o->w2C == 2) func_80020DC4(1);
	D_8004DD90 = 1;
	func_8003C3D8();
	D_8004DECC = 144;
	D_8004DED0 = func_8003C548() ? 248 : 8;
	func_8003BAF0();
	s = o->p4;
	s->p0C = func_8003C144(s->w8);
	func_80038E10();
	D_8004DD90 = 2;
	if (o->h1A) {
		o->e[0].w = 0;
		o->e[1].w = 0;
		o->e[2].w = 0;
	} else {
		o->e[0].w = func_8003093C(o, func_80014354(D_8004DDA0, 0));
		o->e[1].w = func_8003093C(o, func_8001439C(D_8004DDA0, 0));
		o->e[2].w = func_8003093C(o, func_80014384(D_8004DDA0, 0));
	}
	if (D_8004ECE4) {
		D_8004ECE4--;
		o->e[0].w = 0;
		o->e[1].w = 0;
		o->e[2].w = 0;
	} else if (o->e[0].b) {
		D_8004ECE4 = 4;
	}
	func_800143E8(D_8004DDA0, 0);
	func_8006902C();
	D_8004DD90 = 4;
}
