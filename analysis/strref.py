import sys;sys.path.insert(0,'analysis')
import mdis as D
w=D.words()
targets={0x8002b400:'sector error',0x8002b418:'Shell open',0x8002b190:'CdInit fail',0x8002b3dc:'CD_init',0x8002b3a0:'CD_cw',0x8002b374:'CD_sync',0x8002b3f4:'datasync'}
def s16(v): return v-0x10000 if v>=0x8000 else v
for i,x in enumerate(w):
    if x>>26==15 and (x&0xffff)==0x8002:
        r=(x>>16)&31
        for j in range(i+1,i+8):
            y=w[j]
            if y>>26 in(9,35) and (y>>21)&31==r:
                t=0x80020000+s16(y&0xffff)
                if t in targets: print(hex(0x80010000+j*4),targets[t])
