typedef struct { char pad[0xF8]; short *lo; short *hi; unsigned short off; } Obj;
int func_80032984(Obj *o) { return (short)(o->off + (((char*)o->hi - (char*)o->lo) >> 1) * 4); }
