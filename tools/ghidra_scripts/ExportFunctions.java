import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;

public class ExportFunctions extends GhidraScript {
    private static String hex(byte[] bytes) {
        StringBuilder output = new StringBuilder();
        for (byte value : bytes) {
            output.append(String.format("%02x", value & 0xff));
        }
        return output.toString();
    }

    private String functionList(Set<Function> functions) {
        if (functions.isEmpty()) {
            return "none";
        }
        Set<Function> ordered = new TreeSet<>((left, right) ->
            left.getEntryPoint().compareTo(right.getEntryPoint()));
        ordered.addAll(functions);
        StringBuilder output = new StringBuilder();
        for (Function function : ordered) {
            if (output.length() != 0) {
                output.append(", ");
            }
            output.append('`')
                .append(function.getEntryPoint())
                .append("` (`")
                .append(function.getName())
                .append("`)");
        }
        return output.toString();
    }

    private Set<String> referencedStrings(Function function) {
        Set<String> strings = new TreeSet<>();
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(function.getBody(), true);
        while (instructions.hasNext()) {
            Instruction instruction = instructions.next();
            Reference[] references = currentProgram.getReferenceManager()
                .getReferencesFrom(instruction.getAddress());
            for (Reference reference : references) {
                ghidra.program.model.listing.Data data = currentProgram.getListing()
                    .getDataAt(reference.getToAddress());
                if (data != null && data.getValue() instanceof String) {
                    String text = (String) data.getValue();
                    strings.add("`" + data.getAddress() + "` `" +
                        text.replace("`", "\\`")
                            .replace("\r", "\\r")
                            .replace("\n", "\\n") + "`");
                }
            }
        }
        return strings;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException(
                "expected output path and at least one address");
        }

        Map<Address, Function> functions = new TreeMap<>();
        for (int index = 1; index < args.length; index++) {
            Address address = toAddr(args[index]);
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(address);
            if (function == null) {
                // Routines reached only through a function-pointer table are
                // never created by auto-analysis. Define one on demand so the
                // caller can export it, and say so: the boundary comes from
                // this disassembly pass, not from Ghidra's own analysis.
                println("Creating function at " + address
                    + " (not found by analysis)");
                disassemble(address);
                function = createFunction(address, null);
            }
            if (function == null) {
                printerr("No function contains " + address);
            } else {
                functions.put(function.getEntryPoint(), function);
            }
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        StringBuilder output = new StringBuilder();
        output.append("# Selected functions from ")
            .append(currentProgram.getName())
            .append("\n\n");
        for (Function function : functions.values()) {
            monitor.checkCancelled();
            output.append("## `")
                .append(function.getName())
                .append("` at `")
                .append(function.getEntryPoint())
                .append("`\n\n")
                .append("- Bytes: ")
                .append(function.getBody().getNumAddresses())
                .append("\n- Callers: ")
                .append(functionList(function.getCallingFunctions(monitor)))
                .append("\n- Callees: ")
                .append(functionList(function.getCalledFunctions(monitor)))
                .append("\n");
            Set<String> strings = referencedStrings(function);
            if (!strings.isEmpty()) {
                output.append("- Referenced strings:\n");
                for (String text : strings) {
                    output.append("  - ").append(text).append("\n");
                }
            }
            output.append("\n```text\n");
            InstructionIterator iterator = currentProgram.getListing()
                .getInstructions(function.getBody(), true);
            while (iterator.hasNext()) {
                Instruction instruction = iterator.next();
                output.append(instruction.getAddress())
                    .append("  ")
                    .append(hex(instruction.getBytes()))
                    .append("  ")
                    .append(instruction)
                    .append("\n");
            }
            output.append("```\n\n");

            DecompileResults result = decompiler.decompileFunction(
                function, 120, monitor);
            if (result.decompileCompleted()) {
                output.append("```c\n")
                    .append(result.getDecompiledFunction().getC())
                    .append("\n```\n\n");
            } else {
                output.append("Decompiler failed: ")
                    .append(result.getErrorMessage())
                    .append("\n\n");
            }
        }
        decompiler.dispose();

        Path outputPath = Path.of(args[0]);
        Files.createDirectories(outputPath.toAbsolutePath().getParent());
        try (BufferedWriter writer = Files.newBufferedWriter(
                outputPath, StandardCharsets.UTF_8)) {
            writer.write(output.toString());
        }
        println("Wrote " + outputPath);
    }
}
