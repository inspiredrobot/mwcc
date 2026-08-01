import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class RankLeafFunctions extends GhidraScript {
    private static class Candidate {
        Function function;
        int instructions;

        Candidate(Function function, int instructions) {
            this.function = function;
            this.instructions = instructions;
        }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 4) {
            throw new IllegalArgumentException(
                "expected output path, start, end, and limit");
        }
        Address start = toAddr(args[1]);
        Address end = toAddr(args[2]);
        int limit = Integer.parseInt(args[3]);
        List<Candidate> candidates = new ArrayList<>();

        FunctionIterator functions = currentProgram.getFunctionManager()
            .getFunctions(start, true);
        while (functions.hasNext() && !monitor.isCancelled()) {
            Function function = functions.next();
            if (function.getEntryPoint().compareTo(end) >= 0) {
                break;
            }
            if (function.isThunk()) {
                continue;
            }
            int count = 0;
            boolean hasCall = false;
            InstructionIterator instructions = currentProgram.getListing()
                .getInstructions(function.getBody(), true);
            while (instructions.hasNext()) {
                Instruction instruction = instructions.next();
                count++;
                if (instruction.getFlowType().isCall()) {
                    hasCall = true;
                    break;
                }
            }
            if (!hasCall && count >= 3 && count <= 30) {
                candidates.add(new Candidate(function, count));
            }
        }
        candidates.sort(
            Comparator.comparingInt((Candidate value) -> value.instructions)
                .thenComparing(value -> value.function.getEntryPoint()));

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        StringBuilder output = new StringBuilder();
        output.append("# Small leaf functions from ")
            .append(currentProgram.getName())
            .append("\n\n");
        int emitted = 0;
        for (Candidate candidate : candidates) {
            if (emitted++ >= limit) {
                break;
            }
            Function function = candidate.function;
            output.append("## `")
                .append(function.getName())
                .append("` at `")
                .append(function.getEntryPoint())
                .append("` (")
                .append(candidate.instructions)
                .append(" instructions)\n\n");
            DecompileResults result = decompiler.decompileFunction(
                function, 60, monitor);
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
