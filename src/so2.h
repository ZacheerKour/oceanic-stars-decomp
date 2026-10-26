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

/* lane 2 */
typedef struct {
	char pad0[0x78C];
	unsigned short h78C;
	unsigned short h78E;
	char pad790[4];
	short h794;
	char pad796[0x2E];
	short h7C4;
	char pad7C6[6];
} S_8003818C;
typedef struct { int w0; void *p4; } S_80038228;
typedef struct { char pad0[0x5C]; int w5C; char pad60[0x10]; } S_8003BE84;
typedef struct { char pad0[4]; short h4; short h6; char pad8[8]; short h10; short h12; } S_8003C8D8;
typedef struct { char pad0[0x10]; int w10; int w14; int w18; } S_8003E86C;
typedef struct { char pad0[0x26]; unsigned short h26; } S_8003B3D4;

extern S_8003BE84 D_8004DED8[];
extern int D_8004DED4;
extern int D_8004E168[];
extern int D_8004E170;
typedef struct {
	char pad0[0x18];
	int w18;
	char pad1C[0x10];
	short h2C;
	char pad2E[4];
	short h32;
	char pad34[4];
	short h38;
	char pad3A[4];
	short h3E;
	short h40;
	char pad42[2];
	short h44;
} S_8003B024;
extern int D_8004EFB4;
extern short D_8004EFB8, D_8004EFC0, D_8004EFD0;
extern unsigned D_8004E018[][2], D_8004E078[][2];
void func_80036B2C();
void func_80038930();
void func_80038884(int, short, unsigned short);
