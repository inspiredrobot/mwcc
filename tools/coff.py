#!/usr/bin/env python3

import argparse
import json
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

from pe import PEFile, load_config


@dataclass(frozen=True)
class Section:
    name: str
    data: bytes
    relocation_offsets: tuple[int, ...]


@dataclass(frozen=True)
class Symbol:
    name: str
    value: int
    section_number: int
    symbol_type: int
    storage_class: int


@dataclass(frozen=True)
class Function:
    name: str
    section: str
    offset: int
    data: bytes
    has_relocations: bool


class COFFFile:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        if len(self.data) < 20:
            raise ValueError(f"{path}: truncated COFF header")
        (
            self.machine,
            section_count,
            self.timestamp,
            symbol_offset,
            symbol_count,
            optional_size,
            _characteristics,
        ) = struct.unpack_from("<HHIIIHH", self.data, 0)
        if self.machine != 0x014C:
            raise ValueError(f"{path}: expected i386 COFF, got 0x{self.machine:04x}")

        string_offset = symbol_offset + symbol_count * 18
        if string_offset + 4 <= len(self.data):
            string_size = struct.unpack_from("<I", self.data, string_offset)[0]
            self.strings = self.data[string_offset : string_offset + string_size]
        else:
            self.strings = b""

        self.sections = []
        section_offset = 20 + optional_size
        for index in range(section_count):
            offset = section_offset + index * 40
            raw_name = self.data[offset : offset + 8]
            name = self._name(raw_name)
            raw_size, raw_offset, relocation_offset = struct.unpack_from(
                "III", self.data, offset + 16
            )
            relocation_count = struct.unpack_from("<H", self.data, offset + 32)[0]
            relocations = tuple(
                struct.unpack_from("<I", self.data, relocation_offset + item * 10)[0]
                for item in range(relocation_count)
            )
            self.sections.append(
                Section(
                    name=name,
                    data=self.data[raw_offset : raw_offset + raw_size],
                    relocation_offsets=relocations,
                )
            )

        self.symbols = []
        index = 0
        while index < symbol_count:
            offset = symbol_offset + index * 18
            raw_name = self.data[offset : offset + 8]
            value, section_number, symbol_type = struct.unpack_from(
                "<IhH", self.data, offset + 8
            )
            storage_class, aux_count = struct.unpack_from("BB", self.data, offset + 16)
            self.symbols.append(
                Symbol(
                    name=self._name(raw_name),
                    value=value,
                    section_number=section_number,
                    symbol_type=symbol_type,
                    storage_class=storage_class,
                )
            )
            index += 1 + aux_count

    def _name(self, raw: bytes) -> str:
        if raw[:4] == b"\0\0\0\0":
            offset = struct.unpack_from("<I", raw, 4)[0]
            end = self.strings.find(b"\0", offset)
            if end < 0:
                end = len(self.strings)
            return self.strings[offset:end].decode("latin-1")
        return raw.split(b"\0", 1)[0].decode("latin-1")

    def functions(self) -> list[Function]:
        by_section: dict[int, list[Symbol]] = {}
        for symbol in self.symbols:
            if symbol.section_number <= 0 or symbol.storage_class != 2:
                continue
            section = self.sections[symbol.section_number - 1]
            if section.name != ".text":
                continue
            by_section.setdefault(symbol.section_number, []).append(symbol)

        functions = []
        for section_number, symbols in by_section.items():
            section = self.sections[section_number - 1]
            symbols.sort(key=lambda symbol: symbol.value)
            for index, symbol in enumerate(symbols):
                end = (
                    symbols[index + 1].value
                    if index + 1 < len(symbols)
                    else len(section.data)
                )
                if end <= symbol.value:
                    continue
                functions.append(
                    Function(
                        name=symbol.name,
                        section=section.name,
                        offset=symbol.value,
                        data=section.data[symbol.value:end],
                        has_relocations=any(
                            symbol.value <= relocation < end
                            for relocation in section.relocation_offsets
                        ),
                    )
                )
        return functions


def object_paths(inputs: list[Path]) -> list[Path]:
    paths = []
    for path in inputs:
        if path.is_dir():
            paths.extend(sorted(path.rglob("*.obj")))
        else:
            paths.append(path)
    return paths


def main() -> None:
    parser = argparse.ArgumentParser(description="Inspect i386 COFF objects")
    subparsers = parser.add_subparsers(dest="command", required=True)

    functions = subparsers.add_parser("functions")
    functions.add_argument("object", type=Path)

    match = subparsers.add_parser("match-pe")
    match.add_argument("--config", type=Path, required=True)
    match.add_argument("--min-bytes", type=int, default=8)
    match.add_argument("inputs", type=Path, nargs="+")
    args = parser.parse_args()

    if args.command == "functions":
        coff = COFFFile(args.object)
        print(
            json.dumps(
                [
                    {
                        **asdict(function),
                        "data": function.data.hex(),
                    }
                    for function in coff.functions()
                ],
                indent=2,
            )
        )
        return

    _config, original = load_config(args.config)
    pe = PEFile(original)
    matches = []
    for path in object_paths(args.inputs):
        try:
            functions = COFFFile(path).functions()
        except (ValueError, struct.error) as error:
            print(error, file=sys.stderr)
            continue
        for function in functions:
            if function.has_relocations or len(function.data) < args.min_bytes:
                continue
            addresses = [
                address
                for address in pe.find(function.data)
                if pe.section_for_address(address).name == ".text"
            ]
            for address in addresses:
                matches.append(
                    {
                        "object": str(path),
                        "function": function.name,
                        "size": len(function.data),
                        "address": f"0x{address:08x}",
                        "bytes": function.data.hex(),
                    }
                )
    matches.sort(key=lambda value: (-value["size"], value["address"]))
    print(json.dumps(matches, indent=2))


if __name__ == "__main__":
    main()
