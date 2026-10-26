import struct,sys
class Disc:
    def __init__(s,p): s.f=open(p,'rb'); s.f.seek(0,2); s.n=s.f.tell()//2352
    def sec(s,l):
        s.f.seek(l*2352+24); return s.f.read(2048)
    def read(s,l,size):
        out=bytearray()
        for i in range((size+2047)//2048): out+=s.sec(l+i)
        return bytes(out[:size])
    def ls(s,l,size,path='/'):
        d=s.read(l,size); o=0; res=[]
        while o<len(d):
            n=d[o]
            if n==0: o=(o//2048+1)*2048; continue
            lba,sz=struct.unpack('<I',d[o+2:o+6])[0],struct.unpack('<I',d[o+10:o+14])[0]
            fl=d[o+25]; nm=d[o+33:o+33+d[o+32]].decode('latin1')
            if nm not in('\x00','\x01'): res.append((path+nm,lba,sz,fl))
            o+=n
        return res
    def walk(s):
        pvd=s.sec(16); rl=struct.unpack('<I',pvd[158:162])[0]; rs=struct.unpack('<I',pvd[166:170])[0]
        todo=[(rl,rs,'/')]; allf=[]
        while todo:
            l,z,p=todo.pop()
            for nm,a,b,fl in s.ls(l,z,p):
                allf.append((nm,a,b,fl))
                if fl&2: todo.append((a,b,nm+'/'))
        return pvd[40:72].decode().strip(),allf
