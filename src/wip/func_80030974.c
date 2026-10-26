/* DIFF 5 bytes: final addu dest reg (retail a1=a1+v0 keeps base v0 live; ours v0). Wall class register. Tried operand swap. Needs (char*) offset cast: R4 */
#include "so2.h"
void func_80030974(int unused, unsigned short idx, int x, S_80030974p *p)
{
	S_80030974r *rec;
	S_80030974e *e;
	int i;

	rec = (S_80030974r *)(D_8004DDB0->w[idx] + (char *)D_8004DDB0);
	e = rec->e;
	func_80021108(x, rec->b220);
	for ( i = 0; i < p->h6; i++ ) {
		func_80012F28(e++, p->h0, p->h2 + i);
	}
}
