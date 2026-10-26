typedef struct { char pad[0xE8]; short a; short b; short c; } Obj;
void func_80032488(Obj *o, short a, short b, short c) { o->a = a; o->b = b; o->c = c; }
