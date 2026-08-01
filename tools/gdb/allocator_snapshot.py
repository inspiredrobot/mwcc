import json
import sys
from pathlib import Path

import gdb


TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from allocator_snapshot import SnapshotReader


class MwccAllocatorSnapshot(gdb.Command):
    """Write a GC/1.2.5 pre-coloring PCode snapshot: mwcc-snapshot PATH"""

    def __init__(self):
        super().__init__("mwcc-snapshot", gdb.COMMAND_DATA)

    def invoke(self, argument, from_tty):
        del from_tty
        output = Path(argument.strip())
        if not output.name:
            raise gdb.GdbError("usage: mwcc-snapshot PATH")

        inferior = gdb.selected_inferior()
        reader = SnapshotReader(
            lambda address, size: bytes(inferior.read_memory(address, size))
        )
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        function_pointer = reader.u32(stack_pointer + 4)
        program_counter = int(gdb.parse_and_eval("$pc"))
        snapshot = reader.snapshot(function_pointer, program_counter)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("w", encoding="utf-8") as stream:
            json.dump(snapshot, stream, indent=2)
            stream.write("\n")
        gdb.write(f"Wrote {output} with {len(snapshot['blocks'])} blocks\n")


MwccAllocatorSnapshot()
