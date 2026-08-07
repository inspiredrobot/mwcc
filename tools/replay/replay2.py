import json,sys
CAP="/tmp/scratch/vi1201/cap"
idx=sys.argv[1] if len(sys.argv)>1 else "0016"
b=json.load(open(f"{CAP}/coloring-{idx}-gpr-01-before.json"))
a=json.load(open(f"{CAP}/coloring-{idx}-gpr-01-after.json"))
bn={n['virtual_register']:n for n in b['nodes']}
an={n['virtual_register']:n for n in a['nodes']}
order=b['simplify_order']

VOLATILE=[0,3,4,5,6,7,8,9,10,11,12]      # initial available
CLAIM=list(range(31,13,-1))               # r31..r14 high-to-low

def replay(order):
    color={vr:vr for vr in bn if vr<32}
    mask=0
    for c in VOLATILE: mask|=(1<<c)
    claimed=set()
    for vr in order:
        n=bn[vr]
        avail=mask
        for nb in n['neighbors']:
            c=color.get(nb,-1)
            if 0<=c<32: avail &= ~(1<<c)
        if avail!=0:
            for c in range(32):
                if avail&(1<<c): color[vr]=c;break
        else:
            for c in CLAIM:
                if c not in claimed:
                    color[vr]=c;claimed.add(c);mask|=(1<<c);break
    return color

ref={vr:an[vr]['physical_register'] for vr in order}
col=replay(order)
mism=[(vr,col[vr],ref[vr]) for vr in order if col.get(vr)!=ref[vr]]
print("mismatches:",len(mism))
for vr,c,r in mism[:20]: print("  vr",vr,"got r",c,"ref r",r)
