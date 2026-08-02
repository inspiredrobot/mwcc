import json
import sys
from pathlib import Path

import gdb


TOOLS_DIR = Path(__file__).resolve().parents[1]
REPOSITORY_DIR = TOOLS_DIR.parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from allocator_snapshot import TARGET_NINJI_SHA256, TARGET_SHA256, SnapshotReader


ALLOCATE_REGISTERS_ADDRESS = 0x004CDEF0
SELECT_COLORS_ADDRESS = 0x004CE2D0
CODEGEN_GENERATOR_ADDRESS = 0x004351C0
INITIAL_PCODE_ADDRESS = 0x00435B04
OPTIMIZED_PCODE_ADDRESS = 0x00435B39
POST_SCHEDULER_PCODE_ADDRESS = 0x00435BAF
FORWARD_PEEPHOLE_PCODE_ADDRESS = 0x00435BFD
PCODE_EMIT_ADDRESS = 0x004A25D0
PCODE_CREATE_ADDRESS = 0x004A2620
PCODE_BUILDER_RETURN_ADDRESS = 0x004A2B6D
PCODE_ARENA_ALLOCATOR_RETURN_ADDRESS = 0x00441FD5
PCODE_CLONE_ADDRESS = 0x0049D270
PCODE_CLONE_RETURN_ADDRESS = 0x0049D2EC
CURRENT_CODEGEN_ITEM_ADDRESS = 0x00587130
VIRTUAL_REGISTER_ALLOCATORS = {
    0x004C1F60: ("vr", "single"),
    0x004C2040: ("fpr", "single"),
    0x004C2120: ("gpr", "pair"),
    0x004C2280: ("gpr", "single"),
}

PCODE_WRAPPERS = {
    PCODE_EMIT_ADDRESS: "emit",
    PCODE_CREATE_ADDRESS: "create",
}
REGISTER_CLASS_NAMES = {0: "gpr", 1: "fpr", 9: "vr"}
CAPTURE_TARGETS = {
    "stock": ("GC/1.2.5", TARGET_SHA256),
    "ninji": ("GC/1.2.5n", TARGET_NINJI_SHA256),
}
VIRTUAL_REGISTER_SITE_CATALOGS = {
    "stock": "GC_1_2_5",
    "ninji": "GC_1_2_5n",
}
VIRTUAL_REGISTER_COUNTER_RESET_ADDRESS = 0x004C23C0


def load_virtual_register_sites(target):
    version = VIRTUAL_REGISTER_SITE_CATALOGS[target]
    path = REPOSITORY_DIR / "config" / version / "virtual_register_sites.json"
    with path.open(encoding="utf-8") as stream:
        catalog = json.load(stream)
    if catalog.get("format") != "mwcc-virtual-register-sites-v1":
        raise gdb.GdbError(f"unsupported virtual-register site catalog: {path}")
    if catalog.get("target_sha256") != CAPTURE_TARGETS[target][1]:
        raise gdb.GdbError(f"virtual-register site catalog hash mismatch: {path}")
    return [
        {
            **site,
            "address": int(site["address"], 0),
            "counter_address": int(site["counter_address"], 0),
        }
        for site in catalog["sites"]
    ]


def snapshot_reader(session=None):
    inferior = gdb.selected_inferior()
    compiler, target_sha256 = CAPTURE_TARGETS["stock"]
    if session is not None:
        compiler = session.compiler
        target_sha256 = session.target_sha256
    return SnapshotReader(
        lambda address, size: bytes(inferior.read_memory(address, size)),
        compiler,
        target_sha256,
    )


def write_snapshot(output, snapshot):
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8") as stream:
        json.dump(snapshot, stream, indent=2)
        stream.write("\n")


def optional_raw(reader, address, size):
    if address == 0:
        return None
    try:
        return reader.raw(address, size).hex()
    except gdb.MemoryError:
        return None


def optional_compiler_object(reader, address):
    if address == 0:
        return None
    try:
        type_address = reader.u32(address + 0x0E)
        info_26 = reader.u32(address + 0x26)
        info_2e = reader.u32(address + 0x2E)
        result = {
            "address": f"0x{address:08x}",
            "header": reader.raw(address, 0x32).hex(),
            "object_tag_00": reader.u8(address),
            "kind_02": reader.u8(address + 0x02),
            "type_address": f"0x{type_address:08x}",
            "flags_12": reader.u32(address + 0x12),
            "register_info_26": optional_register_info(reader, info_26),
            "register_info_2e": optional_register_info(reader, info_2e),
            "type": None,
        }
        if type_address != 0:
            result["type"] = {
                "header": reader.raw(type_address, 0x0F).hex(),
                "kind_00": reader.u8(type_address),
                "size_02": reader.u32(type_address + 0x02),
                "flags_0a": reader.u32(type_address + 0x0A),
                "subtype_0e": reader.u8(type_address + 0x0E),
            }
        return result
    except gdb.MemoryError:
        return None


def optional_register_info(reader, address):
    if address == 0:
        return None
    try:
        return {
            "address": f"0x{address:08x}",
            "header": reader.raw(address, 0x2C).hex(),
            "physical_register_24": reader.s16(address + 0x24),
            "secondary_register_26": reader.s16(address + 0x26),
            "is_fpr_28": reader.u8(address + 0x28),
            "is_vector_2a": reader.u8(address + 0x2A),
        }
    except gdb.MemoryError:
        return None


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
            internal=True,
        )
        self.session = session
        self.function_index = session.function_index
        self.attempt = attempt
        self.reg_class = reg_class

    def stop(self):
        if not self.session.capture_current:
            return False
        reader = snapshot_reader(self.session)
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
            internal=True,
        )
        self.session = session

    def stop(self):
        if not self.session.capture_current:
            return False
        reader = snapshot_reader(self.session)
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
            internal=True,
        )
        self.session = session

    def stop(self):
        reader = snapshot_reader(self.session)
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        function_pointer = reader.u32(stack_pointer + 4)
        if not self.session.active:
            self.session.begin_function(function_pointer)
        if not self.session.capture_current:
            return False
        self.session.coloring_attempts = {}
        snapshot = reader.snapshot(
            function_pointer, int(gdb.parse_and_eval("$pc"))
        )
        snapshot["capture_index"] = self.session.function_index
        output = self.session.output / (
            f"allocator-{self.session.function_index:04d}.json"
        )
        write_snapshot(output, snapshot)
        self.session.write_creation_trace("allocator", snapshot)
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
        reader = snapshot_reader(self.session)
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
        if not self.session.capture_current:
            return False
        if self.phase == "optimized":
            self.session.pcode_allocation_breakpoint.enabled = False
            self.session.pcode_clone_breakpoint.enabled = False
            self.session.pcode_clone_return_breakpoint.enabled = False
        self.session.write_pcode_stage(
            self.phase,
            int(gdb.parse_and_eval("$pc")),
        )
        self.session.creation_epoch = self.next_epoch
        if self.phase == "initial":
            self.session.pcode_allocation_breakpoint.enabled = True
            self.session.pcode_clone_breakpoint.enabled = True
            self.session.pcode_clone_return_breakpoint.enabled = True
        return False


class PCodeAllocationReturnBreakpoint(gdb.Breakpoint):
    """Record arena allocations made while the backend optimizer runs."""

    def __init__(self, session):
        super().__init__(
            f"*0x{PCODE_ARENA_ALLOCATOR_RETURN_ADDRESS:08x}",
            internal=True,
        )
        self.session = session
        self.enabled = False

    def stop(self):
        if (
            not self.session.capture_current
            or self.session.creation_epoch != "backend_optimization"
        ):
            return False
        reader = snapshot_reader(self.session)
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        return_address = reader.u32(stack_pointer)
        call_address = return_address - 5
        if reader.u8(call_address) != 0xE8:
            call_address = None
        self.session.optimizer_allocations.append(
            {
                "sequence": len(self.session.optimizer_allocations),
                "epoch": self.session.creation_epoch,
                "address": f"0x{int(gdb.parse_and_eval('$eax')):08x}",
                "requested_size": reader.u32(stack_pointer + 4),
                "caller_return_address": f"0x{return_address:08x}",
                "call_address": (
                    f"0x{call_address:08x}" if call_address is not None else None
                ),
            }
        )
        return False


class PCodeCloneBreakpoint(gdb.Breakpoint):
    """Capture the parent and caller when the optimizer clones PCode."""

    def __init__(self, session):
        super().__init__(f"*0x{PCODE_CLONE_ADDRESS:08x}", internal=True)
        self.session = session
        self.enabled = False

    def stop(self):
        if (
            not self.session.capture_current
            or self.session.creation_epoch != "backend_optimization"
        ):
            return False
        reader = snapshot_reader(self.session)
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        return_address = reader.u32(stack_pointer)
        call_address = return_address - 5
        if reader.u8(call_address) != 0xE8:
            call_address = None
        self.session.pending_clones.append(
            {
                "epoch": self.session.creation_epoch,
                "source_address": f"0x{reader.u32(stack_pointer + 4):08x}",
                "caller_return_address": f"0x{return_address:08x}",
                "call_address": (
                    f"0x{call_address:08x}" if call_address is not None else None
                ),
            }
        )
        return False


class PCodeCloneReturnBreakpoint(gdb.Breakpoint):
    def __init__(self, session):
        super().__init__(f"*0x{PCODE_CLONE_RETURN_ADDRESS:08x}", internal=True)
        self.session = session
        self.enabled = False

    def stop(self):
        if not self.session.capture_current or not self.session.pending_clones:
            return False
        reader = snapshot_reader(self.session)
        clone = self.session.pending_clones.pop()
        destination = int(gdb.parse_and_eval("$eax"))
        source = int(clone["source_address"], 0)
        clone["sequence"] = len(self.session.clone_events)
        clone["destination_address"] = f"0x{destination:08x}"
        clone["source_instruction"] = reader.instruction(source)
        clone["destination_instruction"] = reader.instruction(destination)
        if (
            clone["source_instruction"]["opcode"]
            != clone["destination_instruction"]["opcode"]
        ):
            raise gdb.GdbError("PCode clone changed opcode during copy")
        self.session.clone_events.append(clone)
        return False


class VirtualRegisterReturnBreakpoint(gdb.Breakpoint):
    def __init__(self, session, event, return_address):
        super().__init__(f"*0x{return_address:08x}", internal=True)
        self.session = session
        self.event = event

    def stop(self):
        if not self.session.capture_current:
            self.enabled = False
            return False
        reader = snapshot_reader(self.session)
        self.event["sequence"] = len(self.session.virtual_register_events)
        self.event["object_after"] = optional_compiler_object(
            reader, int(self.event["object_address"], 0)
        )
        self.session.virtual_register_events.append(self.event)
        self.enabled = False
        return False


class VirtualRegisterAllocatorBreakpoint(gdb.Breakpoint):
    def __init__(self, session, address):
        super().__init__(f"*0x{address:08x}", internal=True)
        self.session = session
        self.address = address

    def stop(self):
        if not self.session.capture_current:
            return False
        reader = snapshot_reader(self.session)
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        return_address = reader.u32(stack_pointer)
        call_address = return_address - 5
        if reader.u8(call_address) != 0xE8:
            call_address = None
        object_address = reader.u32(stack_pointer + 4)
        reg_class, allocation_kind = VIRTUAL_REGISTER_ALLOCATORS[self.address]
        event = {
            "epoch": self.session.creation_epoch,
            "allocator_address": f"0x{self.address:08x}",
            "register_class": reg_class,
            "allocation_kind": allocation_kind,
            "caller_return_address": f"0x{return_address:08x}",
            "call_address": (
                f"0x{call_address:08x}" if call_address is not None else None
            ),
            "object_address": f"0x{object_address:08x}",
            "object_before": optional_compiler_object(reader, object_address),
            "codegen_item_address": (
                f"0x{reader.u32(CURRENT_CODEGEN_ITEM_ADDRESS):08x}"
            ),
        }
        VirtualRegisterReturnBreakpoint(self.session, event, return_address)
        return False


class DirectVirtualRegisterBreakpoint(gdb.Breakpoint):
    """Capture a verified direct virtual-register counter increment."""

    def __init__(self, session, site):
        super().__init__(f"*0x{site['address']:08x}", internal=True)
        self.session = session
        self.site = site

    def stop(self):
        if self.site["allocation_kind"] == "object_allocator_internal":
            return False
        reader = snapshot_reader(self.session)
        event = {
            "epoch": self.session.creation_epoch,
            "allocator_address": f"0x{self.site['address']:08x}",
            "allocator_write_return_address": None,
            "allocator_address_is_post_write": False,
            "register_class": self.site["register_class"],
            "allocation_kind": "temporary",
            "allocator_function": self.site.get("function"),
            "allocator_operation": self.site.get("operation"),
            "caller_return_address": None,
            "call_address": None,
            "object_address": "0x00000000",
            "object_before": None,
            "object_after": None,
            "codegen_item_address": (
                f"0x{reader.u32(CURRENT_CODEGEN_ITEM_ADDRESS):08x}"
            ),
            "primary_register": reader.s16(self.site["counter_address"]),
            "secondary_register": None,
        }
        if not self.session.capture_current:
            self.session.pending_frontend_virtual_register_events.append(event)
        else:
            event["sequence"] = len(self.session.virtual_register_events)
            self.session.virtual_register_events.append(event)
        return False


class VirtualRegisterCounterResetBreakpoint(gdb.Breakpoint):
    """Delimit pre-CodeGen allocation events at the register-counter reset."""

    def __init__(self, session):
        super().__init__(
            f"*0x{VIRTUAL_REGISTER_COUNTER_RESET_ADDRESS:08x}", internal=True
        )
        self.session = session

    def stop(self):
        self.session.pending_frontend_virtual_register_events = []
        if self.session.capture_current:
            self.session.virtual_register_events = []
        return False


class PCodeWrapperBreakpoint(gdb.Breakpoint):
    def __init__(self, session, address):
        super().__init__(f"*0x{address:08x}", internal=True)
        self.session = session
        self.address = address

    def stop(self):
        if not self.session.capture_current:
            return False
        reader = snapshot_reader(self.session)
        stack_pointer = int(gdb.parse_and_eval("$esp"))
        return_address = reader.u32(stack_pointer)
        call_address = return_address - 5
        if reader.u8(call_address) != 0xE8:
            call_address = None
        codegen_item = reader.u32(CURRENT_CODEGEN_ITEM_ADDRESS)
        item_fields = None
        pointer_0a_data = None
        pointer_0e_data = None
        if codegen_item:
            pointer_0a = reader.u32(codegen_item + 0x0A)
            pointer_0e = reader.u32(codegen_item + 0x0E)
            item_fields = {
                "kind_04": reader.u8(codegen_item + 0x04),
                "byte_05": reader.u8(codegen_item + 0x05),
                "flags_06": reader.u8(codegen_item + 0x06),
                "byte_07": reader.u8(codegen_item + 0x07),
                "signed_08": reader.s16(codegen_item + 0x08),
                "pointer_0a": f"0x{pointer_0a:08x}",
                "pointer_0e": f"0x{pointer_0e:08x}",
            }
            pointer_0a_data = optional_raw(reader, pointer_0a, 0x20)
            pointer_0e_data = optional_raw(reader, pointer_0e, 0x20)
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
                "codegen_item_fields": item_fields,
                "codegen_pointer_0a_data": pointer_0a_data,
                "codegen_pointer_0e_data": pointer_0e_data,
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
        if not self.session.capture_current or not self.session.pending_creations:
            return False
        pending = self.session.pending_creations.pop()
        instruction_pointer = int(gdb.parse_and_eval("$eax"))
        reader = snapshot_reader(self.session)
        instruction = reader.instruction(instruction_pointer)
        for operand in instruction["operands"]:
            object_address = int(operand["object"], 0)
            operand["compiler_object"] = optional_compiler_object(
                reader, object_address
            )
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
    def __init__(self, output, target_index=None, target="stock"):
        self.output = output
        self.target_index = target_index
        self.compiler, self.target_sha256 = CAPTURE_TARGETS[target]
        self.function_index = 0
        self.function_pointer = 0
        self.coloring_attempts = {}
        self.active = False
        self.capture_current = False
        self.creation_epoch = "initial_lowering"
        self.creation_events = []
        self.pending_creations = []
        self.optimizer_allocations = []
        self.clone_events = []
        self.pending_clones = []
        self.virtual_register_events = []
        self.pending_frontend_virtual_register_events = []
        self.pcode_allocation_breakpoint = PCodeAllocationReturnBreakpoint(self)
        self.pcode_clone_breakpoint = PCodeCloneBreakpoint(self)
        self.pcode_clone_return_breakpoint = PCodeCloneReturnBreakpoint(self)
        self.virtual_register_breakpoints = [
            VirtualRegisterAllocatorBreakpoint(self, address)
            for address in VIRTUAL_REGISTER_ALLOCATORS
        ]
        self.direct_virtual_register_breakpoints = [
            DirectVirtualRegisterBreakpoint(self, site)
            for site in load_virtual_register_sites(target)
            if site["allocation_kind"] == "temporary"
        ]
        self.virtual_register_counter_reset_breakpoint = (
            VirtualRegisterCounterResetBreakpoint(self)
        )
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
        self.post_scheduler_pcode_breakpoint = PCodeStageBreakpoint(
            self,
            POST_SCHEDULER_PCODE_ADDRESS,
            "scheduled",
            "forward_peephole",
        )
        self.forward_peephole_pcode_breakpoint = PCodeStageBreakpoint(
            self,
            FORWARD_PEEPHOLE_PCODE_ADDRESS,
            "forward_peephole",
            "register_allocation",
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
        self.capture_current = (
            self.target_index is None or self.function_index == self.target_index
        )
        self.creation_epoch = "initial_lowering"
        self.creation_events = []
        self.pending_creations = []
        self.optimizer_allocations = []
        self.clone_events = []
        self.pending_clones = []
        self.virtual_register_events = (
            self.pending_frontend_virtual_register_events
            if self.capture_current
            else []
        )
        self.pending_frontend_virtual_register_events = []
        for sequence, event in enumerate(self.virtual_register_events):
            event["sequence"] = sequence
        self.pcode_allocation_breakpoint.enabled = False
        self.pcode_clone_breakpoint.enabled = False
        self.pcode_clone_return_breakpoint.enabled = False

    def write_pcode_stage(self, phase, program_counter):
        reader = snapshot_reader(self)
        snapshot = reader.snapshot(self.function_pointer, program_counter)
        snapshot["capture_index"] = self.function_index
        snapshot["phase"] = phase
        output = self.output / f"pcode-{self.function_index:04d}-{phase}.json"
        write_snapshot(output, snapshot)

        self.write_creation_trace(phase, snapshot)

        instruction_count = sum(
            len(block["instructions"]) for block in snapshot["blocks"]
        )
        gdb.write(
            f"Captured {phase} PCode {self.function_index}: "
            f"{instruction_count} live instructions, "
            f"{len(self.creation_events)} creation events\n"
        )

    def write_creation_trace(self, phase, snapshot=None):
        live_instructions = {}
        if snapshot is not None:
            live_instructions = {
                instruction["address"]: instruction
                for block in snapshot["blocks"]
                for instruction in block["instructions"]
            }
        wrapped_addresses = {
            event["instruction"]["address"] for event in self.creation_events
        }
        clone_by_destination = {
            clone["destination_address"]: clone for clone in self.clone_events
        }
        unwrapped_allocations = []
        for allocation in self.optimizer_allocations:
            instruction = live_instructions.get(allocation["address"])
            if instruction is None or allocation["address"] in wrapped_addresses:
                continue
            unwrapped_allocations.append(
                {
                    **allocation,
                    "first_observed_phase": phase,
                    "clone_sequence": (
                        clone_by_destination[allocation["address"]]["sequence"]
                        if allocation["address"] in clone_by_destination
                        else None
                    ),
                    "instruction": instruction,
                }
            )
        trace = {
            "format": "mwcc-pcode-creation-trace-v1",
            "compiler": self.compiler,
            "target_sha256": self.target_sha256,
            "capture_index": self.function_index,
            "function_pointer": f"0x{self.function_pointer:08x}",
            "through_phase": phase,
            "events": self.creation_events,
            "clone_events": self.clone_events,
            "virtual_register_events": self.virtual_register_events,
            "unwrapped_instruction_allocations": unwrapped_allocations,
            "optimizer_allocation_count": len(self.optimizer_allocations),
            "pending_event_count": len(self.pending_creations),
            "pending_clone_count": len(self.pending_clones),
        }
        trace_output = self.output / (
            f"pcode-creations-{self.function_index:04d}-{phase}.json"
        )
        write_snapshot(trace_output, trace)

    def coloring_path(self, function_index, reg_class, attempt, phase):
        return self.output / (
            f"coloring-{function_index:04d}-{REGISTER_CLASS_NAMES[reg_class]}-"
            f"{attempt:02d}-{phase}.json"
        )


class MwccAutoCapture(gdb.Command):
    """Capture MWCC passes: mwcc-auto-capture DIR [INDEX] [stock|ninji]"""

    def __init__(self):
        super().__init__("mwcc-auto-capture", gdb.COMMAND_DATA)
        self.session = None

    def invoke(self, argument, from_tty):
        del from_tty
        arguments = gdb.string_to_argv(argument)
        if len(arguments) not in (1, 2, 3):
            raise gdb.GdbError(
                "usage: mwcc-auto-capture DIRECTORY [FUNCTION_INDEX] "
                "[stock|ninji]"
            )
        output = Path(arguments[0])
        target_index = int(arguments[1], 0) if len(arguments) == 2 else None
        if len(arguments) == 3:
            target_index = int(arguments[1], 0)
        target = arguments[2] if len(arguments) == 3 else "stock"
        if target not in CAPTURE_TARGETS:
            raise gdb.GdbError("target must be stock or ninji")
        if target_index is not None and target_index <= 0:
            raise gdb.GdbError("FUNCTION_INDEX must be positive")
        output.mkdir(parents=True, exist_ok=True)
        self.session = CaptureSession(output, target_index, target)
        selection = (
            f"function {target_index}" if target_index is not None else "all functions"
        )
        gdb.write(
            f"Capturing {target} MWCC passes for {selection} in {output}\n"
        )


MwccAllocatorSnapshot()
MwccColoringSnapshot()
MwccAutoCapture()
