import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ExportSubsystems extends GhidraScript {
    private static final String[] MODULES = {
        "IrOptimizer.c",
        "COptimizer.c",
        "CodeGen.c",
        "Registers.c",
        "Coloring.c",
        "SpillCode.c",
        "StackFrameEABI.c",
        "Scheduler.c",
    };

    private static final String[] SIGNALS = {
        "AFTER PEEPHOLE FORWARD",
        "AFTER COPY PROPAGATION",
        "AFTER STRENGTH REDUCTION",
        "AFTER CONSTANT PROPAGATION",
        "AFTER ARRAY => REGISTER TRANSFORM",
        "AFTER REGISTER COLORING",
        "AFTER GENERATING EPILOGUE, PROLOGUE",
        "AFTER MERGING EPILOGUE, PROLOGUE",
        "AFTER PEEPHOLE OPTIMIZATION",
        "AFTER CHECKING FOR ALTIVEC FRAME",
        "fSpilled",
        "stackframe",
        "stack depth",
        "can't color virtual",
        "could not be assigned to a register",
    };

    private static String escape(String text) {
        return text.replace("|", "\\|")
            .replace("\r", "\\r")
            .replace("\n", "\\n")
            .replace("`", "\\`");
    }

    private static boolean equalsAny(String text, String[] values) {
        return Arrays.asList(values).contains(text);
    }

    private static boolean containsAny(String text, String[] values) {
        for (String value : values) {
            if (text.contains(value)) {
                return true;
            }
        }
        return false;
    }

    private Set<Function> referencingFunctions(Data data) {
        Set<Function> functions = new TreeSet<>((left, right) ->
            left.getEntryPoint().compareTo(right.getEntryPoint()));
        ReferenceIterator iterator = currentProgram.getReferenceManager()
            .getReferencesTo(data.getAddress());
        while (iterator.hasNext()) {
            Reference reference = iterator.next();
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            if (function != null) {
                functions.add(function);
            }
        }
        return functions;
    }

    private String functionList(Set<Function> functions) {
        if (functions.isEmpty()) {
            return "none";
        }
        StringBuilder result = new StringBuilder();
        for (Function function : functions) {
            if (result.length() != 0) {
                result.append(", ");
            }
            result.append('`')
                .append(function.getEntryPoint())
                .append("` (`")
                .append(function.getName())
                .append("`)");
        }
        return result.toString();
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("expected output path");
        }

        Map<String, Map<Address, Function>> modules = new LinkedHashMap<>();
        for (String module : MODULES) {
            modules.put(module, new TreeMap<>());
        }
        Map<Address, String> signalText = new TreeMap<>();
        Map<Address, Set<Function>> signalFunctions = new TreeMap<>();

        DataIterator iterator = currentProgram.getListing().getDefinedData(true);
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Data data = iterator.next();
            Object value = data.getValue();
            if (!(value instanceof String)) {
                continue;
            }
            String text = (String) value;
            Set<Function> functions = referencingFunctions(data);
            if (equalsAny(text, MODULES)) {
                Map<Address, Function> entries = modules.get(text);
                for (Function function : functions) {
                    entries.put(function.getEntryPoint(), function);
                }
            }
            if (containsAny(text, SIGNALS)) {
                signalText.put(data.getAddress(), text);
                signalFunctions.put(data.getAddress(), functions);
            }
        }

        StringBuilder output = new StringBuilder();
        output.append("# Core compiler subsystem inventory\n\n")
            .append("Generated from the verified executable by ")
            .append("`ExportSubsystems.java`. Addresses are evidence; names ")
            .append("begin as module-membership hypotheses until control flow ")
            .append("is recovered.\n\n")
            .append("## Embedded source-file anchors\n\n");
        for (Map.Entry<String, Map<Address, Function>> entry : modules.entrySet()) {
            output.append("### `").append(entry.getKey()).append("`\n\n")
                .append("| Entry | Current name | Bytes |\n")
                .append("| --- | --- | ---: |\n");
            for (Function function : entry.getValue().values()) {
                output.append("| `")
                    .append(function.getEntryPoint())
                    .append("` | `")
                    .append(function.getName())
                    .append("` | ")
                    .append(function.getBody().getNumAddresses())
                    .append(" |\n");
            }
            if (entry.getValue().isEmpty()) {
                output.append("| — | no direct references | — |\n");
            }
            output.append('\n');
        }

        output.append("## Diagnostic and pipeline anchors\n\n")
            .append("| String address | Text | Referencing functions |\n")
            .append("| --- | --- | --- |\n");
        for (Map.Entry<Address, String> entry : signalText.entrySet()) {
            output.append("| `")
                .append(entry.getKey())
                .append("` | `")
                .append(escape(entry.getValue()))
                .append("` | ")
                .append(functionList(signalFunctions.get(entry.getKey())))
                .append(" |\n");
        }

        Path outputPath = Path.of(args[0]);
        Files.createDirectories(outputPath.toAbsolutePath().getParent());
        try (BufferedWriter writer = Files.newBufferedWriter(
                outputPath, StandardCharsets.UTF_8)) {
            writer.write(output.toString());
        }
        println("Wrote " + outputPath);
    }
}
