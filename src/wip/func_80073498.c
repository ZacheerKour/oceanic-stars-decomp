/* 80073498: 106/512 bytes differ; wall class: register assignment. Retail keeps the buf address in s3, max width in s4, height in s5; ours puts them in s5/s3/s4 (buf address pseudo gets lowest priority). Declaration order, b=buf pointer var, ?: all tried. Everything else (struct copies via lwl/lwr, q array stores) lines up. */
#include "so2.h"
void func_80073498(S_80073498 *o)
{
    char buf[256];
    S_8006E1A0 r;
    S_80073498q q;
    int w, m, t;
    short n;
    char *s;

    w = 0;
    m = 0;
    s = func_80032380( o->p54 );
    func_80033FB0( o->p54, 0 );
    while ( 1 ) {
        n = func_80032E18( o->p54, s );
        if ( n == -1 ) break;
        func_80023ED0( buf, s, n );
        buf[n] = 0;
        t = func_800339A8( o->p54, buf );
        if ( m < t ) {
            m = t;
        }
        func_8006F730( o, buf );
        s += n;
        w += o->h30;
    }
    m += 32;
    w += 14;
    r.x = 156 - m / 2;
    r.y = 108 - w / 2;
    r.w = m;
    r.h = w;
    o->r62 = r;
    o->r6A = r;
    o->w48 = o->r6A.x;
    o->w4C = o->r6A.y;
    func_80073698( o );
    func_8006F08C( o, m, w );
    q.v[0].x = -16;
    q.v[0].y = -8;
    q.v[0].u = 232;
    q.v[0].v = 0;
    q.v[0].w6 = 31248;
    q.v[1].x = 0;
    q.v[1].y = -8;
    q.v[1].u = 248;
    q.v[1].v = 0;
    q.v[1].w6 = 22;
    q.v[2].x = -16;
    q.v[2].y = 8;
    q.v[2].u = 232;
    q.v[2].v = 16;
    q.v[3].x = 0;
    q.v[3].y = 8;
    q.v[3].u = 248;
    q.v[3].v = 16;
    func_8006FA7C( o, &q, 8, 4 );
}
