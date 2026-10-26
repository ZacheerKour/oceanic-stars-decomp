/* Shared declarations for the main overlay (entry 1). Names are address-derived; layouts are inferred from accesses only. */
typedef struct Node { int pad0; struct Node *next; } Node;
typedef struct { char pad[2]; unsigned char b2; char pad3[0xD]; int w10; } Ctx;

extern unsigned short *D_8004DDA4;
extern void *D_8004DDB8;
extern Ctx *D_8004DDCC;
extern void *D_8004B5A8, *D_8004B838, *D_8004B898;
extern short D_8004EFBC;
extern void *D_8004EFC4, *D_8004EBE0;

void func_8001FD70();
void func_80023F40();
void func_80030FC8();
void func_80031008();
void func_80031CC8();
void func_80031E94();
void func_8006AB68();
void *func_80012108();
int func_8003469C();
int func_80034650();
int func_80034EF8();
int func_8003B8C4();
void func_8003B638();
void func_8003B83C();
void func_8003B5DC();
void func_80038760();
void func_80038854();
void func_8003867C();
void func_80035AA4();
void func_800367B4();

/* lane 3 */
typedef struct { unsigned short h0; short h2; short h4; short h6; short h8; short ha; } S_8006D588;
typedef struct { char pad0[0x2C]; unsigned short h2c; unsigned short h2e; unsigned short h30; char pad32[4]; short h36; } S_8006F070;
typedef struct { char pad0[2]; unsigned short h2; } S_80073418b;
typedef struct { char pad0[0x54]; S_80073418b *p54; } S_80073418;
extern S_8006D588 *D_80074BCC;
extern short D_80074BD8, D_80074BDC;
extern int D_80074BE0;
void func_800492D0();
typedef struct { char pad0[0x1C]; unsigned short h1c; } S_8006E7AC;
typedef struct { char pad0[0x14]; short h14; short h16; char pad18[0x14]; short h2c; short h2e; short h30; char pad32[0x1E]; int w50; int w54; int pad58; int w5c; } S_8006E7E4;
typedef struct { short h0; short h2; short h4; short h6; short h8; } S_8006DAC0;
short func_8006DB24();
typedef struct { short x; short y; short w; short h; } S_8006E1A0;
typedef struct { short h0; short h2; S_8006E1A0 r1; S_8006E1A0 r2; int w14; } S_8006E20C;
typedef struct {
    char pad0[0x18]; unsigned short h18; unsigned short h1a; unsigned short h1c; char pad1e[0xA];
    short h28; short h2a; char pad2c[6]; short h32; char pad34[0x14]; int w48; int w4c; char pad50[0x12];
    S_8006E1A0 r62; S_8006E1A0 r6a;
} S_8006EBFC;
void func_8006E20C();
void func_8006EBFC();
typedef struct { char pad0[8]; unsigned char b8[1]; } S_8004A2BC;
extern S_8004A2BC *D_8004EC20;
