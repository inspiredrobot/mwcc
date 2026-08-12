#!/usr/bin/env python3
"""Quick capstone disassembly / call-xref helper for the MWCC PE."""
import sys, struct, json
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

PE = Path("~/etc/mwcc/orig/GC_1_2_5n/mwcceppc.exe")
data = PE.read_bytes()
pe_off = struct.unpack_from("<I", data, 0x3C)[0]
coff = pe_off + 4
nsec = struct.unpack_from("<H", data, coff + 2)[0]
opt = coff + 20
image_base = struct.unpack_from("<I", data, opt + 28)[0]
sec_off = opt + struct.unpack_from("<H", data, coff + 16)[0]
secs = []
for i in range(nsec):
    b = sec_off + i * 40
    name = data[b:b+8].rstrip(b"\0").decode()
    vsize, va, rsize, roff = struct.unpack_from("<IIII", data, b + 8)
    secs.append((name, va + image_base, vsize, roff, rsize))

def va_to_off(va):
    for name, sva, vsize, roff, rsize in secs:
        if sva <= va < sva + vsize:
            return roff + (va - sva)
    return None

def read(va, n):
    o = va_to_off(va)
    return data[o:o+n] if o is not None else b""

md = Cs(CS_ARCH_X86, CS_MODE_32)

def text_range():
    for name, sva, vsize, roff, rsize in secs:
        if name == ".text":
            return sva, roff, rsize
    return None

def find_calls(target):
    """Find direct `call rel32` (E8) and `jmp rel32` (E9) to target VA."""
    sva, roff, rsize = text_range()
    hits = []
    for i in range(rsize - 5):
        b = data[roff + i]
        if b in (0xE8, 0xE9):
            rel = struct.unpack_from("<i", data, roff + i + 1)[0]
            src = sva + i
            dst = src + 5 + rel
            if dst == target:
                hits.append((src, "call" if b == 0xE8 else "jmp"))
    return hits

def disasm(va, n=200):
    code = read(va, n)
    for ins in md.disasm(code, va):
        print(f"  0x{ins.address:08x}: {ins.mnemonic:<7} {ins.op_str}")

if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "xref":
        for src, kind in find_calls(int(sys.argv[2], 16)):
            print(f"0x{src:08x} {kind}")
    elif cmd == "dis":
        disasm(int(sys.argv[2], 16), int(sys.argv[3]) if len(sys.argv) > 3 else 200)
