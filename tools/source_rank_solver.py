#!/usr/bin/env python3
"""Reversible source-shape solver (SOLVER_ROADMAP "reversible query prototype").

Given a captured coloring graph and a TARGET physical coloring, this tool:
  1. classifies every virtual register by its source origin
     (OBJECT = named-local frontend object, TEMP = compiler temporary,
      COALESCED = folded into a parent);
  2. flags DEAD OBJECT slots -- object-backed vregs that receive no callee/color
     (physical r0) because their runtime value actually lives in a later TEMP
     web (e.g. a `u32* p = call();` local whose value is the call-result temp).
     Each dead object slot still consumes an object-register number and thus
     pushes every compiler TEMP (global-address base, hoisted constants,
     call-result carriers) to a higher virtual-register birth rank;
  3. models base_temp_rank = (highest object vreg) + 1, i.e. reducing the object
     count lowers the first-temp rank;
  4. replays the K=29 simplify/color model over the source-achievable
     renumbering space -- declaration order permutes OBJECT ranks; eliminating a
     dead object slot (inlining its named local) shifts the whole object block
     down by one and lowers every TEMP rank -- and reports whether the TARGET
     becomes reachable and the minimal object-count reduction that unlocks it.

This makes the "which source construct must change" question answerable from a
single capture instead of a manual structural sweep.

Usage:
  source_rank_solver.py CAPTURE_DIR IDX --target vreg:reg,vreg:reg,...
  source_rank_solver.py CAPTURE_DIR IDX --target-file target.json
"""
import json, sys, argparse, random, itertools

K = 29
VOLATILES = [0] + list(range(3, 13))


def load(cap, idx):
    b = json.load(open(f"{cap}/coloring-{idx}-gpr-01-before.json"))
    a = json.load(open(f"{cap}/coloring-{idx}-gpr-01-after.json"))
    p = json.load(open(f"{cap}/pcode-{idx}-scheduled.json"))
    return b, a, p


def classify(before, after, pcode):
    nb = {n["virtual_register"]: n for n in before["nodes"]}
    A = {n["virtual_register"]: n for n in after["nodes"]}
    defmn = {}
    for blk in pcode["blocks"]:
        for x in blk.get("instructions", []):
            ops = x.get("operands", [])
            if ops and ops[0]["kind"] == 0:
                d = ops[0]["reg"]
                if d >= 32 and d not in defmn:
                    defmn[d] = x["opcode_descriptor"]["mnemonic"]
    info = {}
    for v in sorted(nb):
        if v < 32:
            continue
        n = nb[v]
        obj = n.get("object", "0x00000000")
        fl = n.get("flags", 0)
        col = A.get(v, {}).get("physical_register", -1)
        if fl & 4:
            kind = "COALESCED"
        elif obj != "0x00000000":
            kind = "OBJECT"
        else:
            kind = "TEMP"
        info[v] = dict(kind=kind, object=obj, defmn=defmn.get(v, "?"),
                       color=col, flags=fl)
    # dead object slot = OBJECT colored r0 with a value carried by a later TEMP
    for v, i in info.items():
        i["dead_object"] = (i["kind"] == "OBJECT" and i["color"] == 0)
    return info


def build_model(before, after):
    nb = {n["virtual_register"]: n for n in before["nodes"]}
    A = {n["virtual_register"]: n for n in after["nodes"]}
    order = before["simplify_order"]
    active = set(order)
    NB = {v: set(nb[v]["neighbors"]) for v in nb}

    def canon(v):
        seen = set()
        while v >= 32 and v not in seen and (A.get(v, {}).get("flags", 0) & 4):
            seen.add(v)
            v = A[v].get("physical_register", -1)
        return v

    def replay(key):
        deg = {v: len(nb[v]["neighbors"]) for v in order}
        neigh = {v: [x for x in nb[v]["neighbors"] if x in active] for v in order}
        removed, stack = set(), []
        while True:
            ch = True
            while ch:
                ch = False
                for v in sorted(deg, key=lambda w: key.get(w, w)):
                    if v in removed:
                        continue
                    if deg[v] < K:
                        removed.add(v); stack.append(v)
                        for w in neigh[v]:
                            if w not in removed:
                                deg[w] -= 1
                        ch = True
            rem = [v for v in deg if v not in removed]
            if not rem:
                break
            c = min(rem, key=lambda w: key.get(w, w))
            removed.add(c); stack.append(c)
            for w in neigh[c]:
                if w not in removed:
                    deg[w] -= 1
        color = {p: p for p in range(32)}
        mask = set(VOLATILES); nxt = 31
        for v in reversed(stack):
            blocked = {color[canon(x)] % 32 for x in NB[v] if canon(x) in color}
            av = sorted(c for c in mask if c not in blocked)
            if not av:
                while nxt in mask:
                    nxt -= 1
                mask.add(nxt); av = sorted(c for c in mask if c not in blocked)
            color[v] = av[0] if av else -1
        return color

    return order, replay


def solve(cap, idx, target):
    before, after, pcode = load(cap, idx)
    info = classify(before, after, pcode)
    order, replay = build_model(before, after)

    objects = sorted(v for v in info if info[v]["kind"] == "OBJECT")
    temps = sorted(v for v in info if info[v]["kind"] == "TEMP")
    dead = sorted(v for v in info if info[v]["dead_object"])
    top_obj = max(objects) if objects else 31
    base_temp = min(temps) if temps else None

    print(f"objects={len(objects)} (vregs {objects[0]}..{top_obj}), "
          f"first-temp(base)=vr{base_temp}, dead-object-slots={dead}")

    def score(key):
        c = replay(key)
        return sum(1 for v in target if c.get(v) == target[v])

    def named_key(declperm, removed_dead):
        """Model: OBJECT vregs assigned top-down by declaration position, with
        `removed_dead` dead-object slots eliminated (whole object block shifts
        down by len(removed_dead), lowering every TEMP rank by the same)."""
        shift = len(removed_dead)
        key = {}
        live_objs = [v for v in objects if v not in removed_dead]
        # declperm is an ordering of live_objs -> assign descending vregs
        hi = top_obj - shift
        for i, v in enumerate(declperm):
            key[v] = hi - i
        for t in temps:
            key[t] = t - shift  # temps shift down with the object block
        return key

    n = len(target)
    # 0) current source score
    print(f"target size={n}. baseline(identity) score={score({})}/{n}")

    # 1) decl-order search at each dead-slot-elimination level
    for r in range(0, len(dead) + 1):
        best = -1
        combos = list(itertools.combinations(dead, r))
        for rem in combos:
            live_objs = [v for v in objects if v not in rem]
            random.seed(0)
            for _ in range(4000):
                perm = live_objs[:]
                random.shuffle(perm)
                s = score(named_key(perm, set(rem)))
                if s > best:
                    best = s
                if best == n:
                    break
            if best == n:
                new_base = (top_obj - r) + 1
                print(f"  REACHABLE eliminating {r} dead slot(s) {rem} "
                      f"-> base rank {new_base}: {best}/{n}")
                return dict(reachable=True, remove=list(rem), base_rank=new_base)
        print(f"  eliminate {r} dead slot(s): best decl-order score {best}/{n} "
              f"(base rank {(top_obj - r) + 1})")
    return dict(reachable=False)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap")
    ap.add_argument("idx")
    ap.add_argument("--target", help="vreg:reg,vreg:reg,...")
    ap.add_argument("--target-file")
    a = ap.parse_args()
    if a.target_file:
        target = {int(k): v for k, v in json.load(open(a.target_file)).items()}
    else:
        target = {}
        for pair in a.target.split(","):
            k, v = pair.split(":")
            target[int(k)] = int(v)
    solve(a.cap, a.idx, target)


if __name__ == "__main__":
    main()


def creation_order_reachable(cap, idx, target, restarts=80, iters=2500, seed=0):
    """Answer: is TARGET reachable if compiler TEMPS keep their code-creation
    order (ascending vreg) but the temp block may sit at any offset/gap relative
    to the OBJECT block (objects freely permuted by declaration order)?

    Returns (best_score, reachable_bool). If reachable, the remaining question
    is only whether the source's FIXED temp gaps + object-web count can realize
    the needed offset -- i.e. whether the algorithm produces few enough object
    webs. If unreachable even here, the target needs the base temp interleaved
    BELOW object ranks, which MWCC's object-before-temp numbering forbids from
    any declaration order (the base would have to become a named object, adding
    a materialization copy)."""
    before, after, pcode = load(cap, idx)
    order, replay = build_model(before, after)
    nb = {n["virtual_register"]: n for n in before["nodes"]}
    OBJ = [v for v in order if nb[v].get("object", "0x0") != "0x00000000"
           and not (nb[v].get("flags", 0) & 4)]
    TEMPS = sorted(v for v in order if nb[v].get("object", "0x0") == "0x00000000"
                   and not (nb[v].get("flags", 0) & 4))

    def score(key):
        c = replay(key)
        return sum(1 for v in target if c.get(v) == target[v])

    random.seed(seed)
    best = -1
    for st in range(restarts):
        key = {v: random.uniform(0, 60) for v in OBJ}
        toff = random.uniform(0, 80); tgap = random.uniform(1, 15)
        for i, t in enumerate(TEMPS):
            key[t] = toff + i * tgap
        s = score(key)
        for _ in range(iters):
            if random.random() < 0.5:
                v = random.choice(OBJ); old = key[v]
                key[v] = random.uniform(0, 60); s2 = score(key)
                if s2 >= s:
                    s = s2
                else:
                    key[v] = old
            else:
                old = (toff, tgap)
                toff = random.uniform(0, 90); tgap = random.uniform(0.5, 15)
                for i, t in enumerate(TEMPS):
                    key[t] = toff + i * tgap
                s2 = score(key)
                if s2 >= s:
                    s = s2
                else:
                    toff, tgap = old
                    for i, t in enumerate(TEMPS):
                        key[t] = toff + i * tgap
            if s == len(target):
                return len(target), True
        best = max(best, s)
    return best, False
