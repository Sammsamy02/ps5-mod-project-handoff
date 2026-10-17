# -*- coding: utf-8 -*-
# Read-only inspection of the currently-open program (worker-rea-trial).
fm = currentProgram.getFunctionManager()
print("PROGRAM_NAME=%s" % currentProgram.getName())
try:
    print("EXEC_SHA256=%s" % currentProgram.getExecutableSHA256())
except:
    print("EXEC_SHA256=unavailable")
print("EXEC_MD5=%s" % currentProgram.getExecutableMD5())
print("FUNC_COUNT=%d" % fm.getFunctionCount())
print("IMAGE_BASE=%s" % currentProgram.getImageBase())
af = currentProgram.getAddressFactory().getDefaultAddressSpace()
for a in ["0x0023d590", "0x0023a5d0", "0x00246090", "0x09365ec0"]:
    addr = af.getAddress(a)
    f = fm.getFunctionContaining(addr)
    if f is None:
        ins = currentProgram.getListing().getInstructionAt(addr)
        print("ADDR %s -> no function; instruction=%s" % (a, ins))
    else:
        print("ADDR %s -> func %s @ %s (body %s)" % (a, f.getName(), f.getEntryPoint(), f.getBody()))
