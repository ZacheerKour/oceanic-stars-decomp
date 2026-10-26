import struct
def dec(d,off=0):
    assert d[off:off+3]==b'SLZ'
    v=d[off+3]; n=struct.unpack_from('<I',d,off+8)[0]; nxt=struct.unpack_from('<I',d,off+12)[0]
    p=off+16
    if v==0: return d[p:p+n],nxt
    out=bytearray(); flag=0; bits=1
    while True:
        if v==2 and len(out)>=n: break
        bits-=1
        flag>>=1
        if bits==0: bits=8; flag=d[p]; p+=1
        if flag&1: out.append(d[p]); p+=1; continue
        b0,b1=d[p],d[p+1]; f=b0|(b1&15)<<8; ln=(b1>>4)+3
        if v==1:
            if f==0: break
        elif ln==18:
            if f<0x100: fill=d[p+2]; ln=f+0x13; p+=1
            else: fill=f&255; ln=(f>>8)+3
            out+=bytes([fill])*ln; p+=2; continue
        for _ in range(ln): out.append(out[-f])
        p+=2
    return bytes(out),nxt

def dec_all(d):
    """Decode every chunk of a (possibly multi-chunk) entry -> list of bytes. next offset is relative to the chunk start."""
    out=[]; off=0
    while True:
        o,nx=dec(d,off); out.append(o)
        if not nx: return out
        off+=nx
