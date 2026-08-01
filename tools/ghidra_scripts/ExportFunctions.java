import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;
import java.util.TreeMap;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class ExportFunctions extends GhidraScript {
    private static String hex(byte[] bytes) {
        StringBuilder output = new StringBuilder();
        for (byte value : bytes) {
            output.append(String.format("%02x", value & 0xff));
        }
        return output.toString();
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
                .append("`\n\n```text\n");
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
