typedef struct { char pad[0xF8]; short *base; short *cur; short rem; short pad2[7]; short pos; } Obj;
void func_80032950(Obj *o, int a) { int q; o->pos = a; q = a / 4; o->rem = a - q * 4; o->cur = o->base + q; }
