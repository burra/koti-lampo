// DecompileToC.java - Ghidra headless post-script
//
// Decompiles every function in the current program and writes the C output to
// the file given as the first script argument. Used to produce a Ghidra C view
// of the Kotilampo ROM banks for comparison against the hand reconstruction in
// disasm/koti_lampo.c.
//
// Java is used instead of Python because Ghidra 12 dropped Jython and only runs
// Python scripts under PyGhidra.
//
// @category Kotilampo

import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.util.task.ConsoleTaskMonitor;

public class DecompileToC extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String outPath = (args.length > 0)
            ? args[0]
            : currentProgram.getName() + ".ghidra.c";

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        ConsoleTaskMonitor monitor = new ConsoleTaskMonitor();

        List<Function> funcs = new ArrayList<>();
        currentProgram.getFunctionManager().getFunctions(true).forEach(funcs::add);
        funcs.sort((a, b) -> a.getEntryPoint().compareTo(b.getEntryPoint()));

        int ok = 0;
        try (PrintWriter fh = new PrintWriter(new FileWriter(outPath))) {
            fh.printf("/* Ghidra decompiler output for %s%n", currentProgram.getName());
            fh.printf(" * Language: %s%n", currentProgram.getLanguageID());
            fh.printf(" * Image base: %s%n", currentProgram.getImageBase());
            fh.printf(" * Functions: %d%n */%n%n", funcs.size());

            for (Function f : funcs) {
                DecompileResults res = decomp.decompileFunction(f, 60, monitor);
                if (res != null && res.decompileCompleted()) {
                    fh.println(res.getDecompiledFunction().getC());
                    ok++;
                } else {
                    fh.printf("/* %s @ %s : decompilation failed: %s */%n%n",
                        f.getName(), f.getEntryPoint(),
                        res != null ? res.getErrorMessage() : "no result");
                }
            }
        }
        println("[DecompileToC] wrote " + ok + "/" + funcs.size()
            + " functions to " + outPath);
    }
}
