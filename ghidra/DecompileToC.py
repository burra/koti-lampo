# DecompileToC.py - Ghidra headless post-script
#
# Decompiles every function in the current program and writes the C output to
# the file given as the first script argument. Used to produce a Ghidra C view
# of the Kotilampo ROM banks for comparison against the hand reconstruction in
# disasm/koti_lampo.c.
#
# @category Kotilampo
#
# Run via analyzeHeadless ... -postScript DecompileToC.py <output.c>

import os
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

args = getScriptArgs()
if not args:
    out_path = os.path.join(os.getcwd(), currentProgram.getName() + ".ghidra.c")
else:
    out_path = args[0]

monitor = ConsoleTaskMonitor()
decomp = DecompInterface()
decomp.openProgram(currentProgram)

fm = currentProgram.getFunctionManager()
funcs = list(fm.getFunctions(True))
funcs.sort(key=lambda f: f.getEntryPoint().getOffset())

with open(out_path, "w") as fh:
    fh.write("/* Ghidra decompiler output for %s\n" % currentProgram.getName())
    fh.write(" * Language: %s\n" % currentProgram.getLanguageID())
    fh.write(" * Image base: %s\n" % currentProgram.getImageBase())
    fh.write(" * Functions: %d\n */\n\n" % len(funcs))
    for f in funcs:
        res = decomp.decompileFunction(f, 60, monitor)
        if res is not None and res.decompileCompleted():
            fh.write(res.getDecompiledFunction().getC())
            fh.write("\n")
        else:
            fh.write("/* %s @ %s : decompilation failed: %s */\n\n" % (
                f.getName(), f.getEntryPoint(),
                res.getErrorMessage() if res else "no result"))

print("[DecompileToC] wrote %d functions to %s" % (len(funcs), out_path))
