#!/usr/bin/env python3
"""inspect_dll_census.py - worker-dll-mod-retention (PS5 mod-manager, ZCode team)

Given downloaded archives (.zip, in-memory, nothing extracted to disk) and/or
raw PE files, extract every PE DLL and emit a full import census JSON:

  - every imported symbol: library, name (or ordinal), family classification
  - family taxonomy (project runtime track):
      crt / heap / tls / loader / clock / workers / critical / address-wait /
      srw / init_once / mutex / fileio / uncategorized
  - static-TLS detection (TLS data directory: AddressOfIndex, callbacks, raw data)
  - CRT-startup flags (_initterm/_onexec/_onexit/etc.; which CRT runtime DLLs
    are imported; static-CRT likelihood heuristic)
  - export census (proxy-DLL detection: e.g. dwmapi.dll forwarding shims)

Pure stdlib. No network. Never writes outside -o target. Run:

  python3 inspect_dll_census.py <archive.zip|file.dll|dir> [...] -o census.json
"""
import argparse, datetime, hashlib, io, json, os, struct, sys, zipfile

TOOL_VERSION = "1.0.0"

MACHINES = {0x014C: "x86", 0x8664: "x64", 0xAA64: "arm64", 0x01C0: "arm"}
SUBSYSTEMS = {2: "WINDOWS_CUI", 3: "WINDOWS_GUI", 1: "NATIVE", 9: "WINDOWS_CE_GUI",
              10: "EFI_APPLICATION", 13: "XBOX_CODE_CATALOG"}

# ---------------------------------------------------------------- families ---
# Ordered: first match wins. Match on lowercase import name (prefix or exact
# where noted). Subtag keeps finer granularity inside a family bucket.
FAMILY_RULES = [
    ("loader", [
        ("loadlibrary", None), ("getprocaddress", None), ("getmodulehandle", None),
        ("freelibrary", None), ("freelibraryandexitthread", None),
        ("ldrloaddll", None), ("ldrgetprocedureaddress", None),
        ("ldrregisterdllnotification", None), ("disablethreadlibrarycalls", None),
        ("getmodulefilename", None), ("loadresource", None), ("findresource", None),
        ("lockresource", None), ("sizeofresource", None), ("loadmodule", None),
        ("getmodulehandleex", None),
    ]),
    ("tls", [
        ("tlsalloc", None), ("tlsfree", None), ("tlsgetvalue", None),
        ("tlssetvalue", None), ("flsalloc", None), ("flsfree", None),
        ("flsgetvalue", None), ("flssetvalue", None),
        ("__tls_get_address", "ia32_tls_array"), ("__tls_array", "ia32_tls_array"),
    ]),
    ("init_once", [
        ("initoncebegininitialize", None), ("initoncecomplete", None),
        ("initonceexecuteonce", None),
    ]),
    ("srw", [
        ("acquiresrwlock", None), ("releasesrwlock", None),
        ("tryacquiresrwlock", None), ("initializesrwlock", None),
        ("sleepconditionvariable", "condition_variable"),
        ("wakeconditionvariable", "condition_variable"),
        ("wakeallconditionvariable", "condition_variable"),
    ]),
    ("mutex", [
        ("createmutex", None), ("openmutex", None), ("releasemutex", None),
        ("waitformutexobject", None),
    ]),
    ("critical", [
        ("initializecriticalsection", None), ("entercriticalsection", None),
        ("leavecriticalsection", None), ("tryentercriticalsection", None),
        ("deletecriticalsection", None), ("setcriticalsectionspincount", None),
    ]),
    ("address-wait", [
        ("waitonaddress", "wait_on_address"), ("wakebyaddress", "wait_on_address"),
        ("waitforsingleobject", "legacy_object_wait"),
        ("waitformultipleobjects", "legacy_object_wait"),
        ("waitformultipleobjectsex", "legacy_object_wait"),
        ("signalobjectandwait", "legacy_object_wait"),
        ("msgwaitformultipleobjects", "legacy_object_wait"),
        ("sleepex", "alertable_wait"),
    ]),
    ("workers", [
        ("createthread", None), ("beginthread", "crt_threadwrap"),
        ("beginthreadex", "crt_threadwrap"), ("endthreadex", "crt_threadwrap"),
        ("endthread", "crt_threadwrap"), ("createremotethread", None),
        ("exitthread", None), ("terminatethread", None), ("resumethread", None),
        ("suspendthread", None), ("switchtothread", None),
        ("queueuserworkitem", "threadpool_legacy"),
        ("queueuserapc", None), ("createshreadpool", None), ("createthreadpool", "threadpool_vista"),
        ("trysubmitthreadpoolcallback", "threadpool_vista"),
        ("setthreadpoolthreadminimum", "threadpool_vista"),
        ("setthreadpoolthreadmaximum", "threadpool_vista"),
        ("closethreadpool", "threadpool_vista"),
        ("registerwaitforsingleobject", "threadpool_legacy"),
        ("unregisterwait", "threadpool_legacy"),
        ("setthreadpriority", None), ("getthreadpriority", None),
        ("getcurrentthread", None), ("openthread", None),
        ("getexitcodethread", None), ("getcurrentthreadid", None),
        ("setthreaddescription", None), ("getthreaddescription", None),
        ("setthreadaffinitymask", None), ("setthreadidealprocessor", None),
        ("getcurrentprocess" , None), ("exitprocess", None), ("terminateprocess", None),
    ]),
    ("clock", [
        ("gettickcount", None), ("queryperformancecounter", None),
        ("queryperformancefrequency", None), ("timegettime", None),
        ("getsystemtime", None), ("getlocaltime", None), ("getSystemTime", None),
        ("systemtimetofiletime", None), ("filetimetosystemtime", None),
        ("getsystemtimeasfiletime", None), ("getsystemtimes", None),
        ("getprocesstimes", None), ("getthreadtimes", None),
        ("sleep", None), ("_sleep", None), ("_time32", None), ("_time64", None),
        ("_timespec32_get", None), ("_timespec64_get", None), ("timespec_get", None),
        ("kbhit", None),
    ]),
    ("heap", [
        ("heapalloc", None), ("heapfree", None), ("heaprealloc", None),
        ("heapcreate", None), ("heapdestroy", None), ("heapsetinformation", None),
        ("heapvalidate", None), ("heapcompact", None), ("heapwalk", None),
        ("heapqueryinformation", None), ("heapsize", None),
        ("getprocessheap", None), ("getprocessheaps", None),
        ("rtlallocateheap", None), ("rtlfreeheap", None), ("rtlreallocheap", None),
        ("rtlsizeheap", None), ("rtlcreateheap", None), ("rtldestroyheap", None),
        ("localalloc", None), ("localfree", None), ("localrealloc", None),
        ("globalalloc", None), ("globalfree", None), ("globalrealloc", None),
        ("globallock", None), ("globalunlock", None),
        ("cotaskmemalloc", None), ("cotaskmemfree", None), ("cotaskmemrealloc", None),
        ("cogetmalloc", None), ("virtualalloc", None), ("virtualfree", None),
        ("virtualprotect", None), ("virtualquery", None), ("virtuallock", None),
        ("virtualunlock", None), ("createactctx", None),
        ("malloc", None), ("free", None), ("calloc", None), ("realloc", None),
        ("_malloc_base", None), ("_free_base", None), ("_recalloc", None),
        ("_aligned_malloc", None), ("_aligned_free", None),
        ("_expand", None), ("_msize", None),
    ]),
    ("fileio", [
        ("createfile", None), ("readfile", None), ("readfileex", None),
        ("writefile", None), ("writefileex", None), ("closehandle", None),
        ("setfilepointer", None), ("setfilepointerex", None), ("getfilesize", None),
        ("getfilesizeex", None), ("flushfilebuffers", None), ("lockfile", None),
        ("unlockfile", None), ("setendoffile", None), ("getfileinformationbyhandle", None),
        ("setfiletime", None), ("getfiletime", None), ("getfiletype", None),
        ("findfirstfile", None), ("findnextfile", None), ("findclose", None),
        ("deletefile", None), ("movefile", None), ("copyfile", None),
        ("createdirectory", None), ("removedirectory", None),
        ("getfileattributes", None), ("setfileattributes", None),
        ("getfullpathname", None), ("getlongpathname", None), ("getshortpathname", None),
        ("gettempfilename", None), ("gettemppath", None),
        ("getcurrentdirectory", None), ("setcurrentdirectory", None),
        ("getdiskfreespace", None), ("getvolumeinformation", None),
        ("createfilemapping", None), ("openfilemapping", None),
        ("mapviewoffile", None), ("mapviewoffileex", None),
        ("unmapviewoffile", None), ("duplicatehandle", None),
        ("getstdhandle", None), ("writeconsole", None), ("readconsole", None),
    ]),
]

CRT_RUNTIME_DLL_HINTS = ("ucrtbase", "msvcrt", "msvcp", "vcruntime", "api-ms-win-crt",
                         "api-ms-win-cpp", "msvcr", "vcomp")

CRT_STARTUP_SYMBOLS = {
    "_initterm": "crt_initializer_table_walk",
    "_initterm_e": "crt_initializer_table_walk",
    "_initialize_onexit_table": "atexit_table",
    "_execute_onexit_table": "atexit_table",
    "_register_onexit_function": "atexit_table",
    "_register_thread_local_exe_atexit_callback": "tls_atexit",
    "_crt_atexit": "atexit",
    "atexit": "atexit",
    "_onexit": "atexit",
    "__set_app_type": "crt_startup",
    "__security_init_cookie": "stack_cookie_init",
    "_encode_pointer": "pointer_encode",
    "_decode_pointer": "pointer_encode",
    "__p__commode": "crt_startup",
    "_amsg_exit": "crt_abort_msg",
    "_XcptFilter": "crt_seh_filter",
    "__getmainargs": "crt_main_args",
    "_get_initial_narrow_environment": "crt_env",
    "_configure_narrow_environment": "crt_env",
    "_initialize_narrow_environment": "crt_env",
    "_initialize_wide_environment": "crt_env",
    "__setusermatherr": "crt_matherr",
    "_cexit": "crt_exit",
    "_exit": "crt_exit",
    "_crt_debugger_hook": "crt_debug",
    "__crt_debugger_hook": "crt_debug",
    "__report_gsfailure": "crt_gs_failure",
    "_invalid_parameter_noinfo": "crt_invalid_param",
    "_invalid_parameter_noinfo_noreturn": "crt_invalid_param",
    "__stdio_common_vfprintf": "stdio_common",
    "__stdio_common_vfwprintf": "stdio_common",
    "__stdio_common_vsprintf": "stdio_common",
    "__stdio_common_vsnprintf_s": "stdio_common",
    "errno": "crt_tls_errno",
    "_errno": "crt_tls_errno",
}


def classify(name: str):
    low = name.lower()
    for family, rules in FAMILY_RULES:
        for prefix, subtag in rules:
            if low.startswith(prefix):
                return family, subtag
    # CRT fallback: anything imported from a CRT runtime DLL or with a known
    # CRT startup symbol is crt.
    if low in CRT_STARTUP_SYMBOLS:
        return "crt", CRT_STARTUP_SYMBOLS[low]
    return None, None


def classify_for_library(name: str, library_lower: str):
    fam, sub = classify(name)
    if fam is not None:
        return fam, sub
    if any(h in library_lower for h in CRT_RUNTIME_DLL_HINTS):
        return "crt", "crt_runtime_dll_member"
    return "uncategorized", None


# -------------------------------------------------------------------- PE -----
class PE:
    def __init__(self, data: bytes, path: str):
        self.data, self.path = data, path
        if len(data) < 0x40 or data[:2] != b"MZ":
            raise ValueError("not an MZ image")
        self.e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        if data[self.e_lfanew:self.e_lfanew + 4] != b"PE\x00\x00":
            raise ValueError("no PE signature")
        coff = self.e_lfanew + 4
        (self.machine, self.num_sections, self.timestamp, _sym_ptr, _num_syms,
         self.size_opt, self.characteristics) = struct.unpack_from("<HHIIIHH", data, coff)
        opt = coff + 20
        self.magic = struct.unpack_from("<H", data, opt)[0]
        if self.magic not in (0x10B, 0x20B):
            raise ValueError("not PE32/PE32+ optional header")
        self.plus = self.magic == 0x20B
        base_off = opt + 24
        self.image_base = struct.unpack_from("<Q" if self.plus else "<I", data, base_off)[0]
        self.subsystem = struct.unpack_from("<H", data, opt + 68)[0]
        self.dll_characteristics = struct.unpack_from("<H", data, opt + 70)[0]
        nrva_off = opt + (108 if self.plus else 92)
        self.num_rva = struct.unpack_from("<I", data, nrva_off)[0]
        self.datadirs = []
        dd = nrva_off + 4
        for i in range(self.num_rva):
            va, size = struct.unpack_from("<II", data, dd + i * 8)
            self.datadirs.append((va, size))
        self.sections = []
        sec = opt + self.size_opt
        for i in range(self.num_sections):
            name = data[sec + i * 40: sec + i * 40 + 8].rstrip(b"\x00").decode("latin1")
            vsz, va, rsz, raw = struct.unpack_from("<IIII", data, sec + i * 40 + 8)
            self.sections.append((name, va, vsz, rsz, raw))

    def rva_off(self, rva: int):
        for name, va, vsz, rsz, raw in self.sections:
            span = max(vsz, rsz)
            if span and va <= rva < va + span:
                off = rva - va + raw
                if off < len(self.data):
                    return off
        # headers region (e.g. IAT inside header)
        if rva < (self.datadirs and 0 or 0) or True:
            if rva < 0x1000 and rva < len(self.data):
                return rva
        raise ValueError(f"rva 0x{rva:x} unmapped")

    def cstr(self, off: int):
        end = self.data.find(b"\x00", off)
        return self.data[off:end].decode("latin1")

    def read_imports(self, dir_index, delay=False):
        if dir_index >= len(self.datadirs):
            return []
        va, size = self.datadirs[dir_index]
        if not va or not size:
            return []
        results = []
        if not delay:
            off = self.rva_off(va)
            while True:
                oft, _ts, _fwd, name_rva, ft = struct.unpack_from("<IIIII", self.data, off)
                if oft == 0 and name_rva == 0 and ft == 0:
                    break
                lib = self.cstr(self.rva_off(name_rva))
                entries = self._walk_thunks(oft or ft)
                results.append((lib, entries))
                off += 20
        else:
            off = self.rva_off(va)
            while True:
                attrs, name_field, _phmod, _iat, int_field, _unl, _ts = \
                    struct.unpack_from("<IIIIIII", self.data, off)
                if attrs == 0 and name_field == 0:
                    break
                rva_mode = bool(attrs & 1)
                name_rva = name_field if rva_mode else (name_field - self.image_base)
                int_rva = int_field if rva_mode else (int_field - self.image_base)
                try:
                    lib = self.cstr(self.rva_off(name_rva))
                    entries = self._walk_thunks(int_rva)
                except ValueError:
                    lib, entries = f"<unresolved-delay-name@0x{name_field:x}>", []
                results.append((lib, entries))
                off += 32
        return results

    def _walk_thunks(self, rva: int):
        out = []
        if not rva:
            return out
        off = self.rva_off(rva)
        fmt, obit = ("<Q", 1 << 63) if self.plus else ("<I", 1 << 31)
        step = 8 if self.plus else 4
        i = 0
        while True:
            (val,) = struct.unpack_from(fmt, self.data, off + i * step)
            if val == 0:
                break
            if val & obit:
                out.append({"ordinal": val & 0xFFFF})
            else:
                try:
                    noff = self.rva_off(val)
                    hint = struct.unpack_from("<H", self.data, noff)[0]
                    out.append({"name": self.cstr(noff + 2), "hint": hint})
                except ValueError:
                    out.append({"unresolved_rva": hex(val)})
            i += 1
            if i > 100000:
                break
        return out

    def read_exports(self):
        if 0 >= len(self.datadirs):
            return None
        va, size = self.datadirs[0]
        if not va or not size:
            return None
        try:
            off = self.rva_off(va)
            (_c, _ts, _mj, _mn, name_rva, ord_base, nfun, nnames,
             _pfun, pnames, _pords) = struct.unpack_from("<IIHHIIIIIII", self.data, off)
            names = []
            noff = self.rva_off(pnames)
            for i in range(min(nnames, 256)):
                (nrva,) = struct.unpack_from("<I", self.data, noff + i * 4)
                names.append(self.cstr(self.rva_off(nrva)))
            return {"dll_name": self.cstr(self.rva_off(name_rva)),
                    "ordinal_base": ord_base, "functions": nfun,
                    "named": nnames, "names_sample": names}
        except (ValueError, struct.error):
            return {"parse_error": True}

    def read_tls(self):
        if 9 >= len(self.datadirs):
            return {"directory_present": False}
        va, size = self.datadirs[9]
        if not va or not size:
            return {"directory_present": False}
        out = {"directory_present": True}
        try:
            off = self.rva_off(va)
            if self.plus:
                start, end, idx, cb, zero, chars = struct.unpack_from("<QQQQII", self.data, off)
                out["raw_data_bytes"] = end - start if end >= start else 0
                out["address_of_index_rva"] = idx
                out["callbacks_rva"] = cb
                out["size_of_zero_fill"] = zero
                out["characteristics"] = chars
            else:
                start, end, idx, cb, zero, chars = struct.unpack_from("<IIIIII", self.data, off)
                out["raw_data_bytes"] = end - start if end >= start else 0
                out["address_of_index_rva"] = idx
                out["callbacks_rva"] = cb
                out["size_of_zero_fill"] = zero
                out["characteristics"] = chars
            cbs = []
            cb_field_kind = "rva"
            if cb:
                # TLS dir fields are RVAs per spec but MSVD/older linkers emit
                # VAs; try RVA first, fall back to VA-minus-image-base.
                for cand, kind in ((cb, "rva"), (cb - self.image_base, "va")):
                    try:
                        coff = self.rva_off(cand)
                        fmt, step = ("<Q", 8) if self.plus else ("<I", 4)
                        found = []
                        for i in range(64):
                            (v,) = struct.unpack_from(fmt, self.data, coff + i * step)
                            if v == 0:
                                break
                            found.append(hex(v))
                        if found:
                            cbs, cb_field_kind = found, kind
                            break
                        if not found and kind == "va":
                            # zero callbacks under both readings is valid
                            cb_field_kind = kind
                    except ValueError:
                        continue
            out["callbacks"] = cbs
            out["callbacks_field_kind"] = cb_field_kind
            out["uses_static_tls"] = bool(out.get("address_of_index_rva"))
        except (ValueError, struct.error) as e:
            out["parse_error"] = str(e)
        return out


def census_dll(data: bytes, path: str):
    pe = PE(data, path)
    imports = pe.read_imports(1)
    delay = pe.read_imports(13, delay=True)
    tls = pe.read_tls()
    exports = pe.read_exports()
    is_dll = bool(pe.characteristics & 0x2000)

    fam_counts = {}
    crt = {"runtime_dlls_imported": [], "startup_imports": [], "static_crt_likely": False}
    import_table = []
    for lib, entries in imports:
        lib_lower = lib.lower()
        if any(h in lib_lower for h in CRT_RUNTIME_DLL_HINTS):
            crt["runtime_dlls_imported"].append(lib)
        lib_json = {"library": lib, "thunks": [], "families": {}}
        for e in entries:
            if "name" in e:
                fam, sub = classify_for_library(e["name"], lib_lower)
                key = fam if not sub else f"{fam}:{sub}"
                fam_counts[fam] = fam_counts.get(fam, 0) + 1
                lib_json["families"][key] = lib_json["families"].get(key, 0) + 1
                lib_json["thunks"].append({"name": e["name"], "family": fam,
                                           "subtag": sub})
                if fam == "crt" and e["name"].lower() in CRT_STARTUP_SYMBOLS:
                    crt["startup_imports"].append(
                        {"name": e["name"], "role": CRT_STARTUP_SYMBOLS[e["name"].lower()],
                         "library": lib})
            elif "ordinal" in e:
                fam_counts["uncategorized"] = fam_counts.get("uncategorized", 0) + 1
                lib_json["thunks"].append({"ordinal": e["ordinal"], "family": "uncategorized"})
            else:
                lib_json["thunks"].append(e)
        import_table.append(lib_json)

    delay_table = []
    for lib, entries in delay:
        lib_lower = lib.lower()
        lib_json = {"library": lib, "thunks": [], "families": {}, "delay": True}
        for e in entries:
            if "name" in e:
                fam, sub = classify_for_library(e["name"], lib_lower)
                key = fam if not sub else f"{fam}: {sub}"
                lib_json["families"][key] = lib_json["families"].get(key, 0) + 1
                lib_json["thunks"].append({"name": e["name"], "family": fam, "subtag": sub})
            elif "ordinal" in e:
                lib_json["thunks"].append({"ordinal": e["ordinal"], "family": "uncategorized"})
            else:
                lib_json["thunks"].append(e)
        delay_table.append(lib_json)

    if not crt["runtime_dlls_imported"] and is_dll:
        # no CRT runtime DLL imported while CRT-ish symbols are absent -> either
        # static CRT or freestanding; heuristic flags for review, never asserts.
        crt["static_crt_likely"] = any(
            f == "crt" for f in fam_counts) or bool(tls.get("directory_present"))

    bound = len(pe.datadirs) > 11 and bool(pe.datadirs[11][0])
    return {
        "path": path,
        "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data),
        "is_dll": is_dll,
        "machine": MACHINES.get(pe.machine, hex(pe.machine)),
        "pe32_plus": pe.plus,
        "subsystem": SUBSYSTEMS.get(pe.subsystem, pe.subsystem),
        "link_timestamp_utc": datetime.datetime.fromtimestamp(
            pe.timestamp, datetime.timezone.utc).isoformat() if pe.timestamp else None,
        "dll_characteristics": hex(pe.dll_characteristics),
        "sections": [s[0] for s in pe.sections],
        "imports": import_table,
        "delay_imports": delay_table,
        "bound_imports_present": bound,
        "tls": tls,
        "crt": crt,
        "exports": exports,
        "family_summary": dict(sorted(fam_counts.items(), key=lambda kv: -kv[1])),
        "import_total": sum(fam_counts.values()),
    }


def iter_sources(paths):
    for p in paths:
        p = os.path.abspath(p)
        if os.path.isdir(p):
            for root, _dirs, files in os.walk(p):
                for f in sorted(files):
                    yield os.path.join(root, f), None
        elif p.lower().endswith(".zip"):
            with zipfile.ZipFile(p) as z:
                for info in z.infolist():
                    if info.is_dir():
                        continue
                    yield info.filename, z.read(info)
        else:
            yield p, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inputs", nargs="+", help="archives (.zip), PE files, or dirs")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--include-exes", action="store_true",
                    help="also census EXE images (default: DLLs only)")
    args = ap.parse_args()

    report = {
        "tool": "inspect_dll_census.py",
        "tool_version": TOOL_VERSION,
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "families": ["crt", "heap", "tls", "loader", "clock", "workers", "critical",
                     "address-wait", "srw", "init_once", "mutex", "fileio",
                     "uncategorized"],
        "coverage_notes": {
            "mutex": "worker-delivered per backlog",
            "fileio": "in-flight per backlog",
        },
        "inputs": args.inputs,
        "scanned": 0,
        "skipped_non_pe": [],
        "skipped_exe": [],
        "dlls": [],
    }
    for path, blob in iter_sources(args.inputs):
        data = blob
        if data is None:
            try:
                with open(path, "rb") as fh:
                    data = fh.read()
            except OSError:
                report["skipped_non_pe"].append(path)
                continue
        report["scanned"] += 1
        try:
            entry = census_dll(data, path)
        except ValueError as e:
            report["skipped_non_pe"].append(f"{path} ({e})")
            continue
        if not entry["is_dll"] and not args.include_exes:
            report["skipped_exe"].append(path)
            continue
        report["dlls"].append(entry)

    with open(args.out, "w") as fh:
        json.dump(report, fh, indent=1)
    total = sum(d["import_total"] for d in report["dlls"])
    print(f"scanned={report['scanned']} dlls={len(report['dlls'])} "
          f"imports_total={total} -> {args.out}")
    for d in report["dlls"]:
        print(f"  {os.path.basename(d['path'])}: {d['machine']} "
              f"imports={d['import_total']} families={d['family_summary']} "
              f"static_tls={d['tls'].get('uses_static_tls', False)}")


if __name__ == "__main__":
    sys.exit(main())
