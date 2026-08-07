#!/usr/bin/env python3
"""Exact replay of MWCC Coloring_SimplifyGraph + color selection from a
coloring-NNNN-gpr-01-before.json capture, with hypothesis-edge search support.

Validated 2026-08-07 (grHomeRun_8021CB20 match): full simplify_order
reproduction with K=29 on grhomerun (three source variants) and
ftCh_Wait1_0_Anim.

Model (from src/backend/Coloring.c):
- Nodes in simplify_order are the active (simplifiable) webs. Web nodes NOT in
  simplify_order are coalesced-to-physical blockers (flags=4); together with
  precolored nodes (<32) they are PERMANENT: they never simplify, never
  decrement, and block their color during selection.
- degree init = len(ALL neighbors) (permanent + active).
- Repeated ascending-vreg scans remove nodes with dynamic degree < K=29,
  decrementing ACTIVE neighbors immediately (in-scan); removed nodes push a
  LIFO stack, so pop (select) order = reverse removal. If nothing is removable
  (jam), remove min spill_cost/degree and resume (spill costs stay 0 in the
  snapshot unless a jam happened).
- A web whose PERMANENT degree keeps its dynamic degree >= K survives into a
  later pass and pops earlier; the longest survivor pops first and claims r31.
- Selection: mask starts as volatiles {r0,r3..r12}; callee-saved claimed on
  demand r31 down; each pop takes the lowest set bit of (mask minus neighbor
  colors), where permanent neighbors block their fixed color.

Usage:
  simplify_replay.py CAPTURE_DIR IDX                 # validate + show pop/colors
  simplify_replay.py CAPTURE_DIR IDX vr:+n [vr:+n]   # hypothesis: add n
                                                     # permanent edges to vr
The hypothesis mode answers "which web needs how much extra permanent degree"
before hunting the source shape (e.g. a staged load-backed call-arg local that
coalesces into its argument register = +1 permanent edge for every web live in
its window).
"""
import json
import sys

K = 29
VOLATILES = [0] + list(range(3, 13))


def load(cap, idx):
    b = json.load(open(f"{cap}/coloring-{idx}-gpr-01-before.json"))
    try:
        a = json.load(open(f"{cap}/coloring-{idx}-gpr-01-after.json"))
    except FileNotFoundError:
        a = None
    return b, a


def replay(before, extra_perm=None):
    """Return (pop_order, colors). extra_perm: {vreg: extra permanent degree}."""
    extra_perm = extra_perm or {}
    order = before["simplify_order"]
    active = set(order)
    nodes = {n["virtual_register"]: n for n in before["nodes"]}

    deg, neigh = {}, {}
    for v in order:
        nb = nodes[v]["neighbors"]
        deg[v] = len(nb) + extra_perm.get(v, 0)
        neigh[v] = [x for x in nb if x in active]

    removed, stack = set(), []
    while True:
        changed = True
        while changed:
            changed = False
            for v in sorted(deg):
                if v in removed:
                    continue
                if deg[v] < K:
                    removed.add(v)
                    stack.append(v)
                    for w in neigh[v]:
                        if w not in removed:
                            deg[w] -= 1
                    changed = True
        remaining = [v for v in deg if v not in removed]
        if not remaining:
            break
        cand = min(
            remaining,
            key=lambda v: (nodes[v].get("spill_cost", 0) / deg[v]) if deg[v] else 1e9,
        )
        removed.add(cand)
        stack.append(cand)
        for w in neigh[cand]:
            if w not in removed:
                deg[w] -= 1
    pop = list(reversed(stack))

    color = {p: p for p in range(32)}
    for n in before["nodes"]:
        v = n["virtual_register"]
        if v >= 32 and v not in active:
            color[v] = n["physical_register"]
    mask = set(VOLATILES)
    next_claim = 31
    for v in pop:
        blocked = {color[nb] % 32 for nb in nodes[v]["neighbors"] if nb in color}
        avail = sorted(c for c in mask if c not in blocked)
        if not avail:
            while next_claim in mask:
                next_claim -= 1
            mask.add(next_claim)
            avail = sorted(c for c in mask if c not in blocked)
        color[v] = avail[0] if avail else -1
    return pop, color


def main():
    cap, idx = sys.argv[1], sys.argv[2]
    extra = {}
    for arg in sys.argv[3:]:
        vr, n = arg.split(":")
        extra[int(vr)] = int(n.lstrip("+"))
    before, after = load(cap, idx)
    pop, color = replay(before)
    ok = pop == before["simplify_order"]
    print(f"baseline replay {'VALID' if ok else 'DIVERGES'} "
          f"({sum(1 for a, b in zip(pop, before['simplify_order']) if a == b)}"
          f"/{len(pop)} rows)")
    if after:
        an = {n["virtual_register"]: n["physical_register"] for n in after["nodes"]}
        bad = [v for v in pop if an.get(v, -1) >= 0 and color[v] != an[v]]
        print(f"color replay: {len(pop) - len(bad)}/{len(pop)} match captured")
    head = pop[:10]
    print("pop head:", ", ".join(f"vr{v}->r{color[v]}" for v in head))
    if extra:
        pop2, color2 = replay(before, extra)
        print(f"hypothesis {extra}:")
        print("pop head:", ", ".join(f"vr{v}->r{color2[v]}" for v in pop2[:10]))


if __name__ == "__main__":
    main()
