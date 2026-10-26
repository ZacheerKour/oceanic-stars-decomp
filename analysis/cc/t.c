extern short g1; extern unsigned short g2;
unsigned f1(char *a, int i){ return ((((unsigned short*)(a+i*4))[3] & 0xff00) | (unsigned char)a[0x4804+i]) << 11; }
int f2(void){ int v = g1 - 1; if (v < 0) v += (short)g2; return v; }
void f3(unsigned *ctx){
    unsigned a = 0x13578642; int i;
    for (i = 0; i < 0x1200; i++) {
        ctx[i+1] ^= a;
        a ^= a << 1;
        ((unsigned char*)ctx)[0x4804+i] ^= a;
        a = ~a ^ 0x13578642;
    }
}
