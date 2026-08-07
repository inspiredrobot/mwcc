"""Search select (pop) orders of the callee-saved GPR subgraph that reproduce
a target (retail) color assignment, and report order invariants.

The full-order replay model (validated): initial mask = volatiles {r0,r3..r12};
callee-saved claimed on demand r31..r14; otherwise lowest set bit of
(mask & ~neighbor_colors). For webs whose volatile colors are all blocked
(every callee-saved-colored web qualifies), colors depend only on the
callee-saved subgraph and the claim state, so the replay can be restricted to
those nodes: validate first (subgraph replay of the captured order must equal
captured colors), then DFS over pop orders pruning any assignment that
diverges from the retail map.

Usage: cs_subgraph_dfs.py <capture_dir> <idx> <retail_map.json>
  retail_map.json: {"57": 31, "60": 28, ...}  vreg -> retail physical register
  (only callee-saved webs, 26..31 etc.; must cover exactly the webs whose
  captured after-color is callee-saved)
"""
import json, sys

def load(cap, idx):
    b = json.load(open(f"{cap}/coloring-{idx}-gpr-01-before.json"))
    a = json.load(open(f"{cap}/coloring-{idx}-gpr-01-after.json"))
    bn = {n['virtual_register']: n for n in b['nodes']}
    an = {n['virtual_register']: n for n in a['nodes']}
    return bn, an, b['simplify_order']

def color_seq(seq, NB):
    color, claimed = {}, set()
    for vr in seq:
        blocked = set(color[nb] for nb in NB[vr] if nb in color)
        avail = sorted(c for c in claimed if c not in blocked)
        if avail:
            color[vr] = avail[0]
        else:
            c = 31
            while c in claimed:
                c -= 1
            color[vr] = c
            claimed.add(c)
    return color

def main():
    cap, idx, mapfile = sys.argv[1], sys.argv[2], sys.argv[3]
    TGT = {int(k): v for k, v in json.load(open(mapfile)).items()}
    bn, an, order = load(cap, idx)
    CS = [vr for vr in order if an[vr]['physical_register'] >= 14
          and vr >= 32 and an[vr]['physical_register'] < 32
          and an[vr]['physical_register'] >= 14]
    CS = [vr for vr in CS if an[vr]['physical_register'] >= min(TGT.values())]
    assert set(CS) == set(TGT), (sorted(CS), sorted(TGT))
    NB = {v: set(u for u in bn[v]['neighbors'] if u in TGT) for v in CS}
    # validate restricted replay against the capture itself
    cand = color_seq(CS, NB)
    bad = [(v, cand[v], an[v]['physical_register']) for v in CS
           if cand[v] != an[v]['physical_register']]
    if bad:
        print("SUBGRAPH REPLAY INVALID:", bad)
        sys.exit(1)
    hits = []
    LIMIT = 200000
    def dfs(seq, remaining, color, claimed):
        if not remaining:
            hits.append(tuple(seq)); return
        if len(hits) >= LIMIT: return
        for vr in list(remaining):
            blocked = set(color[nb] for nb in NB[vr] if nb in color)
            avail = sorted(c for c in claimed if c not in blocked)
            if avail:
                c, newclaim = avail[0], False
            else:
                c = 31
                while c in claimed:
                    c -= 1
                newclaim = True
            if c != TGT[vr]:
                continue
            color[vr] = c
            if newclaim: claimed.add(c)
            seq.append(vr); remaining.remove(vr)
            dfs(seq, remaining, color, claimed)
            seq.pop(); remaining.add(vr); del color[vr]
            if newclaim: claimed.discard(c)
    dfs([], set(CS), {}, set())
    print("retail-producing orders:", len(hits), "(capped)" if len(hits) >= LIMIT else "")
    if not hits:
        print("UNREACHABLE by pop order alone -> edge/web difference (terminal or structural)")
        return
    idxs = [{v: i for i, v in enumerate(h)} for h in hits]
    print("always-before pairs:")
    for x in CS:
        for y in CS:
            if x != y and all(ix[x] < ix[y] for ix in idxs):
                print(f"  {x} < {y}")
    print("position ranges:")
    for v in CS:
        ps = [ix[v] for ix in idxs]
        print(f"  vr{v}: [{min(ps)},{max(ps)}]")

if __name__ == '__main__':
    main()
