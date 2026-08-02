#!/usr/bin/env python3

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from coff import COFFArchive, COFFFile  # noqa: E402


def make_object() -> bytes:
    header_size = 20
    section_size = 40
    code_offset = header_size + section_size
    symbol_offset = code_offset + 1
    header = struct.pack("<HHIIIHH", 0x14C, 1, 0, symbol_offset, 1, 0, 0)
    section = bytearray(section_size)
    section[:8] = b".text\0\0\0"
    struct.pack_into("<III", section, 16, 1, code_offset, 0)
    symbol = struct.pack("<8sIhHBB", b"_leaf\0\0\0", 0, 1, 0x20, 2, 0)
    return header + bytes(section) + b"\xc3" + symbol + struct.pack("<I", 4)


def archive_member(name: str, data: bytes) -> bytes:
    header = (
        name.ljust(16)
        + "0".ljust(12)
        + "0".ljust(6)
        + "0".ljust(6)
        + "100644".ljust(8)
        + str(len(data)).ljust(10)
        + "`\n"
    ).encode("ascii")
    return header + data + (b"\n" if len(data) & 1 else b"")


def main() -> None:
    object_data = make_object()
    functions = COFFFile(Path("leaf.obj"), object_data).functions()
    assert len(functions) == 1
    assert functions[0].name == "_leaf"
    assert functions[0].data == b"\xc3"
    assert not functions[0].has_relocations

    archive_data = b"!<arch>\n" + archive_member("leaf.obj/", object_data)
    members = COFFArchive(Path("sample.lib"), archive_data).members()
    assert len(members) == 1
    assert members[0].name == "leaf.obj"
    assert members[0].data == object_data

    print("COFF parser tests passed")


if __name__ == "__main__":
    main()
