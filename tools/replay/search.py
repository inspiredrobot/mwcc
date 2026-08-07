import json,itertools
CAP='/private/tmp/mncharsel-capture'
b=json.load(open(f'{CAP}/coloring-0016-gpr-01-before.json'))
a=json.load(open(f'{CAP}/coloring-0016-gpr-01-after.json'))
bn={n['virtual_register']:n for n in b['nodes']}
an={n['virtual_register']:n for n in a['nodes']}
order=b['simplify_order']
VOL=[0,3,4,5,6,7,8,9,10,11,12]; CLAIM=list(range(31,13,-1))
def replay(order):
    color={vr:vr for vr in bn if vr<32}; mask=0
    for c in VOL: mask|=(1<<c)
    claimed=set()
    for vr in order:
        avail=mask
        for nb in bn[vr]['neighbors']:
            c=color.get(nb,-1)
            if 0<=c<32: avail&=~(1<<c)
        if avail:
            for c in range(32):
                if avail&(1<<c): color[vr]=c;break
        else:
            for c in CLAIM:
                if c not in claimed: color[vr]=c;claimed.add(c);mask|=(1<<c);break
    return color
# target
TGT={61:31,42:27,41:30}
# webs in early slots 0..6
early=[order[i] for i in range(7)]
print("early slot webs:",early)
slots=list(range(7))
found=0
for perm in itertools.permutations(early):
    o=list(order)
    for s,w in zip(slots,perm): o[s]=w
    col=replay(o)
    if all(col.get(v)==TGT[v] for v in TGT):
        found+=1
        if found<=5:
            print("HIT perm=",perm,"-> 41:",col[41],"42:",col[42],"61:",col[61],
                  "128:",col.get(128),"54:",col.get(54),"43:",col.get(43))
print("total hits:",found)
