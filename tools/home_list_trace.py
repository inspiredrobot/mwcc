#!/usr/bin/env python3
"""Explain a mwcc-local-home-list-v1 capture: for each local in the codegen
home-reservation list, show its name, kind, register-info physical_register
(committed register, or 0 = homed) and whether a stack spill-home is reserved.
Usage: home_list_trace.py home-list-NNNN.json [--compare other.json]"""
import argparse, binascii, json, re


def names_from(obj):
    if not obj:
        return []
    hexd = obj.get("opaque_value_0a_data") or ""
    try:
        b = binascii.unhexlify(hexd)
    except binascii.Error:
        return []
    return [m.decode() for m in re.findall(rb"[ -~]{3,}", b)]


def phys(reg):
    return None if reg is None else reg.get("physical_register_24")


def rows(doc):
    out = []
    for e in doc["entries"]:
        obj = e["object"]
        nm = names_from(obj)
        name = nm[0] if nm else "?"
        r26 = phys(obj.get("register_info_26")) if obj else None
        r2e = phys(obj.get("register_info_2e")) if obj else None
        kind = obj.get("kind_02") if obj else None
        tk = (obj.get("type") or {}).get("kind_00") if obj else None
        tsz = (obj.get("type") or {}).get("size_02") if obj else None
        # homing checks the reg-info selected by kind; report both, and the
        # effective committed reg = first non-None/non-zero seen.
        committed = None
        for r in (r26, r2e):
            if r:
                committed = r
                break
        homed = not committed
        out.append(
            dict(name=name, kind=kind, type_kind=tk, type_size=tsz,
                 reg26=r26, reg2e=r2e, committed=committed, homed=homed,
                 all_names=nm, addr=e["object_address"])
        )
    return out


def show(doc, title):
    print(f"== {title}: {len(doc['entries'])} locals (capture {doc['capture_index']}) ==")
    print(f"{'name':12} {'kind':4} {'tkind':5} {'tsz':3} {'reg26':6} {'reg2e':6} {'homed'}")
    for r in rows(doc):
        print(f"{r['name']:12} {str(r['kind']):4} {str(r['type_kind']):5} "
              f"{str(r['type_size']):3} {str(r['reg26']):6} {str(r['reg2e']):6} "
              f"{'HOME' if r['homed'] else 'reg'+str(r['committed'])}"
              + ('' if r['name'] != '?' else '  names=' + str(r['all_names'])))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("--compare")
    a = ap.parse_args()
    show(json.load(open(a.trace)), a.trace.split("/")[-1])
    if a.compare:
        print()
        show(json.load(open(a.compare)), a.compare.split("/")[-1])


if __name__ == "__main__":
    main()
