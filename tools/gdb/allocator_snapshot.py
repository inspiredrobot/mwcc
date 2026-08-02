import json
import sys
from pathlib import Path

import gdb


TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from allocator_snapshot import TARGET_SHA256, SnapshotReader


ALLOCATE_REGISTERS_ADDRESS = 0x004CDEF0
SELECT_COLORS_ADDRESS = 0x004CE2D0
CODEGEN_GENERATOR_ADDRESS = 0x004351C0
INITIAL_PCODE_ADDRESS = 0x00435B04
OPTIMIZED_PCODE_ADDRESS = 0x00435B39
PCODE_EMIT_ADDRESS = 0x004A25D0
PCODE_CREATE_ADDRESS = 0x004A2620
PCODE_BUILDER_RETURN_ADDRESS = 0x004A2B6D
CURRENT_CODEGEN_ITEM_ADDRESS = 0x00587130

PCODE_WRAPPERS = {
    PCODE_EMIT_ADDRESS: "emit",
    PCODE_CREATE_ADDRESS: "create",
}
REGISTER_CLASS_NAMES = {0: "gpr", 1: "fpr", 9: "vr"}


def snapshot_reader():
    inferior = gdb.selected_inferior()
    return SnapshotReader(
        lambda address, size: bytes(inferior.read_memory(address, size))
    )


def write_snapshot(output, snapshot):
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8") as stream:
        json.dump(snapshot, stream, indent=2)
        stream.write("\n")


class MwccAllocatorSnapshot(gdb.Command):
    """Write a GC/1.2.5 pre-coloring PCode snapshot: mwcc-snapshot PATH"""

    def __init__(self):
        super().__init__("mwcc-snapshot", gdb.COMMAND_DATA)

    def invoke(self, argument, from_tty):
        del from_tty
        output = Path(argument.strip())
        if not output.name:
            raise gdb.GdbError("usage: mwcc-snapshot PATH")

        reader = snapshot_reader()
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        function_pointer = reader.u32(stack_pointer + 4)
        program_counter = int(gdb.parse_and_eval("$pc"))
        snapshot = reader.snapshot(function_pointer, program_counter)
        write_snapshot(output, snapshot)
        gdb.write(f"Wrote {output} with {len(snapshot['blocks'])} blocks\n")


class MwccColoringSnapshot(gdb.Command):
    """Write a GC/1.2.5 coloring graph snapshot: mwcc-coloring-snapshot PATH"""

    def __init__(self):
        super().__init__("mwcc-coloring-snapshot", gdb.COMMAND_DATA)

    def invoke(self, argument, from_tty):
        del from_tty
        output = Path(argument.strip())
        if not output.name:
            raise gdb.GdbError("usage: mwcc-coloring-snapshot PATH")

        reader = snapshot_reader()
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        reg_class = reader.u32(stack_pointer + 4)
        simplify_stack = reader.u32(stack_pointer + 8)
        program_counter = int(gdb.parse_and_eval("$pc"))
        snapshot = reader.coloring_snapshot(
            reg_class, simplify_stack, program_counter
        )
        write_snapshot(output, snapshot)
        gdb.write(f"Wrote {output} with {len(snapshot['nodes'])} nodes\n")


class ColoringReturnBreakpoint(gdb.Breakpoint):
    def __init__(self, session, attempt, reg_class, return_address):
        super().__init__(
            f"*0x{return_address:08x}",
            type=gdb.BP_HARDWARE_BREAKPOINT,
            internal=True,
        )
        self.session = session
        self.function_index = session.function_index
        self.attempt = attempt
        self.reg_class = reg_class

    def stop(self):
        reader = snapshot_reader()
        snapshot = reader.coloring_snapshot(
            self.reg_class,
            0,
            int(gdb.parse_and_eval("$pc")),
        )
        snapshot["capture_index"] = self.function_index
        snapshot["attempt"] = self.attempt
        snapshot["phase"] = "after"
        output = self.session.coloring_path(
            self.function_index, self.reg_class, self.attempt, "after"
        )
        write_snapshot(output, snapshot)
        self.enabled = False
        return False


class ColoringBreakpoint(gdb.Breakpoint):
    def __init__(self, session):
        super().__init__(
            f"*0x{SELECT_COLORS_ADDRESS:08x}",
            type=gdb.BP_HARDWARE_BREAKPOINT,
            internal=True,
        )
        self.session = session

    def stop(self):
        reader = snapshot_reader()
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        reg_class = reader.u32(stack_pointer + 4)
        if reg_class not in REGISTER_CLASS_NAMES:
            return False

        simplify_stack = reader.u32(stack_pointer + 8)
        return_address = reader.u32(stack_pointer)
        attempt = self.session.coloring_attempts.get(reg_class, 0) + 1
        self.session.coloring_attempts[reg_class] = attempt
        snapshot = reader.coloring_snapshot(
            reg_class, simplify_stack, int(gdb.parse_and_eval("$pc"))
        )
        snapshot["capture_index"] = self.session.function_index
        snapshot["attempt"] = attempt
        snapshot["phase"] = "before"
        output = self.session.coloring_path(
            self.session.function_index, reg_class, attempt, "before"
        )
        write_snapshot(output, snapshot)
        ColoringReturnBreakpoint(
            self.session,
            attempt,
            reg_class,
            return_address,
        )
        return False


class AllocateBreakpoint(gdb.Breakpoint):
    def __init__(self, session):
        super().__init__(
            f"*0x{ALLOCATE_REGISTERS_ADDRESS:08x}",
            type=gdb.BP_HARDWARE_BREAKPOINT,
            internal=True,
        )
        self.session = session

    def stop(self):
        reader = snapshot_reader()
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        function_pointer = reader.u32(stack_pointer + 4)
        if not self.session.active:
            self.session.begin_function(function_pointer)
        self.session.coloring_attempts = {}
        snapshot = reader.snapshot(
            function_pointer, int(gdb.parse_and_eval("$pc"))
        )
        snapshot["capture_index"] = self.session.function_index
        output = self.session.output / (
            f"allocator-{self.session.function_index:04d}.json"
        )
        write_snapshot(output, snapshot)
        instruction_count = sum(
            len(block["instructions"]) for block in snapshot["blocks"]
        )
        gdb.write(
            f"Captured allocator {self.session.function_index}: "
            f"{len(snapshot['blocks'])} blocks, {instruction_count} instructions\n"
        )
        return False


class CodeGenBreakpoint(gdb.Breakpoint):
    def __init__(self, session):
        super().__init__(
            f"*0x{CODEGEN_GENERATOR_ADDRESS:08x}",
            internal=True,
        )
        self.session = session

    def stop(self):
        reader = snapshot_reader()
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        function_pointer = reader.u32(stack_pointer + 8)
        self.session.begin_function(function_pointer)
        return False


class PCodeStageBreakpoint(gdb.Breakpoint):
    def __init__(self, session, address, phase, next_epoch):
        super().__init__(f"*0x{address:08x}", internal=True)
        self.session = session
        self.phase = phase
        self.next_epoch = next_epoch

    def stop(self):
        if not self.session.active:
            return False
        self.session.write_pcode_stage(
            self.phase,
            int(gdb.parse_and_eval("$pc")),
        )
        self.session.creation_epoch = self.next_epoch
        return False


class PCodeWrapperBreakpoint(gdb.Breakpoint):
    def __init__(self, session, address):
        super().__init__(f"*0x{address:08x}", internal=True)
        self.session = session
        self.address = address

    def stop(self):
        if not self.session.active:
            return False
        reader = snapshot_reader()
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        return_address = reader.u32(stack_pointer)
        call_address = return_address - 5
        if reader.u8(call_address) != 0xE8:
            call_address = None
        codegen_item = reader.u32(CURRENT_CODEGEN_ITEM_ADDRESS)
        self.session.pending_creations.append(
            {
                "epoch": self.session.creation_epoch,
                "wrapper": PCODE_WRAPPERS[self.address],
                "wrapper_address": f"0x{self.address:08x}",
                "caller_return_address": f"0x{return_address:08x}",
                "call_address": (
                    f"0x{call_address:08x}" if call_address is not None else None
                ),
                "opcode_argument": reader.s16(stack_pointer + 4),
                "codegen_item_address": f"0x{codegen_item:08x}",
                "codegen_item_header": (
                    reader.raw(codegen_item, 0x12).hex() if codegen_item else None
                ),
            }
        )
        return False


class PCodeBuilderReturnBreakpoint(gdb.Breakpoint):
    def __init__(self, session):
        super().__init__(
            f"*0x{PCODE_BUILDER_RETURN_ADDRESS:08x}",
            internal=True,
        )
        self.session = session

    def stop(self):
        if not self.session.active or not self.session.pending_creations:
            return False
        pending = self.session.pending_creations.pop()
        instruction_pointer = int(gdb.parse_and_eval("$eax"))
        instruction = snapshot_reader().instruction(instruction_pointer)
        if instruction["opcode"] != pending["opcode_argument"]:
            raise gdb.GdbError(
                "PCode creation opcode changed between wrapper and builder"
            )
        pending["sequence"] = len(self.session.creation_events)
        pending["builder_return_address"] = (
            f"0x{PCODE_BUILDER_RETURN_ADDRESS:08x}"
        )
        pending["instruction"] = instruction
        self.session.creation_events.append(pending)
        return False


class CaptureSession:
    def __init__(self, output):
        self.output = output
        self.function_index = 0
        self.function_pointer = 0
        self.coloring_attempts = {}
        self.active = False
        self.creation_epoch = "initial_lowering"
        self.creation_events = []
        self.pending_creations = []
        self.codegen_breakpoint = CodeGenBreakpoint(self)
        self.initial_pcode_breakpoint = PCodeStageBreakpoint(
            self,
            INITIAL_PCODE_ADDRESS,
            "initial",
            "backend_optimization",
        )
        self.optimized_pcode_breakpoint = PCodeStageBreakpoint(
            self,
            OPTIMIZED_PCODE_ADDRESS,
            "optimized",
            "post_optimization",
        )
        self.pcode_wrapper_breakpoints = [
            PCodeWrapperBreakpoint(self, address) for address in PCODE_WRAPPERS
        ]
        self.pcode_builder_return_breakpoint = PCodeBuilderReturnBreakpoint(self)
        self.allocate_breakpoint = AllocateBreakpoint(self)
        self.coloring_breakpoint = ColoringBreakpoint(self)

    def begin_function(self, function_pointer):
        self.function_index += 1
        self.function_pointer = function_pointer
        self.coloring_attempts = {}
        self.active = True
        self.creation_epoch = "initial_lowering"
        self.creation_events = []
        self.pending_creations = []

    def write_pcode_stage(self, phase, program_counter):
        reader = snapshot_reader()
        snapshot = reader.snapshot(self.function_pointer, program_counter)
        snapshot["capture_index"] = self.function_index
        snapshot["phase"] = phase
        output = self.output / f"pcode-{self.function_index:04d}-{phase}.json"
        write_snapshot(output, snapshot)

        trace = {
            "format": "mwcc-pcode-creation-trace-v1",
            "compiler": "GC/1.2.5",
            "target_sha256": TARGET_SHA256,
            "capture_index": self.function_index,
            "function_pointer": f"0x{self.function_pointer:08x}",
            "through_phase": phase,
            "events": self.creation_events,
            "pending_event_count": len(self.pending_creations),
        }
        trace_output = self.output / (
            f"pcode-creations-{self.function_index:04d}-{phase}.json"
        )
        write_snapshot(trace_output, trace)
        instruction_count = sum(
            len(block["instructions"]) for block in snapshot["blocks"]
        )
        gdb.write(
            f"Captured {phase} PCode {self.function_index}: "
            f"{instruction_count} live instructions, "
            f"{len(self.creation_events)} creation events\n"
        )

    def coloring_path(self, function_index, reg_class, attempt, phase):
        return self.output / (
            f"coloring-{function_index:04d}-{REGISTER_CLASS_NAMES[reg_class]}-"
            f"{attempt:02d}-{phase}.json"
        )


class MwccAutoCapture(gdb.Command):
    """Capture every allocator pass: mwcc-auto-capture DIRECTORY"""

    def __init__(self):
        super().__init__("mwcc-auto-capture", gdb.COMMAND_DATA)
        self.session = None

    def invoke(self, argument, from_tty):
        del from_tty
        output = Path(argument.strip())
        if not output.name:
            raise gdb.GdbError("usage: mwcc-auto-capture DIRECTORY")
        output.mkdir(parents=True, exist_ok=True)
        self.session = CaptureSession(output)
        gdb.write(f"Capturing MWCC allocator passes in {output}\n")


MwccAllocatorSnapshot()
MwccColoringSnapshot()
MwccAutoCapture()
