//Dumps decompiler high-variable info (locals + params) for the function at the cursor.
//Shows live ranges computed from SSA def-use chains so you can identify which register
//holds a variable at a given RIP: find the instance whose range contains RIP.
//Run from Script Manager with the cursor inside any function.
//Output appears in the Script Console.
//@author smc64
//@category Reversing
//@keybinding
//@menupath
//@toolbar

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.*;

import java.io.File;
import java.io.PrintWriter;
import java.util.Iterator;

public class DumpLocals extends GhidraScript {

    private long funcEntryOffset;

    @Override
    public void run() throws Exception {
        Function func = getFunctionContaining(currentAddress);
        if (func == null) {
            println("No function at current address: " + currentAddress);
            return;
        }

        funcEntryOffset = func.getEntryPoint().getOffset();

        println("");
        println("=== " + func.getName() + " @ " + func.getEntryPoint() + " ===");
        println("    Stack frame size : " + func.getStackFrame().getFrameSize() + " bytes");
        println("    Param offset     : " + func.getStackFrame().getParameterOffset());
        println("");

        DecompInterface decomp = new DecompInterface();
        decomp.setOptions(new DecompileOptions());
        decomp.openProgram(currentProgram);

        try {
            DecompileResults res = decomp.decompileFunction(func, 60, monitor);
            if (!res.decompileCompleted()) {
                println("Decompilation failed: " + res.getErrorMessage());
                return;
            }

            HighFunction highFunc = res.getHighFunction();
            if (highFunc == null) {
                println("No HighFunction produced.");
                return;
            }

            LocalSymbolMap symMap = highFunc.getLocalSymbolMap();

            println(String.format("  %-4s  %-30s %-25s %-6s  %-22s  %s",
                    "Kind", "Name", "Type", "Bytes", "Storage", "Live Range [def → lastUse]"));
            println("  " + "-".repeat(105));

            Iterator<HighSymbol> syms = symMap.getSymbols();
            while (syms.hasNext()) {
                HighSymbol sym = syms.next();
                HighVariable var = sym.getHighVariable();
                if (var == null) continue;

                String kind = (var instanceof HighParam) ? "PARM" : "LOCL";
                String name = var.getName();
                String typeName = var.getDataType().getDisplayName();
                int size = var.getSize();

                Varnode rep = var.getRepresentative();
                String repStorage = describeVarnode(rep);
                String repRange = liveRangeStr(rep);

                println(String.format("  %-4s  %-30s %-25s %-6d  %-22s  %s",
                        kind, name, typeName, size, repStorage, repRange));

                // Show all SSA instances of this variable (e.g. spills, copies into regs)
                Varnode[] instances = var.getInstances();
                if (instances.length > 1) {
                    for (Varnode inst : instances) {
                        if (inst == rep) continue;
                        String instStorage = describeVarnode(inst);
                        String instRange = liveRangeStr(inst);
                        println(String.format("  %-4s  %-30s %-25s %-6s  %-22s  %s  [also]",
                                "", "", "", "", instStorage, instRange));
                    }
                }
            }

            println("");
            println("--- Notes ---");
            println("  To find the register at a given RIP: pick the instance whose live range contains RIP.");
            println("  Live ranges are def-use approximations; control flow branches are not modeled.");
            println("  stack[+N] = above frame base (return addr / caller params)");
            println("  stack[-N] = below frame base (local vars / saved regs)");
            println("  Stack addr at runtime: RSP + <prologue_delta> + offset");

            // --- Lua output ---
            writeLua(func, symMap);

        } finally {
            decomp.dispose();
        }
    }

    private void writeLua(Function func, LocalSymbolMap symMap) throws Exception {
        File outDir;
        try {
            outDir = getSourceFile().getFile(false).getParentFile();
        } catch (Exception e) {
            outDir = new File(System.getProperty("java.io.tmpdir"));
        }

        File luaFile = new File(outDir, "instances.lua");
        try (PrintWriter lua = new PrintWriter(luaFile)) {
            lua.println("-- Auto-generated by DumpLocals.java");
            lua.println("-- " + func.getName() + " @ 0x" + Long.toHexString(funcEntryOffset));
            lua.println("-- frameSize = " + func.getStackFrame().getFrameSize());
            lua.println("return {");

            Iterator<HighSymbol> syms = symMap.getSymbols();
            while (syms.hasNext()) {
                HighSymbol sym = syms.next();
                HighVariable var = sym.getHighVariable();
                if (var == null) continue;

                String kind = (var instanceof HighParam) ? "PARM" : "LOCL";
                String name = var.getName();
                String typeName = var.getDataType().getDisplayName();

                for (Varnode inst : var.getInstances()) {
                    if (!inst.isRegister()) continue;
                    Register reg = currentProgram.getLanguage()
                            .getRegister(inst.getAddress(), inst.getSize());
                    if (reg == null) continue;
                    long def = getDefPC(inst);
                    long end = liveRangeEnd(inst);
                    if (end == 0) continue; // dead / unused

                    // Normalise to the largest enclosing register name (e.g. EAX -> RAX)
                    // so CE register names match without extra aliasing logic.
                    Register parent = reg.getParentRegister();
                    String regName = (parent != null) ? parent.getName() : reg.getName();

                    lua.printf("  { reg=%-6s def=0x%x, lastUse=0x%x, name=%s, type=%s, kind=%s },\n",
                            luaStr(regName) + ",",
                            def, end,
                            luaStr(name),
                            luaStr(typeName),
                            luaStr(kind));
                }
            }

            lua.println("}");
        }

        println("");
        println("Lua written to: " + luaFile.getAbsolutePath());
    }

    private static String luaStr(String s) {
        return "\"" + s.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
    }

    private long getDefPC(Varnode vn) {
        Address pc = vn.getPCAddress();
        if (pc == null || pc.equals(Address.NO_ADDRESS)) return funcEntryOffset;
        return pc.getOffset();
    }

    private long liveRangeEnd(Varnode vn) {
        long end = 0;
        Iterator<PcodeOp> uses = vn.getDescendants();
        while (uses.hasNext()) {
            long addr = uses.next().getSeqnum().getTarget().getOffset();
            if (addr > end) end = addr;
        }
        return end;
    }

    private String liveRangeStr(Varnode vn) {
        if (vn == null || vn.isConstant() || vn.isUnique()) return "";
        long def = getDefPC(vn);
        long end = liveRangeEnd(vn);
        String defStr = (def == funcEntryOffset) ? "entry" : Long.toHexString(def);
        String endStr = (end == 0) ? "(dead)" : Long.toHexString(end);
        return "[" + defStr + " \u2192 " + endStr + "]";
    }

    private String describeVarnode(Varnode vn) {
        if (vn == null) return "(null)";

        if (vn.isConstant()) {
            return "const=0x" + Long.toHexString(vn.getOffset());
        }

        if (vn.isUnique()) {
            return "unique(temp/SSA)";
        }

        if (vn.isAddress()) {
            long offset = vn.getOffset();
            String spaceName = vn.getAddress().getAddressSpace().getName();
            if ("stack".equalsIgnoreCase(spaceName)) {
                // offset is signed; cast to int for sign extension on 32-bit offsets
                long signed = (int) offset;
                return String.format("stack[%+d]", signed);
            }
            // Global / RAM address
            return spaceName + "[0x" + Long.toHexString(offset) + "]";
        }

        if (vn.isRegister()) {
            Register reg = currentProgram.getLanguage()
                    .getRegister(vn.getAddress(), vn.getSize());
            if (reg != null) return reg.getName();
            return "reg@0x" + Long.toHexString(vn.getOffset()) + ":" + vn.getSize();
        }

        return "other(" + vn.getAddress().getAddressSpace().getName()
                + "[0x" + Long.toHexString(vn.getOffset()) + "])";
    }
}
