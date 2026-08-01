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
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ExportOptimizer extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("expected output path");
        }

        Map<Address, Function> functions = new TreeMap<>();
        StringBuilder references = new StringBuilder();
        DataIterator dataIterator = currentProgram.getListing().getDefinedData(true);
        while (dataIterator.hasNext() && !monitor.isCancelled()) {
            Data data = dataIterator.next();
            Object value = data.getValue();
            if (!(value instanceof String)) {
                continue;
            }
            String text = (String) value;
            if (!text.contains("IRO_") &&
                    !text.equals("IrOptimizer.c") &&
                    !text.equals("COptimizer.c")) {
                continue;
            }

            references.append("## String `")
                .append(text.replace("`", "\\`"))
                .append("` at `")
                .append(data.getAddress())
                .append("`\n\n");
            ReferenceIterator iterator = currentProgram.getReferenceManager()
                .getReferencesTo(data.getAddress());
            boolean found = false;
            while (iterator.hasNext()) {
                found = true;
                Reference reference = iterator.next();
                Address from = reference.getFromAddress();
                Function function = currentProgram.getFunctionManager()
                    .getFunctionContaining(from);
                references.append("- `").append(from).append("`");
                if (function != null) {
                    references.append(" in `")
                        .append(function.getName())
                        .append("` at `")
                        .append(function.getEntryPoint())
                        .append("`");
                    functions.put(function.getEntryPoint(), function);
                }
                references.append("\n");
            }
            if (!found) {
                references.append("- no references found\n");
            }
            references.append("\n");
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        StringBuilder output = new StringBuilder();
        output.append("# ")
            .append(currentProgram.getName())
            .append(" optimizer string references\n\n")
            .append("Generated from the verified executable by `ExportOptimizer.java`.\n\n")
            .append(references)
            .append("# Referencing functions\n\n");
        for (Function function : functions.values()) {
            monitor.checkCancelled();
            output.append("## `")
                .append(function.getName())
                .append("` at `")
                .append(function.getEntryPoint())
                .append("`\n\n");
            DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
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
