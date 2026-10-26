import struct,iso
M=0xffffffff
def load(D):
    b=D.read(300,0x6000)
    w=list(struct.unpack('<%dI'%0x1200,b[:0x4800])); sz=bytearray(b[0x4800:0x4800+0x1200])
    a=0x13578642
    for i in range(0x1200):
        w[i]^=a; a=(a^(a<<1))&M
        sz[i]^=a&0xff; a=(~a&M)^0x13578642
    return w,sz
def bcd(x): return (x>>4)*10+(x&15)
def ent(w,sz,i):
    v=w[i]; m,s,f,hi=bcd(v&255),bcd(v>>8&255),bcd(v>>16&255),v>>24
    lba=(m*60+s)*75+f-150
    return lba,(hi<<8|sz[i])
if __name__=='__main__':
    D=iso.Disc("roms/disc1/Star Ocean - The Second Story (USA) (Disc 1).bin")
    w,sz=load(D)
    print([hex(x) for x in w[:8]],list(sz[:8]))
    for i in range(12): print(i,ent(w,sz,i))
