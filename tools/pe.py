#!/usr/bin/env python3

import argparse
import json
import struct
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class Section:
    name: str
    virtual_address: int
    virtual_size: int
    file_offset: int
    file_size: int
    characteristics: int


class PEFile:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        if self.data[:2] != b"MZ":
            raise ValueError(f"{path} has no DOS header")
        pe_offset = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe_offset : pe_offset + 4] != b"PE\0\0":
            raise ValueError(f"{path} has no PE header")

        coff = pe_offset + 4
        self.machine, section_count, self.timestamp = struct.unpack_from(
            "<HHI", self.data, coff
        )
        optional_size = struct.unpack_from("<H", self.data, coff + 16)[0]
        optional = coff + 20
        if struct.unpack_from("<H", self.data, optional)[0] != 0x10B:
            raise ValueError("only PE32 is supported")
        self.entry_point = struct.unpack_from("<I", self.data, optional + 16)[0]
        self.image_base = struct.unpack_from("<I", self.data, optional + 28)[0]

        section_table = optional + optional_size
        sections = []
        for index in range(section_count):
            offset = section_table + index * 40
            name = self.data[offset : offset + 8].split(b"\0", 1)[0].decode("ascii")
            virtual_size, rva, file_size, file_offset = struct.unpack_from(
                "<IIII", self.data, offset + 8
            )
            characteristics = struct.unpack_from("<I", self.data, offset + 36)[0]
            sections.append(
                Section(
                    name=name,
                    virtual_address=self.image_base + rva,
                    virtual_size=virtual_size,
                    file_offset=file_offset,
                    file_size=file_size,
                    characteristics=characteristics,
                )
            )
        self.sections = tuple(sections)

    def section_for_address(self, address: int) -> Section:
        for section in self.sections:
            size = max(section.virtual_size, section.file_size)
            if section.virtual_address <= address < section.virtual_address + size:
                return section
        raise ValueError(f"address 0x{address:x} is not in a PE section")

    def address_to_offset(self, address: int) -> int:
        section = self.section_for_address(address)
        delta = address - section.virtual_address
        if delta >= section.file_size:
            raise ValueError(f"address 0x{address:x} has no file-backed data")
        return section.file_offset + delta

    def offset_to_address(self, offset: int) -> int:
        for section in self.sections:
            if section.file_offset <= offset < section.file_offset + section.file_size:
                return section.virtual_address + offset - section.file_offset
        raise ValueError(f"file offset 0x{offset:x} is not in a section")

    def read(self, address: int, size: int) -> bytes:
        offset = self.address_to_offset(address)
        return self.data[offset : offset + size]

    def find(self, needle: bytes) -> list[int]:
        addresses = []
        start = 0
        while True:
            offset = self.data.find(needle, start)
            if offset < 0:
                return addresses
            try:
                addresses.append(self.offset_to_address(offset))
            except ValueError:
                pass
            start = offset + 1

    def pointer_xrefs(self, address: int, section_name: str = ".text") -> list[int]:
        encoded = struct.pack("<I", address)
        section = next(section for section in self.sections if section.name == section_name)
        data = self.data[
            section.file_offset : section.file_offset + section.file_size
        ]
        xrefs = []
        start = 0
        while True:
            offset = data.find(encoded, start)
            if offset < 0:
                return xrefs
            xrefs.append(section.virtual_address + offset)
            start = offset + 1


def load_config(path: Path) -> tuple[dict, Path]:
    with path.open(encoding="utf-8") as stream:
        config = json.load(stream)
    return config, Path(config["original"])


def parse_int(value: str) -> int:
    return int(value, 0)


def write_output(value: object, output: Path | None) -> None:
    text = json.dumps(value, indent=2) + "\n"
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


def main() -> None:
    parser = argparse.ArgumentParser(description="Inspect the configured MWCC PE")
    subparsers = parser.add_subparsers(dest="command", required=True)

    info = subparsers.add_parser("info")
    info.add_argument("--config", type=Path, required=True)
    info.add_argument("--output", type=Path)

    find = subparsers.add_parser("find-string")
    find.add_argument("--config", type=Path, required=True)
    find.add_argument("text")

    find_bytes = subparsers.add_parser("find-bytes")
    find_bytes.add_argument("--config", type=Path, required=True)
    find_bytes.add_argument("hex_bytes")

    read = subparsers.add_parser("read")
    read.add_argument("--config", type=Path, required=True)
    read.add_argument("address", type=parse_int)
    read.add_argument("size", type=parse_int)

    xrefs = subparsers.add_parser("xrefs")
    xrefs.add_argument("--config", type=Path, required=True)
    xrefs.add_argument("address", type=parse_int)

    args = parser.parse_args()
    config, original = load_config(args.config)
    pe = PEFile(original)

    if args.command == "info":
        write_output(
            {
                "path": str(original),
                "version": config["version"],
                "machine": pe.machine,
                "timestamp": pe.timestamp,
                "image_base": pe.image_base,
                "entry_point": pe.image_base + pe.entry_point,
                "sections": [asdict(section) for section in pe.sections],
            },
            args.output,
        )
    elif args.command == "find-string":
        for address in pe.find(args.text.encode("latin-1")):
            print(f"0x{address:08x}")
    elif args.command == "find-bytes":
        needle = bytes.fromhex(args.hex_bytes)
        for address in pe.find(needle):
            print(f"0x{address:08x}")
    elif args.command == "read":
        print(pe.read(args.address, args.size).hex(" "))
    elif args.command == "xrefs":
        for address in pe.pointer_xrefs(args.address):
            print(f"0x{address:08x}")


if __name__ == "__main__":
    main()
