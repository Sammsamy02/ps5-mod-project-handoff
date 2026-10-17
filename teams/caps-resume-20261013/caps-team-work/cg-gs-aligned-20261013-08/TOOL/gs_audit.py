"""Read-only x64 PE executable-section sweep for direct GS memory operands.

This is a candidate detector, not a reachability proof or a proof of absence.
Linear disassembly can interpret embedded data as instructions and miss code
entered at a different offset. The loader must not treat a clean sweep as safe.
"""

from typing import Any, Dict


def audit_gs_access(image: Any) -> Dict[str, Any]:
    if image.coff.get("machine") != 0x8664 or image.optional.get("format") != "PE32+":
        return {"status": "not_applicable", "candidate_count": 0, "sites": []}
    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_64
        from capstone.x86_const import X86_OP_MEM, X86_REG_GS
    except ImportError:
        return {"status": "unavailable", "reason": "Python Capstone is required for the GS audit",
                "candidate_count": 0, "sites": []}

    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    decoder.skipdata = True
    counts: Dict[str, int] = {}
    sites = []
    scanned_bytes = 0
    try:
        for section in image.sections:
            if not section["executable"]:
                continue
            raw = section["raw_size"]
            off = section["raw_ptr"]
            if off < 0 or raw < 0 or off + raw > len(image.data):
                return {"status": "unavailable", "reason": "executable section is not file-backed",
                        "candidate_count": 0, "sites": []}
            scanned_bytes += raw
            for insn in decoder.disasm(image.data[off:off + raw], section["virtual_address"]):
                if insn.id == 0 or insn.mnemonic == "lea":
                    continue
                for operand in insn.operands:
                    if operand.type != X86_OP_MEM or operand.mem.segment != X86_REG_GS:
                        continue
                    disp = operand.mem.disp
                    key = "0x%X" % disp if disp >= 0 else "-0x%X" % -disp
                    counts[key] = counts.get(key, 0) + 1
                    if len(sites) < 32:
                        sites.append({"rva": insn.address, "displacement": disp,
                                      "bytes": insn.bytes.hex(),
                                      "instruction": "%s %s" % (insn.mnemonic, insn.op_str)})
    except Exception as error:
        return {"status": "unavailable", "reason": "instruction decoder failed: %s" % error,
                "candidate_count": 0, "sites": []}
    return {"status": "scanned", "candidate_count": sum(counts.values()),
            "candidate_count_by_displacement": counts, "sites": sites,
            "executable_file_bytes_scanned": scanned_bytes,
            "limitation": "linear sweep: candidates may be data; zero candidates does not prove absence"}
