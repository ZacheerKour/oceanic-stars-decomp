import struct,sys
from capstone import *
d=open('extracted/SCUS_944.21','rb').read()
base=0x80010000; code=d[0x800:]
md=Cs(CS_ARCH_MIPS,CS_MODE_MIPS32+CS_MODE_LITTLE_ENDIAN)
def dis(a,n):
    o=a-base
    for i in md.disasm(code[o:o+n*4],a): print(f"{i.address:08x}  {code[i.address-base:i.address-base+4].hex()}  {i.mnemonic} {i.op_str}")
def words(): return struct.unpack('<%dI'%(len(code)//4),code)
if __name__=='__main__': dis(int(sys.argv[1],16),int(sys.argv[2]))
