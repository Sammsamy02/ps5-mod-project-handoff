#!/usr/bin/env python3
"""l26 twin-variant runtime build driver (l25 machinery repointed by agent CM).

Produces TWO build variants from one tree state, reusing build_runtime.py
helpers WITHOUT modifying that base driver:

  variant A "l26-A": PRODUCTION (l26 default = VARIANT C ON): compiled with
      -DMODBRIDGE_L25_ENGINE_ADD_LISTENER so delete-listener registration
      delegates to the ENGINE'S OWN Add/RemoveUObjectDeleteListener through
      pointers that admission verified against the cached image prologue
      signatures (Claude review 3 + user OK; agent EE integration). No skip
      token. The B skip path is removed from production.
  variant B "l26-B": diagnostic only. Token translation units compile with
      -DMODBRIDGE_L25_SKIP_DELETE_LISTENER (exact spelling — the token keeps
      its L25-era name because the guards already landed in the tree use
      that spelling; renaming it would be a source edit) so the call sites
      wrapped by the listener agent in
      typescan-r4/candidate/src/UnrealInitializer.cpp (and LuaMod.cpp)
      take the #else skip+log branch and the delete-listener registration
      never runs.

BASE is the l25-built production state outputs/2026-10-08-l25-build-variants/
l25-A (the normal-registration twin; l25-B is the UNSAFE diagnostic and is
never a BASE). The variant-dir base is structurally identical to the historical
build-r2 bases: SOURCE-SELECTION.json, pass1/pass2 COMPOSITION-RECEIPT.json
with pinned argvs/objects/dependencies, undefined.stdout baselines and
LINK-COMMAND.json all live under l25-A/. Rows in the part CHANGES.json files
are keyed on the identities the l25-A build actually consumed (the l25-runtime
candidates, and the composed staging file for UnrealInitializer.cpp).

Each variant is built as an independent pass1/pass2 twin exactly the way
build_runtime.py builds the single l26 runtime (same gates, same receipts,
same determinism discipline: per-variant composed-object hashes pinned in
COMPOSITION-RECEIPT/TWIN-RECEIPT). A cross-variant pair receipt proves the
two runtimes differ and confines per-unit object deltas to the units that
received the variant define.

--plan performs NO toolchain execution: it recomputes the selection from the
current CHANGES.json state, verifies every pin it can, and lists every step,
input hash and gate. The real build runs only with --execute after the
parallel logwriter/UFunction-fix agents land their edits and refresh their
row hashes.

INCREMENTAL MODE (--incremental, agent EZ 2026-10-09): recompiles ONLY the
units whose SOURCE .cpp carries a CHANGES row (the cpp-row fresh set) and
RETAINS every other unit's object from a previous completed build
(--base-build DIR; default: the most recent completed l26-A*/l26-B* variant
dir, key-matched per variant so a skip-token object can never be retained
into the production variant). Header-replacement rows are deliberately NOT
cascaded into the fresh set in this mode: their consumers keep the retained
objects compiled by the previous build (mixed-header ODR risk — the driver
prints an explicit skipped-header warning with consumer counts). Nothing is
trusted blindly: every unit slated for retention is re-verified against the
previous build's own pins (source, object, dependency log, and each dep file
pinned by the previous SOURCE-SELECTION); any drifted unit is forced back
into the fresh set, and a variant define reaching only retained units must
be proven by the previous same-variant build's marker define_units. The G12
header-routing RESOLUTION checks still run for every compiled unit; the
G12b consumer-recompile coverage half (and G2t presence, if applicable) is
waived to an explicit warning. Incremental is a LOCAL ITERATION accelerator
only (target: a .cpp-only edit rebuilds a handful of units in ~1-2 min):
the default full build remains the ONLY mode for upload-grade builds unless
the operator explicitly passes --incremental. --execute still refuses to
overwrite existing variant dirs, so archive/rename the previous build (or
point --base-build at it) before an incremental execute.

CONCURRENT TWINS (--execute only, agent FY 2026-10-10): the variant loop and
the pass loop were sequential only by accident of code structure — pass2 has
no data dependency on pass1 (both read only the frozen l25-A BASE pass
receipts and write to their own pass dir; the variant overlay and marker are
prepared before either pass), and variant B never reads variant A's outputs.
Execute now runs all four (variant,pass) tasks concurrently in one pool of 4.
Gate and receipt CONTENT is unchanged: the EXECUTION-RECEIPT gates list is a
deterministic merge (A-pass1, A-pass2, B-pass1, B-pass2, then the serial
finisher/pair gates), stdout gate lines interleave atomically under a print
lock, and per-pass DEPENDENCY-GRAPH/COMPOSITION receipts are written by the
serial finisher because pass2's dependency graph is cumulative over pass1's
pins (historical behavior preserved: pass1 graph = pass1 pins, pass2 graph =
merged pins). Global clang/lld subprocess concurrency is capped by env
L26_BUILD_JOBS (integer >=1; default min(6, ncpu) — 8 logical cores / 16 GB
on this Mac). --plan remains fully sequential and byte-identical to the
pre-FY driver; --incremental composes with the concurrent path unchanged.

Rules honored: no console/network contact; never edits
typescan-r4/candidate/src or include/ (owned by the code agents); the base
driver build_runtime.py is imported, never rewritten.

[P2, agent FX 2026-10-10, PARKED patch set] variant B (l26-B, the
MODBRIDGE_L25_SKIP_DELETE_LISTENER diagnostic twin) is DEAD BY DEFAULT:
env opt-in L26_BUILD_B=1 only (default off). Its UNSAFE skip-listener
diagnostic purpose ended when variant C's engine-call registration was
proven on console; the P2 tree patch removes the skip arms themselves, so an
opt-in B --execute fails LOUDLY at G4-token-present (token absent from every
compiled source) and can never silently build a B identical to A. The A/B
pair gates (G4-token-present, G9, G10) and the pair receipts run ONLY when B
is opted in. The env-append mechanism itself stays so EO's NB control
(parallel-candidate/02) and any future diagnostic sibling append the same
way.
"""
import argparse,concurrent.futures,copy,hashlib,importlib.util,json,os,subprocess,sys,tempfile,threading
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sys.dont_write_bytecode=True
_spec=importlib.util.spec_from_file_location('build_runtime_l25_base',HERE/'build_runtime.py')
br=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(br)
BASE=br.BASE;LLVM=br.LLVM
sha,pin,write,deps,rewrite_compile,run=br.sha,br.pin,br.write,br.deps,br.rewrite_compile,br.run

TOKEN='MODBRIDGE_L25_SKIP_DELETE_LISTENER'
# [NEW-UNITS root 2026-10-10] Claude round-2 TUs with no l25-A BASE unit:
# lua-lfb (agent FD: bridge out-of-line definitions), buildstamp + tcselftest
# (agent FF: [BUILD] stamp TU + wcslen/wstring probes). They cannot enter via
# CHANGES rows (rows replace BASE-consumed sources only — the lfb phantom
# class), so the driver registers them as first-class units: cloned argv from
# a template unit (same include set), rewritten -c/-o/-MF by the normal
# compile_unit path, objects APPENDED to the BASE link command (the zip
# remap pairs BASE[i]↔units[i]; new units append at the END so the pairing
# is preserved), census expected 391+len(NEW_UNITS). Always fresh (never
# retained by --incremental; no previous receipts exist for them).
NEW_UNITS={
    'pakmount':{'source':HERE/'typescan-r4/candidate/src/RuntimePakMount.cpp','template':'unreal-unrealinitializer','note':'cap07 synchronized native pak mount (always fresh)','source_sha256':'f29af9ca0c75ce0aac2046875cfe1969073a54df1383d29a0eee2491a488c33d'},
    'lua-lfb':{'source':HERE/'typescan-r4/candidate/src/LuaFNameBridge.cpp','template':'luamod','note':'agent FD bridge definitions (to_fstring/to_ftext/push_*_to_lua/fname_to_narrow)'},
    'buildstamp':{'source':HERE/'typescan-r4/candidate/src/BuildStamp.cpp','template':'unreal-unrealinitializer','note':'agent FF [BUILD] stamp TU (__has_include GeneratedBuildStamp.hpp)'},
    'tcselftest':{'source':HERE/'typescan-r4/candidate/src/ToolchainSelfTest.cpp','template':'unreal-unrealinitializer','note':'agent FF wcslen/wstring-rt toolchain probes'},
    'lua-pathshim':{'source':HERE/'typescan-r4/candidate/src/LuaPathShim.cpp','template':'luamod','note':'agent GA io/path virtual-cwd shim (whole-TU __PROSPERO__)'},


}
def make_new_units(units,passno,sel,objects):
    by_name={u['name']:u for u in units}
    made=[]
    for name,spec in NEW_UNITS.items():
        if spec.get('source_sha256') and sha(spec['source'])!=spec['source_sha256']:raise RuntimeError('registered NEW_UNIT source pin drift: '+name)
        tpl=by_name[spec['template']]
        u=copy.deepcopy(tpl)
        argv=list(u['argv'])
        assert argv.count(str(u['source']))==1, 'template argv must mention its own source once: '+spec['template']
        argv[argv.index(str(u['source']))]=str(spec['source'])
        u.update(name=name,source=str(spec['source']),source_sha256=sha(spec['source']),
                 argv=argv,selection='l26_new_unit_root_registered (no BASE counterpart; Claude round-2)')
        # object/dependency pins stay INHERITED from the template so
        # compile_unit's entry asserts (which validate inherited pins before
        # the fresh path rewrites them) pass in plan mode; the link remap
        # maps template-object -> this build's new object for the new name.
        made.append(u)
    return made
# [Seeds][L26] VARIANT C production switch (agent EE, Claude review 3 + user
# OK): engine-side delete-listener registration through the admission-verified
# engine FUObjectArray::Add/RemoveUObjectDeleteListener pointers
# (EngineListenerOpsProfile.hpp + UnrealInitializer.cpp Phase-6 cached
# prologue-signature admission; UObjectArray.cpp engine-call path, fail-closed
# refusal codes 8/9). PRODUCTION DEFAULT = C-ON: variant A carries this define
# and NO skip token; the B skip token stays DIAGNOSTIC-ONLY (variant B) and is
# removed from production per the same review.
CTOKEN='MODBRIDGE_L25_ENGINE_ADD_LISTENER'
# [LUA-AS-CPP v2] STAGED (lua-as-cpp track 2026-10-12, round 2; needs the
# user's OK and its own labelled build). Opt-in env L26_LUA_AS_CPP=1; unset =
# today's build, byte-for-byte the same selection.
# Compiles ONLY the 33 luaraw-* units as C++ so lua_error / coroutine.yield
# become C++ throws (ldo.c:55-61) and C++ destructors run in every frame a Lua
# error crosses (PS5/FreeBSD longjmp skips them). Every public Lua symbol keeps
# its C name: lua-c-abi/lua_c_abi_prefix.h is force-included (-include) and
# sets LUA_API to extern "C" (Lua's own customisation point, luaconf.h:282-
# 306). Upstream's LUA_COMPILED_AS_CPP (C++ linkage) is NOT used: the frozen
# plain-C packaging unit lineage/probe/memory_lua.c calls 8 Lua functions by
# C name and would no longer link. So no other unit gets a new define and no
# caller is recompiled for this change.
#  * luaraw-* argv: prospero-clang -> prospero-clang++ (adds -frtti
#    -fexceptions, prospero-clang++:83), -std=c11 -> -std=c++23
#    -Wno-deprecated-enum-enum-conversion -include <prefix>, '-x c++' before
#    -c, and NO -resource-dir (no C++ unit has one).
#  * include-graph gate: for these 33 units a NEW dependency row is also
#    approved when the same resolved path with the same sha256 is already a
#    pinned dependency of the BASE build; every such row is recorded (gate
#    G-LUACPP-include-exemption). The prefix lives under this directory, so
#    the existing HERE rule covers it; its sha is pinned below.
#  * G7: the raw import delta must equal LUA_CPP_IMPORT_DELTA exactly
#    (Mac probe v2: only luaraw-ldo.o used setjmp/longjmp; the 7 new C++ EH
#    references resolve inside the runtime). The FINAL PRX still imports both
#    (the frozen lineage import table import_references.S lists them), so the
#    packaging gates P7/P11 stay at 207 and l26_packaging_driver.py is NOT
#    changed.
LUA_AS_CPP=os.environ.get('L26_LUA_AS_CPP')=='1'
LUA_C_ABI_PREFIX=HERE/'lua-c-abi'/'lua_c_abi_prefix.h'
LUA_C_ABI_PREFIX_SHA256='96b3f261366d8a131627a265463d0d5b9cce6851da406eb8ca9685387fc638ea'
LUA_CPP_EXPECTED_LUARAW_UNITS=33
LUA_CPP_IMPORT_DELTA={'removed':[('U','longjmp'),('U','setjmp')],'added':[]}
def lua_cpp_argv(argv):
    if sha(LUA_C_ABI_PREFIX)!=LUA_C_ABI_PREFIX_SHA256:raise RuntimeError('lua_c_abi_prefix.h sha drift')
    a=list(argv)
    if not a[0].endswith('/prospero-clang'):raise RuntimeError('luaraw unit not compiled by prospero-clang: '+a[0])
    a[0]=a[0]+'++'
    if a.count('-std=c11')!=1 or a.count('-resource-dir')!=1 or a.count('-c')!=1:raise RuntimeError('unexpected luaraw argv shape')
    i=a.index('-std=c11');a[i:i+1]=['-std=c++23','-Wno-deprecated-enum-enum-conversion','-include',str(LUA_C_ABI_PREFIX)]
    j=a.index('-resource-dir');del a[j:j+2]
    a[a.index('-c'):a.index('-c')]=['-x','c++']
    return a
PAIR_ROOT=ROOT/'outputs/2026-10-08-l26-build-variants'
QPART=HERE/'copyin-probe'
QCOMPOSED=PAIR_ROOT/'staging-copyin-probe'/'UnrealInitializer.cpp'
VARIANTS=(
 {'key':'A','dir':PAIR_ROOT/'l26-A','artifact':'runtime-l26-A.o',
  'token':CTOKEN,
  'define':'-D'+CTOKEN,'registration':'engine-call via admission-verified engine FUObjectArray::Add/RemoveUObjectDeleteListener (variant C; fail-closed prologue-signature admission)',
  'label':'l26-A production: engine-side delete-listener registration (variant C on, B skip off)',
  'unsafe':False,
  'status':'MAC_ONLY_L26_VARIANT_A_COMPLETE_LINK_ENGINE_LISTENER',
  'twin_status':'MAC_ONLY_IDENTICAL_L26_VARIANT_A_RUNTIME_PAIR_ENGINE_LISTENER'},
)
# [L26][P2] (agent FX, 2026-10-10; parked patch set under
# outputs/2026-10-08-l25-prep/parked/P2-variant-b-removal/): variant B is
# DEAD BY DEFAULT - dropped from the production tuple. Opt-in ONLY via
# L26_BUILD_B=1. With the P2 tree patch (02-tree-dead-code.patch) applied
# the skip-listener arms are gone from the tree, so an opt-in --execute
# fails LOUDLY at G4-token-present (token not found in the tree) - it can
# never silently build a B that equals A. To make B buildable again: revert
# the P2 tree patch AND this hunk together. The env-append mechanism stays
# so EO's NB control (parallel-candidate/02-build-variants-nb-env.patch)
# appends NB_VARIANT exactly the same way.
B_VARIANT={'key':'B','dir':PAIR_ROOT/'l26-B','artifact':'runtime-l26-B.o',
  'token':TOKEN,
  'define':'-D'+TOKEN,'registration':'skipped (diagnostic only)',
  'label':'l26-B delete-listener registration SKIPPED via '+TOKEN,
  'unsafe':True,
  'status':'MAC_ONLY_L26_VARIANT_B_COMPLETE_LINK_SKIP_DELETE_LISTENER',
  'twin_status':'MAC_ONLY_IDENTICAL_L26_VARIANT_B_RUNTIME_PAIR_SKIP_DELETE_LISTENER'}
if os.environ.get('L26_BUILD_B')=='1':
    VARIANTS=VARIANTS+(B_VARIANT,)
UNSAFE_NOTE=('UNSAFE FOR LONG-TERM USE: this variant compiles with '+TOKEN+
 ' so the delete-listener registration is skipped and stale object pointers '
 'are NOT cleared. Diagnostic A/B isolation only; never promote as the '
 'long-lived runtime and never deploy without an explicit cleanup plan.')
SCAN_SUFFIXES={'.cpp','.cc','.cxx','.c','.hpp','.h','.inl','.inc'}
SCAN_SKIP_PARTS={'tests','original','results'}
# agent O's listener-instrumentation rows: these units MUST be recompiled in
# every variant (O final report section 0b item 6; Z9 audit item 8). If any
# shows as inherited/skipped, a row silently no-oped and the build is wrong.
O_UNITS=('unreal-uobjectarray','unreal-flat-classlistener','lua-luauobject','unreal-unrealinitializer')
# Z13 order-hazard guards: symbols Q's patches DEFINE then USE. The first
# occurrence in the composed/adopted file must be the definition, and it must
# precede every use. The 2026-10-08 execute failure (use of undeclared
# CopyinProbeLiveness at composed :982, struct landed at :1067 after a
# drifted-context match) is exactly this class.
UE4SS_ORDER_SYMBOLS={'CopyinProbeLiveness':{'definition':'struct CopyinProbeLiveness','use':'CopyinProbeLiveness::'}}
BRIDGE_ORDER_SYMBOLS={name:{'definition_prefix':'static ','use':name} for name in (
 'g_copyin_probe_attempts','g_copyin_probe_attempts_late','g_copyin_probe_last_note_ms',
 'g_copyin_retirement_logged','kCopyinProbeNoteIntervalMs')}

# ------------------------------------------------------- build stamp (agent FG)
# Claude build-discipline hardening (2026-10-09, after the lfb-211254 phantom
# build): every --execute stamps the exact build provenance into the candidate
# tree BEFORE any unit compiles (G-stamp-present refuses otherwise), and
# --plan deletes any stale stamp so an unstamped tree compiles to the
# __has_include fallback value in the [BUILD] first-log-line consumer
# (agent FF) instead of a leftover header from an older build. The frozen
# snapshot id arrives via env BRIDGE_SNAPSHOT_ID (harvest/snapshot_tree.sh
# prints it; the parent wires the two commands — see that script's header).
# GeneratedBuildStamp.hpp is a DRIVER-OWNED generated file: it is not a
# CHANGES row, never edits code-agent sources, and rides -I through
# STAMP_INCLUDE_DIR so <GeneratedBuildStamp.hpp> resolves in every unit.
STAMP_PATH=HERE/'typescan-r4/candidate/include/GeneratedBuildStamp.hpp'
STAMP_INCLUDE_DIR=str(STAMP_PATH.parent)
# the not-generated fallback value spelled indirectly so this source file can
# never itself satisfy a marker grep for it
STAMP_UNSET=('UNK'+'NOWN')

# ---------------------------------------------------------------- selection
def compute_selection(record,tolerate_drift=False,incremental=False):
    """Mirror of the build_runtime.main() selection stage (same asserts).
    Plan mode passes tolerate_drift=True so mid-flight edits by the parallel
    code agents are REPORTED (gate G2 fails, plan continues) instead of
    crashing; --execute keeps the hard pin discipline.
    incremental=True (agent EZ) computes the cpp-only fresh set: units whose
    SOURCE .cpp carries a CHANGES row. Header rows still populate
    replacements/aliases (the vfs overlay still routes them; G12 resolution
    checks still run per compiled unit) but they no longer cascade consumers
    into the fresh set — consumers keep retained objects from the previous
    build (see prepare_incremental)."""
    if incremental:raise RuntimeError('cap07 CodeImageCache ABI change requires full header-consumer rebuild; incremental prohibited')
    oldsel=json.loads((BASE/'SOURCE-SELECTION.json').read_text())
    approved_header_identities={(Path(p['path']).name,p['sha256']) for p in oldsel['source_pins'] if Path(p['path']).suffix in ['.h','.hpp','.inc','.inl']}
    allpins=sum((oldsel[k] for k in ('source_pins','receipt_pins','artifact_pins')),[])
    drift=[p['path'] for p in allpins if sha(p['path'])!=p['sha256']]
    # [gate law 2026-10-12] zero pins checked is a failure, not a pass.
    record('G1-base-selection-pins',(not drift) and len(allpins)>0,{'pins':len(allpins),'drift':drift})
    if not allpins:raise RuntimeError('G1: BASE selection lists ZERO pins')
    if drift and not tolerate_drift:raise RuntimeError('BASE selection pin drift: '+repr(drift))
    changes=[];added=[];declared_include_dirs=[]
    for part in ['profile','typescan-r4','thread']:
        d=json.loads((HERE/part/'CHANGES.json').read_text());changes.extend(d['files']);added.extend(d.get('added_headers',[]));declared_include_dirs.extend(d.get('include_directories',[]))
    replacements={};header_specs=[];drift_rows=[]
    for row in changes:
        original=Path(row['original']).resolve();candidate=Path(row['candidate']).resolve()
        ok=sha(original)==row['original_sha256'] and sha(candidate)==row['candidate_sha256'] and candidate.is_relative_to(HERE)
        record('G2-part-change-pins',ok,{'candidate':str(candidate),'declared_sha256':row['candidate_sha256'],'actual_sha256':sha(candidate) if candidate.exists() else None})
        if not ok:
            if not tolerate_drift:raise RuntimeError('part CHANGES pin drift: '+str(candidate))
            drift_rows.append(str(candidate))
        assert str(original) not in replacements or replacements[str(original)]==str(candidate)
        replacements[str(original)]=str(candidate)
        if original.suffix in ['.hpp','.h','.inc','.inl']:header_specs.append((original.name,row['original_sha256'],str(candidate)))
    base=json.loads((BASE/'pass1/COMPOSITION-RECEIPT.json').read_text())
    universe=set();unitdeps={}
    for u in base['selected_units']:
        # .d files are compiler-GENERATED dep lists: a prior execute may
        # legitimately regenerate them in-place when tree headers change
        # (objects/sources are gated separately). Skip drift for *.d only.
        if str(u['dependency']).endswith('.d') and sha(u['dependency'])!=u['dependency_sha256']:
            print(json.dumps({'gate':'G-base-dep','ok':'SKIP','note':'generated .d regenerated: '+u['name']}),flush=True)
        elif sha(u['dependency'])!=u['dependency_sha256']:raise RuntimeError('base dependency drift '+u['name'])
        ds=deps(u['dependency']);unitdeps[u['name']]=ds;universe.update(ds)
    # NEW_UNITS: seed dep whitelist from the template unit (the compile-time
    # include-graph gate below compares each fresh unit's actual deps against
    # its seeded set; templates carry the full SDK/vendored include set the
    # new TU needs — lua.h, SDK headers, port headers).
    for _nu_name,_nu_spec in NEW_UNITS.items():
        _seed=set(unitdeps.get(_nu_spec['template'],set()))
        unitdeps[_nu_name]=set(unitdeps.get(_nu_spec['template'],set()))
    aliases={}
    for p in universe:
        for name,digest,candidate in header_specs:
            if p.name==name and sha(p)==digest:
                aliases[str(p)]=candidate;aliases[str(p.resolve())]=candidate
    for original,candidate in replacements.items():
        if Path(original).suffix in ['.hpp','.h','.inc','.inl']:aliases[original]=candidate
    inherited={}
    for u in base['selected_units']:
        argv=u['argv']
        for i,flag in enumerate(argv):
            if flag!='-ivfsoverlay':continue
            for row in json.loads(Path(argv[i+1]).read_text())['roots']:
                assert row['type']=='file'
                name=row['name'];target=row['external-contents']
                if name in inherited and inherited[name]!=target:
                    layer_rank=lambda path: next((i for i,layer in enumerate(('/2026-10-08-l25-runtime/','/2026-10-08-l24-runtime/','/2026-10-08-l23-runtime/')) if layer in path),99)
                    if layer_rank(target)<layer_rank(inherited[name]):inherited[name]=target
                else:inherited[name]=target
    rewritten_inherited={}
    for name,target in inherited.items():
        mapped=replacements.get(str(Path(target).resolve()),target)
        if mapped==target:
            for basename,digest,candidate in header_specs:
                if Path(target).name==basename and sha(target)==digest:mapped=candidate
        rewritten_inherited[name]=mapped
        if mapped!=target:aliases[name]=mapped
    merged_aliases={**rewritten_inherited,**aliases}
    affected_full=[]
    for u in base['selected_units']:
        if str(Path(u['source']).resolve()) in replacements or any(str(p) in aliases or str(p.resolve()) in aliases for p in unitdeps[u['name']]):affected_full.append(u['name'])
    affected=affected_full
    if incremental:
        # agent EZ: cpp-only fresh set. A unit is fresh iff its SOURCE .cpp
        # carries a CHANGES row (header rows are ignored for the fresh set;
        # their consumers keep retained objects from the previous build).
        affected=[u['name'] for u in base['selected_units'] if str(Path(u['source']).resolve()) in replacements]
        record('G-inc-cpp-only-fresh-set',True,{'fresh_units_full_mode':len(affected_full),'fresh_units_incremental':len(affected),'retained_units_incremental':len(base['selected_units'])-len(affected),'header_rows_in_changes':len(header_specs),'note':'--incremental: only units whose SOURCE .cpp carries a CHANGES row are fresh; the header-consumer cascade is skipped, G12b coverage (and G2t presence, if applicable) is waived to a warning, and retained inputs are re-verified against the previous build pins (see G-inc-retained-inputs-unchanged)'})
    # NEW_UNITS are always fresh in BOTH modes (no CHANGES rows, no previous
    # receipts to retain from) — appended after every affected-set derivation.
    affected=affected+[n for n in NEW_UNITS if n not in affected]
    include_dirs=sorted(set(declared_include_dirs)|{str(Path(p).parent) for p in added}|{str(Path(x['candidate']).parent) for x in changes if Path(x['candidate']).suffix in ['.hpp','.h']})
    # Z9 loud no-op guard: every CHANGES row must key on an original the l25-A
    # BASE build actually consumed (unit source, dep-universe path, inherited
    # overlay target, or alias-identical (name,digest) universe file). A row
    # keyed anywhere else (e.g. a pre-l25 identity no longer in the l25-A
    # consumption graph) would silently compile nothing. Non-code lineage
    # files (.json/.ini/.md) are reported but allowed; every code row must
    # classify or the build stops.
    unit_sources={str(Path(u['source']).resolve()):u['name'] for u in base['selected_units']}
    universe_res={str(Path(p).resolve()) for ds in unitdeps.values() for p in ds}
    overlay_targets={str(Path(t).resolve()) for t in inherited.values()}
    classifications={};noop=[]
    for row in changes:
        o=str(Path(row['original']).resolve());basename=Path(o).name
        if o in unit_sources:kind='unit-source'
        elif o in universe_res:kind='dep-universe'
        elif o in overlay_targets:kind='overlay-target'
        elif any(p.name==basename and sha(p)==row['original_sha256'] for p in universe):kind='alias-name-digest'
        elif Path(o).suffix in ('.json','.ini','.md'):kind='lineage-data(not-compiled)'
        else:kind='NOT-CONSUMED'
        classifications[basename]=kind
        if kind=='NOT-CONSUMED':noop.append({'original':o,'declared_sha256':row['original_sha256']})
    record('G2r-rows-consumed-by-base',not noop,{'classifications':classifications,'not_consumed':noop})
    if noop and not tolerate_drift:raise RuntimeError('CHANGES rows keyed on originals the l25-A BASE build never consumed (silent no-ops): '+repr(noop))
    unit_row_units=sorted({unit_sources[str(Path(r['original']).resolve())] for r in changes if str(Path(r['original']).resolve()) in unit_sources})
    missing=[n for n in unit_row_units if n not in affected]
    record('G2s-unit-source-rows-affected',not missing,{'units_from_cpp_rows':unit_row_units,'missing_from_affected':missing})
    if missing and not tolerate_drift:raise RuntimeError('unit-source rows whose units are not in the affected set: '+repr(missing))
    presence={u:(u in affected) for u in O_UNITS}
    presence_detail={'presence':presence,'note':'unreal-uobjectarray/unreal-flat-classlistener/lua-luauobject/unreal-unrealinitializer are fresh l25 objects in the BASE (l25-A) receipt; the l26 rows must keep all four in the affected set or a row silently no-oped. lua-luauobject rides the UObjectArray.hpp path alias (its l25-A .d consumed the l25 candidate)'}
    if incremental and not all(presence.values()):
        presence_detail['incremental_waiver']=True
        presence_detail['note']+=' | INCREMENTAL WAIVER: listener units outside the cpp-only fresh set keep retained objects from the previous same-variant build; their listener rows must already be proven fresh in that build (see G-inc-retained-inputs-unchanged / the previous receipt), never silently dropped'
        record('G2t-listener-units-affected',None,presence_detail)
    else:
        record('G2t-listener-units-affected',all(presence.values()),presence_detail)
        if not all(presence.values()) and not tolerate_drift:raise RuntimeError('listener-instrumentation units not in affected set (silent row no-op): '+repr(presence))
    # G12b (Z13 E3, coverage half): every BASE consumer of every replaced
    # header must be recompiled. A consumer left retained would keep an
    # object compiled against the pre-fix header (mixed-header ODR hazard;
    # for UObjectArray.hpp the 4-slot vs 5-slot vtable ABI repair).
    coverage=[];header_consumers={}
    for (name,digest,candidate) in header_specs:
        cons={u['name'] for u in base['selected_units'] if any(d.name==name for d in unitdeps[u['name']])}
        header_consumers[name]=len(cons)
        miss=sorted(cons-set(affected))
        if miss:coverage.append({'header':name,'consumers_not_recompiled':miss})
    if incremental:
        # G12b waiver (agent EZ): in incremental mode consumers of replaced
        # headers are retained BY DESIGN, so the coverage half becomes an
        # explicit risk report instead of a hard gate. The RESOLUTION half
        # (G12 header_route_check per compiled unit) is NOT waived: headers
        # must still resolve to the l26 candidates through -I/vfs overlay.
        record('G12b-header-consumers-recompiled',(not coverage) or None,
               {'mode':'incremental','consumers_per_header':header_consumers,'gaps':coverage,
                'waiver':'--incremental deliberately skips recompiling consumers of replaced headers; they keep retained objects from the previous build (mixed-header ODR risk accepted for LOCAL ITERATION only; NOT upload-grade). Header RESOLUTION is still enforced per compiled unit by G12'})
    else:
        record('G12b-header-consumers-recompiled',not coverage,{'consumers_per_header':header_consumers,'gaps':coverage})
        if coverage and not tolerate_drift:raise RuntimeError('replaced-header consumers left retained (mixed-header hazard): '+repr(coverage))
    lua_cpp_units=[];base_dep_pins={}
    if LUA_AS_CPP:
        lua_cpp_units=sorted(u['name'] for u in base['selected_units'] if u['name'].startswith('luaraw-'))
        pinned={str(Path(p['path']).resolve()):p['sha256'] for p in oldsel['source_pins']}
        base_dep_pins={rp:pinned[rp] for rp in universe_res if rp in pinned}
        prefix_ok=LUA_C_ABI_PREFIX.exists() and sha(LUA_C_ABI_PREFIX)==LUA_C_ABI_PREFIX_SHA256
        ok=len(lua_cpp_units)==LUA_CPP_EXPECTED_LUARAW_UNITS and len(base_dep_pins)>0 and prefix_ok
        record('G-LUACPP-scope',ok,{'luaraw_units':len(lua_cpp_units),'expected_luaraw_units':LUA_CPP_EXPECTED_LUARAW_UNITS,
               'base_pinned_dependencies':len(base_dep_pins),'prefix':str(LUA_C_ABI_PREFIX),'prefix_sha256_ok':prefix_ok,
               'other_units_changed':0,'declared_import_delta':{k:[list(t) for t in v] for k,v in LUA_CPP_IMPORT_DELTA.items()}})
        if not ok:raise RuntimeError('G-LUACPP-scope: unexpected Lua-as-C++ scope (zero count, not 33 luaraw units, or prefix sha drift)')
        affected=affected+[n for n in lua_cpp_units if n not in affected]
    return {'oldsel':oldsel,'approved_header_identities':approved_header_identities,'changes':changes,'added':added,
            'lua_cpp_units':set(lua_cpp_units),'base_dep_pins':base_dep_pins,
            'replacements':replacements,'header_specs':header_specs,'base':base,'unitdeps':unitdeps,
            'aliases':aliases,'merged_aliases':merged_aliases,'affected':affected,'include_dirs':include_dirs,
            'pin_drift_rows':drift_rows,'row_classifications':classifications,'unit_row_units':unit_row_units,
            'header_consumers':header_consumers,
            'new_units':{k:{'source':str(v['source']),'template':v['template'],'note':v['note']} for k,v in NEW_UNITS.items()},
            'incremental':incremental,'affected_full':affected_full,
            'header_skip_rows':([{'header':name,'candidate':candidate,'consumers':header_consumers[name]} for (name,digest,candidate) in header_specs] if incremental else [])}

# ------------------------------------------------- incremental mode (agent EZ)
def resolve_base_build(explicit,key):
    """Previous completed l26 variant build whose objects incremental mode
    retains for variant `key`. Never the l25-A BASE: its objects predate the
    variant defines, so retaining from it would silently build a non-variant
    runtime (the define-coverage gate enforces this). `explicit` may be a
    variant dir (its VARIANT-MARKER.json key must match) or a pair root
    containing l26-<key>* subdirs; default = the most recent completed
    PAIR_ROOT/l26-<key>* variant dir by pass2 receipt mtime."""
    def ok_dir(d):
        try:
            if not (d/'VARIANT-MARKER.json').is_file() or not all((d/f'pass{n}'/'COMPOSITION-RECEIPT.json').is_file() for n in (1,2)):return False
            return json.loads((d/'VARIANT-MARKER.json').read_text()).get('variant')==key
        except (OSError,ValueError):return False
    def newest(root):
        c=[p for p in root.glob('l26-'+key+'*') if ok_dir(p)]
        return max(c,key=lambda p:(p/'pass2'/'COMPOSITION-RECEIPT.json').stat().st_mtime) if c else None
    fallback=newest(PAIR_ROOT)
    if explicit is None:
        if fallback is not None:return fallback,'default: most recent completed l26-'+key+'* variant dir'
        return None,'no completed l26-'+key+'* variant build found; retained units would fall back to the l25-A BASE objects (pre-variant-define: G-inc-define-units-covered must then fail rather than build a non-variant runtime)'
    e=Path(explicit).resolve()
    if ok_dir(e):return e,'--base-build variant dir (marker key '+key+' matches)'
    sub=newest(e) if e.is_dir() else None
    if sub is not None:return sub,'--base-build pair root; most recent l26-'+key+'* inside it'
    if fallback is not None:return fallback,'--base-build unusable for variant '+key+' (no matching marker/receipts); fell back to the most recent default'
    return None,'--base-build unusable for variant '+key+' and no default l26-'+key+'* build exists'

def prepare_incremental(sel,record,explicit=None):
    """Resolve per-variant previous builds, load their receipts/pins, verify
    every unit slated for retention against the previous build's OWN pins
    (source, object, dependency log, and each dep file pinned by the previous
    SOURCE-SELECTION), force any drifted unit back into the fresh set (no
    stale silent retention), and record the per-variant base-build gates.
    Also emits nothing here; the operator warning is printed by main()."""
    cache={}
    def csha(p):
        k=str(p)
        if k not in cache:
            try:cache[k]=sha(p)
            except OSError:cache[k]=None
        return cache[k]
    retained={}
    # [L26][P2] retained-variant keys derive from the ACTIVE variant set
    # (B is env-gated dead by default; a dropped variant must not fail
    # G-inc-base-build on a build that never happens).
    for key in sorted({v['key'] for v in VARIANTS}):
        d,source=resolve_base_build(explicit,key)
        entry={'dir':(str(d) if d is not None else None),'source':source,'receipts':{},'marker':None,'pins':{}}
        ok=bool(d)
        if d is not None:
            try:
                entry['marker']=json.loads((d/'VARIANT-MARKER.json').read_text())
                for n in (1,2):entry['receipts'][n]=json.loads((d/f'pass{n}'/'COMPOSITION-RECEIPT.json').read_text())
                prevsel=json.loads((d/'SOURCE-SELECTION.json').read_text())
                entry['pins']={p['path']:p['sha256'] for p in prevsel.get('source_pins',[])}
                detail={'variant':key,'dir':str(d),'source':source,'marker_status':entry['marker'].get('status'),'selected_units':len(entry['receipts'][1]['selected_units']),'pin_count':len(entry['pins'])}
            except (OSError,ValueError) as error:
                ok=False
                detail={'variant':key,'dir':str(d),'error':'unreadable receipts/pins: '+repr(error)}
                entry={'dir':None,'source':source+'; receipts unreadable','receipts':{},'marker':None,'pins':{}}
        else:
            detail={'variant':key,'dir':None,'source':source}
        record('G-inc-base-build-'+key,ok,detail)
        retained[key]=entry
    sel['retained']=retained
    forced=[]
    for key,entry in retained.items():
        if not entry['receipts']:continue
        prev_units={u['name']:u for u in entry['receipts'][1]['selected_units']}
        pins=entry['pins'];stale={}
        for name,pu in prev_units.items():
            if name in sel['affected']:continue
            bad=[]
            if csha(pu['source'])!=pu['source_sha256']:bad.append({'kind':'source','path':pu['source']})
            if csha(pu['object'])!=pu['object_sha256']:bad.append({'kind':'object','path':pu['object']})
            if csha(pu['dependency'])!=pu['dependency_sha256']:
                # .d regenerated in place is tolerated for BASE gating, but in
                # incremental mode the unit is recompiled rather than trusted.
                bad.append({'kind':'dependency-log','path':pu['dependency']})
            else:
                try:
                    for p in deps(pu['dependency']):
                        exp=pins.get(str(p)) or pins.get(str(p.resolve()))
                        if exp is not None and csha(p)!=exp:bad.append({'kind':'input','path':str(p)})
                except OSError:bad.append({'kind':'dependency-log-unreadable','path':pu['dependency']})
            if bad:stale[name]=bad
        newly=sorted(n for n in stale if n not in sel['affected'])
        for n in newly:sel['affected'].append(n)
        forced.extend(newly)
        record('G-inc-retained-inputs-unchanged-'+key,not newly,
               {'variant':key,'retained_from':entry['dir'],'units_checked':len(prev_units)-len([n for n in prev_units if n in sel['affected']]),
                'stale_forced_fresh':newly,'stale_detail':{k:stale[k] for k in sorted(stale)[:20]},
                'note':'every unit slated for retention is re-verified against the previous build\'s own pins (source, object, dependency log, every pinned dep file); drifted units are forced back into the fresh set so a retained object can never be silently stale'})
    sel['forced_fresh']=sorted(set(forced))
    return retained

# ------------------------------------------------------- build stamp (agent FG)
def collect_change_ids():
    """CHANGE_IDS for the stamp: the CHANGES.json scope keys of every part
    that actually contributes file rows to this build's selection (parts with
    zero rows contribute nothing — their scope is not in this build)."""
    ids=[]
    for part in ['profile','typescan-r4','thread']:
        d=json.loads((HERE/part/'CHANGES.json').read_text())
        if d.get('files'):
            ids.extend(part+':'+k for k in d if k.startswith('scope'))
    return ids

def generate_build_stamp(sel,record,execute,steps):
    """Generate/delete candidate/include/GeneratedBuildStamp.hpp (agent FG,
    Claude hardening items b+c after the phantom build):
      --plan    : DELETE any stale stamp (an unstamped tree must compile to
                  the __has_include fallback value, never a leftover header
                  from an older build) and report the would-be stamp values.
      --execute : stamp BEFORE any unit compiles. BRIDGE_SNAPSHOT_ID comes
                  from env (snapshot_tree.sh prints SNAPSHOT-ID; the parent
                  wires snapshot -> build). Gate G-stamp-present then refuses
                  the run — before a single compile — if the file is missing,
                  unreadable, or carries the not-generated value (env unset
                  or not a 8..64 lowercase-hex snapshot id). Receipts record
                  the stamped header (VARIANT-MARKER / TWIN-RECEIPT /
                  SOURCE-SELECTION pin / PLAN)."""
    from datetime import datetime,timezone
    env_snapshot=(os.environ.get('BRIDGE_SNAPSHOT_ID') or '').strip()
    snapshot_ok=8<=len(env_snapshot)<=64 and all(c in '0123456789abcdef' for c in env_snapshot)
    snapshot_id=env_snapshot if snapshot_ok else STAMP_UNSET
    build_id='l26-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    change_ids=collect_change_ids()
    prior=None
    if STAMP_PATH.exists():
        prior={'path':str(STAMP_PATH),'sha256':sha(STAMP_PATH)}
    if not execute and prior is not None:
        STAMP_PATH.unlink()
        record('G-stamp-stale-cleanup',True,{**prior,'action':'stale GeneratedBuildStamp.hpp deleted at plan time',
            'note':'a build without stamp generation must compile to the __has_include fallback value; a stale stamp from an older build would misattribute provenance'})
    else:
        record('G-stamp-stale-cleanup',None,{'mode':'execute' if execute else 'plan','stale_stamp_present':prior is not None,
            'action':('overwritten by a fresh stamp before compiling' if execute else 'no stale stamp present (nothing to clean)')})
    body=('#pragma once\n'
          '// GENERATED FILE - DO NOT EDIT (build_variants.py stamp generator, agent FG 2026-10-09).\n'
          '// Deleted at --plan time and regenerated fresh before compiling on every --execute;\n'
          '// consumed by the [BUILD] first-log-line stamp (agent FF consumer) via\n'
          '// #if __has_include(<GeneratedBuildStamp.hpp>) -> the macros below. A tree built\n'
          '// without generation falls back to the not-generated value.\n'
          '#define RC_BUILD_ID "%s"\n'
          '#define RC_CHANGE_IDS "%s"\n'
          '#define RC_SNAPSHOT_ID "%s"\n'
          '#define BRIDGE_BUILD_ID "%s"\n'
          '#define BRIDGE_CHANGE_IDS "%s"\n'
          '#define BRIDGE_SNAPSHOT_ID "%s"\n')%(build_id,';'.join(change_ids),snapshot_id,
                                                 build_id,';'.join(change_ids),snapshot_id)
    if execute:
        if prior is not None:STAMP_PATH.unlink()
        STAMP_PATH.parent.mkdir(parents=True,exist_ok=True)
        STAMP_PATH.write_text(body)
        ok=STAMP_PATH.is_file() and STAMP_PATH.read_text()==body and STAMP_UNSET not in STAMP_PATH.read_text() \
           and all(('#define BRIDGE_%s "'%f) in STAMP_PATH.read_text() for f in ('BUILD_ID','CHANGE_IDS','SNAPSHOT_ID'))
        stamp={'path':str(STAMP_PATH),'sha256':sha(STAMP_PATH),'bytes':len(body),
               'build_id':build_id,'change_ids':change_ids,'snapshot_id':snapshot_id,
               'snapshot_env_set':bool(env_snapshot),'snapshot_env_valid':snapshot_ok,
               'generated_at_utc':datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
               'source':'build_variants.py generate_build_stamp (agent FG)','console_contact':False}
        record('G-stamp-present',ok,{**stamp,
            'note':'stamped before any unit compiles; refuses the execute when the stamp is missing or carries the not-generated value (env BRIDGE_SNAPSHOT_ID unset/invalid — run harvest/snapshot_tree.sh first and pass its SNAPSHOT-ID)'})
        if not ok:
            raise RuntimeError('G-stamp-present: refusing to build — GeneratedBuildStamp.hpp is missing/not-generated; run snapshot_tree.sh, export BRIDGE_SNAPSHOT_ID=<SNAPSHOT-ID>, then --execute')
    else:
        stamp={'path':str(STAMP_PATH),'planned':True,'build_id':'(generated at execute; l26-<UTC ts>)',
               'change_ids':change_ids,'change_id_count':len(change_ids),
               'snapshot_id':(env_snapshot if snapshot_ok else None),
               'snapshot_env_set':bool(env_snapshot),'snapshot_env_valid':snapshot_ok,
               'execute_refusal_expected':not snapshot_ok,
               'source':'build_variants.py generate_build_stamp (agent FG)','console_contact':False}
        record('G-stamp-present',None,{**stamp,
            'note':'at --execute the stamp is written before compiling; a missing/not-generated stamp (env BRIDGE_SNAPSHOT_ID unset) refuses the execute'})
        steps.append({'step':'generate-build-stamp','mode':'plan','variant':'both','path':str(STAMP_PATH),
                      'action':'stale cleanup only; header generated at execute (BRIDGE_BUILD_ID/BRIDGE_CHANGE_IDS/BRIDGE_SNAPSHOT_ID)',
                      'change_ids':change_ids,'snapshot_env':{'set':bool(env_snapshot),'valid':snapshot_ok},
                      'include_dir_injected':STAMP_INCLUDE_DIR})
    # make <GeneratedBuildStamp.hpp> resolvable from every unit (appended AFTER
    # the existing include dirs so no current resolution can change; G12 still
    # re-proves every replaced-header routing per compiled unit with it in place)
    if STAMP_INCLUDE_DIR not in sel['include_dirs']:
        sel['include_dirs'].append(STAMP_INCLUDE_DIR)
        stamp['include_dir_injected']=STAMP_INCLUDE_DIR
    sel['build_stamp']=stamp
    return stamp

def header_route_check(sel,argv):
    """Z13 E3 guard (resolution half): simulate clang's include lookup for
    every replaced-header row against a unit compile argv — -I directories in
    argv order, then the merged vfs overlay map — and prove every reachable
    spelling resolves to the l26 candidate, never a pre-l26 identity (e.g.
    the port-root original or the l25-runtime candidate dir still carried by
    inherited argvs).
    Spellings checked: the basename and the parent/basename suffix (the forms
    used by these trees, e.g. 'Unreal/UObjectArray.hpp'); deeper suffixes are
    not real include spellings and are skipped."""
    dirs=[];i=0
    while i<len(argv):
        a=argv[i]
        if a=='-I' and i+1<len(argv):dirs.append(argv[i+1]);i+=2;continue
        if a.startswith('-I') and len(a)>2:dirs.append(a[2:]);i+=1;continue
        i+=1
    alias=sel['merged_aliases'];results={};violations=[]
    for (name,digest,candidate) in sel['header_specs']:
        spellings={name}
        for ds in sel['unitdeps'].values():
            for d in ds:
                if d.name==name and len(d.parts)>=2:
                    spellings.add(str(Path(*d.parts[-2:])))
        for s in sorted(spellings):
            winner=None
            for dd in dirs:
                path=Path(dd)/s
                if not path.is_file():continue
                eff=alias.get(str(path)) or alias.get(str(path.resolve()))
                redirected=eff is not None
                eff=eff or str(path)
                if Path(eff).is_file():winner=(eff,dd,redirected);break
            if winner is None:continue
            eff,dd,redirected=winner
            ok=Path(eff).resolve()==Path(candidate).resolve()
            row={'spelling':s,'winner':eff,'winner_sha256':sha(eff),
                 'via':'-I '+dd+(' + vfs-overlay redirect' if redirected else ' (direct)'),
                 'replaced_original_sha256':digest,'is_l26_candidate':ok}
            results.setdefault(name,[]).append(row)
            if not ok:violations.append({'header':name,'spelling':s,'winner':eff,'winner_sha256':row['winner_sha256'],'expected_candidate':candidate,'expected_candidate_sha256':sha(candidate)})
    return results,violations

def effective_source(u,sel):
    return Path(sel['replacements'].get(str(Path(u['source']).resolve()),u['source']))

# ------------------------------------------------- copyin-probe (agent Q) wiring
def check_definition_order(text,symbols,label):
    """Return (evidence, violations) for define-before-use on one file's lines.
    A definition line either contains an explicit definition marker
    ('definition') or is a line starting with 'definition_prefix' that
    mentions the symbol (e.g. 'static uint32_t g_x = 0;'). The FIRST
    occurrence overall must be a definition, and it must precede every use."""
    lines=text.splitlines();evidence={};violations=[]
    for sym,spec in symbols.items():
        if 'definition' in spec:
            def_idx=[i for i,l in enumerate(lines) if spec['definition'] in l]
            mention_idx=[i for i,l in enumerate(lines) if spec['use'] in l or spec['definition'] in l]
        else:
            prefix=spec.get('definition_prefix','static ')
            def_idx=[i for i,l in enumerate(lines) if l.lstrip().startswith(prefix) and spec['use'] in l]
            mention_idx=[i for i,l in enumerate(lines) if spec['use'] in l]
        use_idx=[i for i in mention_idx if i not in def_idx]
        ev={'definition_line':(def_idx[0]+1 if def_idx else None),
            'first_use_line':(min(use_idx)+1 if use_idx else None),
            'last_use_line':(max(use_idx)+1 if use_idx else None)}
        evidence[sym]=ev
        bad=not def_idx or (use_idx and def_idx[0]>min(use_idx)) or (mention_idx and def_idx and mention_idx[0]!=def_idx[0])
        if bad:violations.append({'file':label,'symbol':sym,**ev})
    return evidence,violations

def apply_q_patch(base_bytes,patch_path,fname):
    """Deterministic clean-apply of a pure-addition unified diff. Fails on any
    rejected hunk or fuzz; offsets are allowed and recorded by patch itself."""
    with tempfile.TemporaryDirectory() as td:
        target=Path(td)/fname;target.write_bytes(base_bytes)
        r=subprocess.run(['/usr/bin/patch','--forward','--no-backup-if-mismatch','-i',str(patch_path),str(target)],capture_output=True,text=True)
        log=(r.stdout or '')+(r.stderr or '')
        if r.returncode!=0 or list(Path(td).rglob('*.rej')):
            raise RuntimeError('copyin-probe patch did not apply cleanly (rebase needed by agent Q or the coordinator):\n'+log[-2000:])
        if 'fuzz' in log.lower():
            raise RuntimeError('copyin-probe patch applied with fuzz; exact rebase required:\n'+log[-2000:])
        return target.read_bytes(),log

def compose_copyin_probe(record,steps,execute):
    """Integrate agent-Q's copyin-probe liveness overlay (WIRING.md) for BOTH
    variants. The UE4SS-side change cannot ride the one-hop typescan-r4
    replacement (that part owns the UnrealInitializer.cpp row), so this driver
    composes a second hop in its OWN staging directory: Q's pure-addition
    patch applied on top of the CURRENT typescan-r4 candidate (or Q's
    ready-made candidate when the base is still byte-identical). typescan-r4
    files are only read, never written."""
    man=json.loads((QPART/'MANIFEST.json').read_text())
    ui=man['base_files']['unrealinitializer'];bridge=man['base_files']['runtime_start_bridge']
    pins_to_check=(('unrealinitializer.patch',QPART/ui['patch'],ui['patch_sha256']),
                   ('runtime_start_bridge.patch',QPART/bridge['patch'],bridge['patch_sha256']),
                   ('unrealinitializer.candidate',Path(ui['candidate_path']),ui['candidate_sha256']),
                   ('runtime_start_bridge.candidate',Path(bridge['candidate_path']),bridge['candidate_sha256']),
                   ('unrealinitializer.base',Path(ui['base_path']),ui['base_sha256']),
                   ('runtime_start_bridge.base',Path(bridge['base_path']),bridge['base_sha256']))
    for name,p,expect in pins_to_check:
        matched=p.exists() and sha(p)==expect
        record('G2q-copyin-probe-pin-'+name,matched,{'path':str(p)})
        # Patches and ready candidates are Q's immutable deliverables: any
        # drift is fatal. The two BASE pins are expectations, not invariants:
        # the UE4SS base legitimately changes when the listener/phase-tag
        # agents revise UnrealInitializer.cpp (composition then takes the
        # patch-rebased path); the bridge base is checked again (as the l24
        # outer_bridge sha) by the packaging driver.
        if not matched and not name.endswith('.base'):
            raise RuntimeError('copyin-probe pin drift: '+name)
    current=Path(ui['base_path']);cur_sha=sha(current)
    composed=None;mode=None;patch_log=None;failure=None
    if cur_sha==ui['base_sha256']:
        composed=Path(ui['candidate_path']).read_bytes();mode='adopt-ready-candidate'
    else:
        try:
            composed,patch_log=apply_q_patch(current.read_bytes(),QPART/ui['patch'],'UnrealInitializer.cpp')
            mode='patch-rebased-onto-revised-base'
        except RuntimeError as error:
            if execute:raise
            failure=str(error);mode='patch-failed-rebase-needed'
    if composed is None:
        record('G2q-overlay-compose',False,{'mode':mode,'base':{'path':str(current),'sha256':cur_sha},'failure_tail':failure[-600:]})
        return {'manifest':man,'mode':mode,'composed_path':None,'composed_sha256':None,'pins':{'unrealinitializer':ui,'runtime_start_bridge':bridge},
                'receipt_section':{'part':'copyin-probe','author':'agent-Q (2026-10-08)',
                    'status':'PLAN-ONLY: patch no longer applies to the revised base; agent Q or the coordinator must rebase before --execute',
                    'base':{'path':ui['base_path'],'recorded_base_sha256':ui['base_sha256'],'actual_base_sha256':cur_sha},
                    'runtime_start_bridge':{'base':{'path':bridge['base_path'],'sha256':bridge['base_sha256']},
                        'patch':{'path':str(QPART/bridge['patch']),'sha256':bridge['patch_sha256']},
                        'ready_candidate':{'path':bridge['candidate_path'],'sha256':bridge['candidate_sha256']},
                        'wired_at':'packaging (l26_packaging_driver.py substitutes BUILD-INPUTS outer_bridge)'},
                    'console_contact':False}}
    composed_sha=hashlib.sha256(composed).hexdigest()
    composed_has_token=TOKEN.encode() in composed
    # order gates: the 2026-10-08 execute failure class (struct definition
    # landed after its use under base drift) must never pass silently.
    ue_ev,ue_viol=check_definition_order(composed.decode('utf-8',errors='replace'),UE4SS_ORDER_SYMBOLS,'composed UnrealInitializer.cpp (copyin-probe overlay)')
    bridge_text=Path(bridge['candidate_path']).read_text(errors='replace')
    br_ev,br_viol=check_definition_order(bridge_text,BRIDGE_ORDER_SYMBOLS,'runtime_start_bridge.cpp (agent-Q adopted candidate)')
    order_viol=ue_viol+br_viol
    record('G2q-order-def-before-use',not order_viol,{'composed':ue_ev,'bridge_candidate':br_ev,'violations':order_viol})
    if order_viol:raise RuntimeError('copyin-probe define-after-use order violation (rebase placed a definition after its use): '+repr(order_viol))
    if execute:
        QCOMPOSED.parent.mkdir(parents=True,exist_ok=True);QCOMPOSED.write_bytes(composed)
        if sha(QCOMPOSED)!=composed_sha:raise RuntimeError('composed overlay write drift')
    steps.append({'step':'apply-copyin-probe-overlay','applies_to_both_variants':True,'observation_only':True,
                  'mode':mode,'base':{'path':str(current),'sha256':cur_sha,'matches_q_recorded_base':cur_sha==ui['base_sha256']},
                  'composed':{'path':str(QCOMPOSED),'sha256':composed_sha,'bytes':len(composed),'persisted':execute},
                  'patch':{'path':str(QPART/ui['patch']),'sha256':ui['patch_sha256']},'patch_log_tail':(patch_log or '')[-400:] or None,
                  'order_evidence':{'composed':ue_ev,'bridge_candidate':br_ev},
                  'note':'second hop on unit unreal-unrealinitializer; staged outside typescan-r4 because that tree is owned by the code agents; quoted includes of the unit resolve through -I/vfs overlay, not the source directory (verified: no sibling headers in candidate/src)'})
    return {'manifest':man,'mode':mode,'composed_path':QCOMPOSED,'composed_sha256':composed_sha,'composed_has_token':composed_has_token,
            'order_evidence':{'composed':ue_ev,'bridge_candidate':br_ev},
            'staged_sources':({str(QCOMPOSED.resolve()):composed_sha} if composed_sha else {}),
            'pins':{'unrealinitializer':ui,'runtime_start_bridge':bridge},
            'receipt_section':{'part':'copyin-probe','author':'agent-Q (2026-10-08); patch re-anchored by agent-R same day (struct hunk moved to the stable pre-L13AdmissionOps landmark after the execute-order failure; content byte-identical, see MANIFEST reanchor_note); paths repointed to the l26 tree by agent CM (see MANIFEST l26_repoint_note)','wiring':'outputs/2026-10-08-l26-runtime/copyin-probe/WIRING.md',
                'unrealinitializer':{'base':{'path':ui['base_path'],'sha256':ui['base_sha256']},
                    'patch':{'path':str(QPART/ui['patch']),'sha256':ui['patch_sha256']},
                    'ready_candidate':{'path':ui['candidate_path'],'sha256':ui['candidate_sha256']},
                    'composed':{'path':str(QCOMPOSED),'sha256':composed_sha},'mode':mode,
                    'definition_order':ue_ev},
                'runtime_start_bridge':{'base':{'path':bridge['base_path'],'sha256':bridge['base_sha256']},
                    'patch':{'path':str(QPART/bridge['patch']),'sha256':bridge['patch_sha256']},
                    'ready_candidate':{'path':bridge['candidate_path'],'sha256':bridge['candidate_sha256']},
                    'definition_order':br_ev,
                    'wired_at':'packaging (l26_packaging_driver.py substitutes BUILD-INPUTS outer_bridge; adopted whole-file, never rebased)'},
                'import_delta':man.get('import_delta'),'console_contact':False}}

def token_targets(sel,extra_hit=None,token=TOKEN):
    """Files mentioning the given switch token and the translation units that
    must receive that variant define (parametrized l26: the B skip token for
    the diagnostic variant, the CTOKEN engine-listener switch for the
    production variant). extra_hit adds the composed copyin-probe overlay (its
    token presence is known from the composed BYTES, so plan mode works
    before the file is persisted). Hits on an overridden second-hop
    intermediate (the typescan-r4 candidate that the composed file replaces)
    are redirected to the composed path so they resolve to the unit that
    actually compiles them.
    Contract: the B guard lives only in UnrealInitializer.cpp (O's switch wraps
    BOTH registrations, lines ~544-579); the C switch lives in
    UObjectArray.cpp/.hpp and UnrealInitializer.cpp (Phase-6 admission).
    Defensive fallback: any token hit outside the BASE dependency universe
    that is not itself a replaced source is treated as a shared new header,
    and the define is then applied to every recompiled unit (superset; a macro
    only guards its own sites)."""
    redirect=sel.get('override_intermediates',{})
    raw=set()
    for p in sorted(HERE.rglob('*')):
        if not p.is_file() or p.suffix not in SCAN_SUFFIXES:continue
        if any(part in SCAN_SKIP_PARTS for part in p.relative_to(HERE).parts):continue
        try:data=p.read_bytes()
        except OSError:continue
        if token.encode() in data:raw.add(str(p.resolve()))
    if extra_hit:raw.add(str(Path(extra_hit).resolve()))
    hits={redirect.get(h,h) for h in raw}
    token_units={u['name'] for u in sel['base']['selected_units']
                 if str(effective_source(u,sel).resolve()) in hits
                 or any(str(d.resolve()) in hits for d in sel['unitdeps'][u['name']])}
    universe_resolved={str(Path(d).resolve()) for ds in sel['unitdeps'].values() for d in ds}
    replaced_sources={str(Path(r).resolve()) for r in sel['replacements'].values()}
    shared={h for h in hits if h not in universe_resolved and h not in replaced_sources}
    conservative=bool(shared)
    define_units=set(sel['affected']) if conservative else token_units
    return {'token_files':sorted(hits),'token_units':sorted(token_units),'shared_new_headers':sorted(shared),
            'conservative':conservative,'define_units':sorted(define_units)}

# ------------------------------------------------ concurrent twins (agent FY)
# [FY 2026-10-10] The variant loop and the pass loop were sequential only by
# accident of code structure. pass2 has NO data dependency on pass1: both
# read the frozen l25-A BASE pass receipts and write only to their own
# out/passN/objects dir; the variant overlay is written once (prepare_variant)
# before either pass; variant B never reads variant A's outputs. Execute now
# runs all four (variant,pass) tasks concurrently in one pool of 4. Gate and
# receipt CONTENT is unchanged; only two things differ from the sequential
# driver:
#   * stdout gate lines interleave (each line atomic via _PRINT_LOCK) and the
#     EXECUTION-RECEIPT gates list is a deterministic merge (A-pass1, A-pass2,
#     B-pass1, B-pass2, then the serial finisher/pair gates);
#   * per-pass DEPENDENCY-GRAPH.json / COMPOSITION-RECEIPT.json are written by
#     the serial finisher after both passes join, because pass2's dependency
#     graph is cumulative over pass1's pins (historical content preserved:
#     pass1 graph = pass1 pins; pass2 graph = {**pass1, **pass2}).
# Global clang/lld subprocess concurrency is capped by env L26_BUILD_JOBS
# (see resolve_build_jobs). --plan stays fully sequential and byte-identical
# to the pre-FY driver; --incremental composes with the concurrent path.
_PRINT_LOCK=threading.Lock()

def resolve_build_jobs():
    """Global subprocess budget for --execute (agent FY). L26_BUILD_JOBS env
    (integer >=1) overrides. Default min(6, ncpu): this 16 GB ARM Mac has 8
    logical cores (4 performance + 4 efficiency); 6 concurrent clang/lld
    processes leave headroom for the OS and for the memory-heavy units (the
    luamod object alone is ~7 MB at -O0, so its compile can peak well above
    the average unit)."""
    raw=(os.environ.get('L26_BUILD_JOBS') or '').strip()
    if raw:
        try:n=int(raw)
        except ValueError:raise RuntimeError('L26_BUILD_JOBS must be an integer >=1, got: '+raw)
        if n<1:raise RuntimeError('L26_BUILD_JOBS must be >=1, got: '+raw)
        return n,'env L26_BUILD_JOBS='+raw
    ncpu=os.cpu_count() or 2
    return max(2,min(6,ncpu)),'default min(6,ncpu) with ncpu='+str(ncpu)

def make_task_recorder():
    """Per-(variant,pass) gate collector for the concurrent execute path
    (agent FY): entries are buffered in task order and merged into the main
    gates list deterministically after the join; the stdout line is emitted
    immediately under the print lock so concurrent gate lines never tear."""
    tgates=[]
    def trec(name,ok,detail):
        tgates.append({'gate':name,'ok':(bool(ok) if ok is not None else None),'detail':detail})
        with _PRINT_LOCK:
            print(json.dumps({'gate':name,'ok':(bool(ok) if ok is not None else 'PLANNED'),'detail':detail}),flush=True)
    return tgates,trec

# ---------------------------------------------------------------- building
def prepare_variant(variant,sel,targets,record,execute,steps):
    """Serial per-variant prologue (agent FY split of build_variant): the
    dir-fresh gate, the variant dir + header overlay, the define set, the
    subprocess env and the marker. Runs BEFORE either pass of this variant
    (concurrent execute: serially for A then B up front; plan: once per
    variant in the original order)."""
    out=variant['dir'];overlay=out/'headers.vfs.json'
    overlay_roots=[{'type':'file','name':k,'external-contents':v} for k,v in sorted(sel['merged_aliases'].items())]
    overlay_hash=hashlib.sha256((json.dumps({'version':0,'roots':overlay_roots},indent=2)+'\n').encode()).hexdigest()
    if execute:
        record('G3-variant-dir-fresh',not out.exists(),{'path':str(out)})
        out.mkdir(parents=True,exist_ok=False)
        write(overlay,{'version':0,'roots':overlay_roots})
    else:
        steps.append({'step':'write-header-overlay','variant':variant['key'],'path':str(overlay),'planned_sha256':overlay_hash,'roots':len(overlay_roots)})
    define_units=set(targets['define_units']) if variant['define'] else set()
    env=dict(os.environ,LLVM_CONFIG=str(LLVM/'llvm-config'),CCACHE_DISABLE='1',PYTHONDONTWRITEBYTECODE='1')
    marker={'variant':variant['key'],'label':variant['label'],'define':variant['define'] or '(none)',
            'token':variant['token'],'delete_listener_registration':variant['registration'],
            'unsafe':variant['unsafe'],'safety':UNSAFE_NOTE if variant['unsafe'] else 'normal production configuration',
            'console_contact':False,'native_execution':False,
            'token_units':targets['token_units'],'define_units':sorted(define_units),
            'conservative_define_scope':targets['conservative'],
            'incremental_mode':bool(sel.get('incremental')),
            'retained_from':({k:v['dir'] for k,v in (sel.get('retained') or {}).items()} if sel.get('retained') else None),
            'copyin_probe_wiring':sel['copyin_probe']['receipt_section'],
            'build_stamp':sel.get('build_stamp')}
    return {'variant':variant,'out':out,'overlay':overlay,'overlay_hash':overlay_hash,
            'define_units':define_units,'env':env,'marker':marker}

def build_pass(variant,ctx,passno,sel,record,execute,jobs_sem=None,steps=None):
    """One (variant,pass) build task (agent FY split of build_variant).
    Reads only frozen inputs (the l25-A BASE pass receipts, sel, previous
    incremental receipts) and writes only to its own out/passN dir, so the
    four A1/A2/B1/B2 tasks are independent and run concurrently in execute
    mode. DEPENDENCY-GRAPH/COMPOSITION receipts are written by
    finish_variant (pass2's dependency graph is cumulative over pass1's
    pins). jobs_sem bounds global clang/lld subprocess concurrency."""
    out=ctx['out'];overlay=ctx['overlay'];define_units=ctx['define_units'];env=ctx['env']
    def budgeted_run(cmd,out_dir,label):
        # [agent FY] global compiler/linker subprocess budget: with up to four
        # passes compiling concurrently (each with its own pool of 2), this
        # semaphore caps TOTAL external clang/lld concurrency at
        # L26_BUILD_JOBS instead of 8 unbounded.
        if jobs_sem is None:return run(cmd,out_dir,label,env)
        with jobs_sem:return run(cmd,out_dir,label,env)
    parent=json.loads((BASE/f'pass{passno}/COMPOSITION-RECEIPT.json').read_text())
    units=copy.deepcopy(parent['selected_units'])
    pdir=out/f'pass{passno}';objects=pdir/'objects'
    # NEW_UNITS append AFTER the BASE list: the link remap below zips
    # parent[i]↔units[i], so appending at the end keeps every BASE
    # pairing intact; the new objects are injected into the link argv
    # explicitly (they have no BASE link-command entries).
    units.extend(make_new_units(units,passno,sel,objects))
    if execute:pdir.mkdir();objects.mkdir()
    # agent EZ incremental retention: units OUTSIDE the fresh set adopt
    # the previous same-variant build's compiled provenance (object,
    # source, dependency log, argv) so retained objects byte-match the
    # previous build's receipts and the link consumes those exact files.
    prev=(sel.get('retained') or {}).get(variant['key'])
    if prev and prev['receipts'].get(passno):
        prev_units={u['name']:u for u in prev['receipts'][passno]['selected_units']}
        want=[u for u in units if u['name'] not in sel['affected'] and u['name'] in prev_units]
        for u in want:
            pu=prev_units[u['name']]
            u.update(source=pu['source'],source_sha256=pu['source_sha256'],object=pu['object'],object_sha256=pu['object_sha256'],
                     dependency=pu['dependency'],dependency_sha256=pu['dependency_sha256'],argv=pu['argv'],
                     selection=(pu.get('selection') or u.get('selection') or '')+' | l26-incremental retained from '+str(prev['dir']))
        record('G-inc-retain-objects-from-previous-build',len(want)==len([u for u in units if u['name'] not in sel['affected']]),
               {'variant':variant['key'],'pass':passno,'retained_from':prev['dir'],'remapped_units':len(want),
                'not_in_previous_receipt':sorted(u['name'] for u in units if u['name'] not in sel['affected'] and u['name'] not in prev_units),
                'note':'retained units keep the previous same-variant build\'s pinned objects (verified live by the compile_unit asserts); fresh units compile into this variant dir as usual'})
    def compile_unit(u):
        assert sha(u['source'])==u['source_sha256'];assert sha(u['object'])==u['object_sha256']
        if u['name'] not in sel['affected']:return u,None
        new=copy.deepcopy(u)
        source=effective_source(u,sel)
        obj=objects/(u['name']+'.o');dep=objects/(u['name']+'.d')
        argv=rewrite_compile(u['argv'],u['source'],source,obj,dep)
        argv=[a for i,a in enumerate(argv) if a!='-ivfsoverlay' and not(i and argv[i-1]=='-ivfsoverlay')]
        argv[1:1]=['-ivfsoverlay',str(overlay),*[a for p in sel['include_dirs'] for a in ['-I',p]],'-I',str(Path(u['source']).parent)]
        if u['name'] in define_units:argv[1:1]=[variant['define']]
        if u['name'] in sel.get('lua_cpp_units',()):argv=lua_cpp_argv(argv)
        routes,route_violations=header_route_check(sel,argv)
        route_agg['units'].append(u['name'])
        if route_violations:
            route_agg['violations'].append({'variant':variant['key'],'pass':passno,'unit':u['name'],'violations':route_violations})
            if execute:raise RuntimeError('header routing violation (Z13 E3 class) in '+u['name']+': '+repr(route_violations))
        for k,v in routes.items():route_agg['representative'].setdefault(k,v)
        if execute:
            budgeted_run(argv,objects,u['name'])
            ds=deps(dep)
            oldset={str(p.resolve()) for p in sel['unitdeps'][u['name']]}
            # Attributable whitelist: the ONLY non-baseline paths allowed
            # here are driver-generated staging sources this run composed
            # AND hash-pinned (retry-2 postmortem: the composed overlay is
            # the driver's own file, not source drift). Any other new
            # include path still fails the unit.
            staged=sel.get('driver_staged_sources') or {}
            extra=[str(p) for p in ds if str(p.resolve()) not in oldset
                   and not p.resolve().is_relative_to(HERE)
                   and (p.name,sha(p)) not in sel['approved_header_identities']
                   and staged.get(str(p.resolve()))!=sha(p)]
            if u['name'] in sel.get('lua_cpp_units',()):
                # [LUA-AS-CPP v2] approve ONLY rows identical to a BASE-pinned dependency; record each one.
                lua_exempt=[x for x in extra if sel['base_dep_pins'].get(str(Path(x).resolve()))==sha(x)]
                extra=[x for x in extra if x not in lua_exempt]
                new['lua_cpp_base_pinned_rows']=[{'path':x,'sha256':sha(x)} for x in lua_exempt]
            if extra:raise RuntimeError('unapproved include graph additions '+u['name']+': '+repr(extra))
        else:
            ds=None
            src_sha=sha(source) if source.exists() else sel['copyin_probe']['composed_sha256']
            steps.append({'step':'compile-unit','variant':variant['key'],'pass':passno,'unit':u['name'],
                          'source':{'path':str(source),'sha256':src_sha},
                          'object_planned':str(obj),
                          'define_injected':u['name'] in define_units,
                          'argv':argv})
        new.update(source=str(source),source_sha256=sha(source) if (execute or source.exists()) else src_sha,object=str(obj),
                   object_sha256=sha(obj) if execute else 'PENDING-BUILD',
                   dependency=str(dep),dependency_sha256=sha(dep) if execute else 'PENDING-BUILD',
                   argv=argv,selection='l26_variant_'+variant['key']+'_fresh_selected_source_or_header_dependent',
                   LLVM_CONFIG=str(LLVM/'llvm-config'))
        if u['name'] in define_units:new['variant_define']=variant['define']
        return new,ds
    # [agent FY] pass_pins is PASS-LOCAL now (was variant-wide accumulated
    # across the sequential pass loop); finish_variant merges p1+p2 to
    # reproduce the historical cumulative pass2 dependency graph.
    done={};route_agg={'units':[],'violations':[],'representative':{}};pass_pins={}
    if execute:
        count=0
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures=[pool.submit(compile_unit,u) for u in units]
            for f in concurrent.futures.as_completed(futures):
                try:u,ds=f.result()
                except Exception:
                    for pending in futures:pending.cancel()
                    raise
                done[u['name']]=u
                if ds is not None:
                    count+=1
                    for p in ds:pass_pins[str(p.resolve())]=pin(p)
                    if count%25==0:
                        with _PRINT_LOCK:print(json.dumps({'variant':variant['key'],'pass':passno,'compiled':count,'total':len(sel['affected'])}),flush=True)
    else:
        for u in units:
            u,ds=compile_unit(u);done[u['name']]=u
    units=[done[u['name']] for u in units]
    if LUA_AS_CPP and execute:
        lua_rows={u['name']:len(u.get('lua_cpp_base_pinned_rows',[])) for u in units if u['name'] in sel['lua_cpp_units']}
        ok=len(lua_rows)==LUA_CPP_EXPECTED_LUARAW_UNITS and sum(lua_rows.values())>0
        record('G-LUACPP-include-exemption',ok,{'variant':variant['key'],'pass':passno,'units_checked':len(lua_rows),
               'approved_rows_total':sum(lua_rows.values()),'approved_rows_per_unit':lua_rows,
               'rule':'new dependency row approved only if same resolved path + same sha256 as a BASE-pinned dependency; rows listed per unit in COMPOSITION-RECEIPT selected_units[].lua_cpp_base_pinned_rows'})
        if not ok:raise RuntimeError('G-LUACPP-include-exemption: zero rows or not 33 Lua units checked')
    record('G12-header-routing-resolves-to-l26-candidates',not route_agg['violations'],
           {'variant':variant['key'],'pass':passno,'units_checked':len(route_agg['units']),
            'representative_routes':route_agg['representative'],'violations':route_agg['violations']})
    if route_agg['violations'] and execute:raise RuntimeError('header routing violations: '+repr(route_agg['violations'][:3]))
    runtime=pdir/variant['artifact']
    if execute:
        expected_fresh=[u for u in O_UNITS if u in sel['affected']]
        not_fresh=[u['name'] for u in units if u['name'] in expected_fresh and not str(u['object']).startswith(str(objects)+os.sep)]
        record('G11-o-rows-recompiled-fresh',not not_fresh,{'variant':variant['key'],'pass':passno,'expected_fresh':expected_fresh,'not_fresh':not_fresh})
        if not_fresh:raise RuntimeError('listener units kept inherited objects (row no-op): '+repr(not_fresh))
    if execute:
        link=json.loads(Path(parent['link_command']['path']).read_text())
        old_to_new={a['object']:b['object'] for a,b in zip(parent['selected_units'],units)}
        cmd=[old_to_new.get(a,a) for a in link['argv']];cmd[cmd.index('-o')+1]=str(runtime)
        # NEW_UNITS have no BASE link-command entries: inject their
        # objects just before the linker -o so they actually link (the
        # lfb phantom class, prevented at the link layer).
        cmd[cmd.index('-o'):0]=[str(u['object']) for u in units if u['name'] in NEW_UNITS]
        cmd=['-Map='+str(pdir/'runtime.map') if a.startswith('-Map=') else a for a in cmd]
        input_paths=[a for i,a in enumerate(cmd) if a.endswith(('.o','.a')) and i!=cmd.index('-o')+1]
        _expected_inputs=391+len(NEW_UNITS)
        record('G6-link-input-census',len(input_paths)==len(set(input_paths))==_expected_inputs and all(input_paths.count(u['object'])==1 for u in units),{'variant':variant['key'],'pass':passno,'inputs':len(input_paths),'expected':_expected_inputs})
        for p in link['fixed_inputs']:assert sha(p['path'])==p['sha256']
        budgeted_run(cmd,pdir,'link')
        after=budgeted_run([LLVM/'llvm-nm','-u',runtime],pdir,'undefined')
        before=(BASE/f'pass{passno}/undefined.stdout').read_text()
        symbols=lambda s:{tuple(line.split()[-2:]) for line in s.splitlines() if line.split()}
        if LUA_AS_CPP:
            expected=(symbols(before)-set(LUA_CPP_IMPORT_DELTA['removed']))|set(LUA_CPP_IMPORT_DELTA['added'])
            ok=symbols(after)==expected and len(symbols(after))>0 and len(symbols(before))>0
            record('G7-import-delta-declared-lua-as-cpp',ok,{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout'),
                   'declared':{k:[list(t) for t in v] for k,v in LUA_CPP_IMPORT_DELTA.items()},
                   'actual_added':sorted([list(t) for t in symbols(after)-symbols(before)]),'actual_removed':sorted([list(t) for t in symbols(before)-symbols(after)]),
                   'unexpected_added':sorted([list(t) for t in symbols(after)-expected]),'unexpected_missing':sorted([list(t) for t in expected-symbols(after)]),
                   'counts':(len(symbols(before)),len(symbols(after)))})
            if not ok:raise RuntimeError('Lua-as-C++ raw import delta differs from the declared delta')
        else:
            record('G7-import-delta-none',symbols(after)==symbols(before),{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout')})
            if symbols(after)!=symbols(before):raise RuntimeError('raw import delta must be declared before packaging')
        write(pdir/'LINK-COMMAND.json',{'argv':cmd,'ordered_inputs':input_paths,'fixed_inputs':link['fixed_inputs']})
        write(pdir/'COMPILE-RECEIPT.json',{'fresh_units':[{'name':u['name'],'command':pin(objects/(u['name']+'.command.json')),'object':pin(u['object'])} for u in units if u['name'] in sel['affected']],'retained_units':len(units)-len(sel['affected']),'header_overlay':pin(overlay)})
        # [agent FY] DEPENDENCY-GRAPH.json + COMPOSITION-RECEIPT.json moved to
        # finish_variant: pass2's dependency graph is cumulative over pass1's
        # pins, so both files are written by the serial finisher after the
        # passes join (content identical to the sequential driver's writes;
        # COMPOSITION pins the DEPENDENCY-GRAPH, so the graph writes first).
        write(pdir/'IMPORT-DELTA.json',{'baseline':sorted([list(t) for t in symbols(before)]),'current':sorted([list(t) for t in symbols(after)]),'added':sorted([list(t) for t in symbols(after)-symbols(before)]),'removed':sorted([list(t) for t in symbols(before)-symbols(after)]),'baseline_artifact':pin(BASE/f'pass{passno}/undefined.stdout'),'qualification':'Raw runtime symbols; final PRX import identities checked during packaging.'})
        with _PRINT_LOCK:print(json.dumps({'variant':variant['key'],'pass':passno,'runtime':pin(runtime),'imports_changed':symbols(after)!=symbols(before)}),flush=True)
        return {'passno':passno,'pdir':pdir,'units':units,'pins':pass_pins,'runtime_pin':pin(runtime),
                'route_summary':route_agg['representative'],'selected_count':len(units),
                'undefined_counts':(len(symbols(before)),len(symbols(after))),
                'undefined_delta':(sorted([list(t) for t in symbols(after)-symbols(before)]),sorted([list(t) for t in symbols(before)-symbols(after)]))}
    else:
        link_argv=json.loads(Path(parent['link_command']['path']).read_text())['argv']
        record('G6-link-input-census',None,{'variant':variant['key'],'pass':passno,'planned_inputs':391+len(NEW_UNITS),'note':'387 retained pinned objects + fresh objects + %d NEW_UNITS; exact census enforced at execute'%len(NEW_UNITS)})
        record('G7-import-delta-declared-lua-as-cpp' if LUA_AS_CPP else 'G7-import-delta-none',None,{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout'),'baseline_sha256':sha(BASE/f'pass{passno}/undefined.stdout'),
               'note':('llvm-nm -u must equal BASE minus exactly '+repr(LUA_CPP_IMPORT_DELTA['removed'])+' (enforced at execute)') if LUA_AS_CPP else 'llvm-nm -u equality with BASE enforced at execute'})
        steps.append({'step':'link-runtime','variant':variant['key'],'pass':passno,'output':str(runtime),'link_command_source':parent['link_command']['path'],'argv_shape':link_argv[:3]+['...','-o',str(runtime)]})
        steps.append({'step':'llvm-nm-undefined-gate','variant':variant['key'],'pass':passno,'object':str(runtime),'baseline':{'path':str(BASE/f'pass{passno}/undefined.stdout'),'sha256':sha(BASE/f'pass{passno}/undefined.stdout')}})
        return {'passno':passno,'pdir':pdir,'units':units,'pins':pass_pins,
                'runtime_pin':{'path':str(runtime),'sha256':'PENDING-BUILD'},
                'composition_pin':{'path':str(pdir/'COMPOSITION-RECEIPT.json'),'sha256':'PENDING-BUILD'},
                'route_summary':route_agg['representative'],'selected_count':len(units)}

def finish_variant(variant,ctx,p1,p2,sel,record,execute,steps):
    """Serial per-variant epilogue (agent FY split of build_variant): writes
    the per-pass DEPENDENCY-GRAPH/COMPOSITION receipts (pass1 graph = pass1
    pins; pass2 graph cumulative over pass1 — the sequential driver's exact
    content), the G8 twin-identity gate and the variant-level receipts.
    Runs after both passes of this variant have joined."""
    out=ctx['out'];overlay=ctx['overlay'];marker=ctx['marker']
    route_summaries=[p1['route_summary'],p2['route_summary']]
    results=[]
    if execute:
        merged_pins={**p1['pins'],**p2['pins']}
        for pp,pins_for_graph in ((p1,p1['pins']),(p2,merged_pins)):
            pdir=pp['pdir']
            write(pdir/'DEPENDENCY-GRAPH.json',{'selected_dependencies':[{'name':u['name'],'dependency':pin(u['dependency'])} for u in pp['units']],'new_effective_dependency_pins':list(pins_for_graph.values()),'driver_staged_source_pins':[pin(Path(k)) for k in sorted(sel['driver_staged_sources'])],'header_aliases':sel['aliases'],'added_headers':[pin(p) for p in sel['added']]})
            receipt={'status':variant['status'],'variant':marker,'pass':pp['passno'],'object':pin(pdir/variant['artifact']),'selected_units':pp['units'],'selected_count':pp['selected_count'],'full_link_inputs':391+len(NEW_UNITS),
                     'parent_composition':pin(BASE/f'pass{pp["passno"]}/COMPOSITION-RECEIPT.json'),'fresh_current_pass':len(sel['affected']),'retained_current_units':pp['selected_count']-len(sel['affected']),
                     'scope':'l26 variant '+variant['key']+' accepted local code on the l25-A BASE; '+variant['label']+'; no console proof',
                     'link_command':pin(pdir/'LINK-COMMAND.json'),'header_overlay':pin(overlay),'compile_receipt':pin(pdir/'COMPILE-RECEIPT.json'),'dependency_graph':pin(pdir/'DEPENDENCY-GRAPH.json'),'import_delta':pin(pdir/'IMPORT-DELTA.json'),
                     'baseline_undefined_count':pp['undefined_counts'][0],'candidate_undefined_count':pp['undefined_counts'][1],'actual_undefined_added':pp['undefined_delta'][0],'actual_undefined_removed':pp['undefined_delta'][1],
                     'native_execution':False,'menu_success':False,'console_contact':False}
            write(pdir/'COMPOSITION-RECEIPT.json',receipt)
            results.append({'runtime':pp['runtime_pin'],'composition':pin(pdir/'COMPOSITION-RECEIPT.json')})
    else:
        results=[{'runtime':p1['runtime_pin'],'composition':p1['composition_pin']},
                 {'runtime':p2['runtime_pin'],'composition':p2['composition_pin']}]
    if execute:
        a=Path(results[0]['runtime']['path']).read_bytes();b=Path(results[1]['runtime']['path']).read_bytes()
        record('G8-twin-identity-within-variant',a==b,{'variant':variant['key'],'pass1':results[0]['runtime']['sha256'],'pass2':results[1]['runtime']['sha256']})
        if a!=b:raise RuntimeError('variant '+variant['key']+' runtime twins differ')
        ra=json.loads(Path(results[0]['composition']['path']).read_text());rb=json.loads(Path(results[1]['composition']['path']).read_text())
        for x,y in zip(ra['selected_units'],rb['selected_units']):assert x['object_sha256']==y['object_sha256'],x['name']
        actual_pins={}
        for u in ra['selected_units']:
            if u['name'] in sel['affected']:
                for p in deps(u['dependency']):actual_pins[str(p.resolve())]=pin(p)
        oldsel=sel['oldsel']
        selection=copy.deepcopy(oldsel)
        selection['source_pins']+=list(actual_pins.values())+[pin(x['candidate']) for x in sel['changes']]+[pin(p) for p in sel['added']]+[pin(overlay)]+[pin(sel['copyin_probe']['composed_path']),pin(QPART/'MANIFEST.json'),pin(STAMP_PATH)]
        selection['receipt_pins']+=[x['composition'] for x in results]
        selection['artifact_pins']+=[x['runtime'] for x in results]
        selection.update(parent_selection=pin(BASE/'SOURCE-SELECTION.json'),native_execution=False,selected_changes=sel['changes'],l26_variant=marker)
        write(out/'ACTUAL-DEPENDENCIES.json',list(actual_pins.values()));write(out/'SOURCE-SELECTION.json',selection)
        write(out/'CHANGE-SELECTION.json',{'files':sel['changes'],'added_headers':sel['added'],'aliases':sel['aliases'],'affected_units':sel['affected'],'retained_units':len(sel['base']['selected_units'])-len(sel['affected']),'no_console_contact':True,'l26_variant':marker,'incremental':(None if not sel.get('incremental') else {'fresh_units':len(sel['affected']),'fresh_units_full_mode':len(sel['affected_full']),'retained_from':{k:v['dir'] for k,v in sel['retained'].items()},'forced_fresh_units':sorted(sel.get('forced_fresh') or []),'header_rows_skipped':sel['header_skip_rows'],'upload_grade':'NOT upload-grade; full build (the default, without --incremental) is the only upload-grade mode'}),'copyin_probe_wiring':sel['copyin_probe']['receipt_section'],'second_hop_source_overrides':{'unreal-unrealinitializer':str(sel['copyin_probe']['composed_path'])},'driver_staged_sources':{k:v for k,v in sel['driver_staged_sources'].items()},'build_stamp':sel.get('build_stamp')})
        write(out/'VARIANT-MARKER.json',marker)
        write(out/'TWIN-RECEIPT.json',{'status':variant['twin_status'],'variant':marker,'passes':results,'source_selection':pin(out/'SOURCE-SELECTION.json'),
            'fresh_units_each_pass':len(sel['affected']),'selected_units':len(sel['base']['selected_units']),'full_link_inputs':391+len(NEW_UNITS),'new_imports':[],'console_contact':False,'native_execution':False,
            'copyin_probe_wiring':sel['copyin_probe']['receipt_section']})
    else:
        steps.append({'step':'write-variant-receipts','variant':variant['key'],'dir':str(out),'files':['TWIN-RECEIPT.json','SOURCE-SELECTION.json','CHANGE-SELECTION.json','ACTUAL-DEPENDENCIES.json','VARIANT-MARKER.json'],'passes':results})
    return {'marker':marker,'results':results,'routes':route_summaries}

def build_variant(variant,sel,targets,record,execute,steps):
    """Sequential build path (agent FY: thin wrapper over prepare_variant /
    build_pass / finish_variant). Plan mode output is byte-identical to the
    pre-FY driver — prepare, pass1, pass2, finish, per variant in order. The
    concurrent execute path lives in main() and calls the three stages
    directly through a shared pass pool."""
    ctx=prepare_variant(variant,sel,targets,record,execute,steps)
    p1=build_pass(variant,ctx,1,sel,record,execute,None,steps)
    p2=build_pass(variant,ctx,2,sel,record,execute,None,steps)
    return finish_variant(variant,ctx,p1,p2,sel,record,execute,steps)

def pair_receipt(built,targets,record,execute,steps,root,sel=None):
    # [L26][P2] the A/B pair receipt exists ONLY when both variants were
    # built (variant B is env-gated dead by default). A default A-only run
    # records the pair gates as planned-skip instead of failing on a missing
    # sibling, and writes no pair receipt file.
    if 'A' not in built or 'B' not in built:
        record('G9-variants-differ',None,{'note':'pair receipt skipped: variant B not in the active build set (agent-FX P2 drop); G9/G10 enforced only on A+B runs'})
        record('G10-object-deltas-confined-to-define-units',None,{'note':'pair receipt skipped: variant B not in the active build set (agent-FX P2 drop)'})
        steps.append({'step':'write-variant-pair-receipt','skipped':True,
                      'note':'variant B absent; VARIANT-PAIR-RECEIPT.json not written this run'})
        return None
    a,b=built['A'],built['B']
    receipt={'status':'MAC_ONLY_L26_VARIANT_PAIR_A_VS_B_RECEIPT','token':TOKEN,'ctoken':CTOKEN,
             'contract_call_site':'typescan-r4/candidate/src/UnrealInitializer.cpp wrapped by the listener agent in #if !defined('+TOKEN+')',
             'contract_callee':'typescan-r4/candidate/src/UObjectArray.cpp engine-call path under #if defined('+CTOKEN+'); production variant A defines it (engine-side registration, admission fail-closed), variant B does not define it (B skip wins at the call sites: registration never called)',
             'copyin_probe_wiring_in_both_variants':True,
             'variant_A':{'label':a['marker']['label'],'registration':a['marker']['delete_listener_registration'],'runtime':a['results'][0]['runtime']},
             'variant_B':{'label':b['marker']['label'],'registration':b['marker']['delete_listener_registration'],'unsafe':True,'safety':UNSAFE_NOTE,'runtime':b['results'][0]['runtime']},
             'token_files':targets['token_files'],'token_units':targets['token_units'],'conservative_define_scope':targets['conservative'],
             'console_contact':False,'native_execution':False}
    if execute:
        ha=Path(a['results'][0]['runtime']['path']).read_bytes();hb=Path(b['results'][0]['runtime']['path']).read_bytes()
        record('G9-variants-differ',ha!=hb,{'A':a['results'][0]['runtime']['sha256'],'B':b['results'][0]['runtime']['sha256']})
        if ha==hb:raise RuntimeError('variants A and B are identical: the skip-define had no effect (token not found or guard not landed)')
        ra=json.loads(Path(a['results'][0]['composition']['path']).read_text());rb=json.loads(Path(b['results'][0]['composition']['path']).read_text())
        deltas=[x['name'] for x,y in zip(ra['selected_units'],rb['selected_units']) if x['object_sha256']!=y['object_sha256']]
        allowed=set(targets['define_units'])
        detail={'deltas':deltas,'allowed':sorted(allowed)}
        if sel is not None and sel.get('incremental'):
            # agent EZ incremental: retained objects come from the two
            # previous SAME-VARIANT builds, whose define-bearing units
            # legitimately differ (that divergence is the previous pair's own
            # proven G10 delta set). A cross-variant delta is accepted iff it
            # is a current define unit OR byte-matches each variant's previous
            # receipt (inherited divergence); any other delta still fails.
            pa=(sel.get('retained') or {}).get('A');pb=(sel.get('retained') or {}).get('B')
            pua={u['name']:u for u in pa['receipts'][1]['selected_units']} if pa and pa.get('receipts') else {}
            pub={u['name']:u for u in pb['receipts'][1]['selected_units']} if pb and pb.get('receipts') else {}
            inherited=[]
            for x,y in zip(ra['selected_units'],rb['selected_units']):
                if x['name'] in allowed or x['object_sha256']==y['object_sha256']:continue
                qa=pua.get(x['name']);qb=pub.get(y['name'])
                if qa and qb and x['object_sha256']==qa['object_sha256'] and y['object_sha256']==qb['object_sha256']:inherited.append(x['name'])
            allowed|=set(inherited)
            detail['incremental_inherited_deltas']=inherited
            detail['note']='incremental: deltas may also be units retained from the two previous same-variant builds (byte-matched against their receipts = inherited divergence); any OTHER delta still fails'
        record('G10-object-deltas-confined-to-define-units',set(deltas)<=allowed,detail)
        if not set(deltas)<=allowed:raise RuntimeError('unexpected cross-variant object deltas: '+repr(sorted(set(deltas)-allowed)))
        receipt['per_unit_object_deltas']=deltas
    else:
        record('G9-variants-differ',None,{'note':'enforced at execute; fails if the skip-define has no effect'})
        record('G10-object-deltas-confined-to-define-units',None,{'allowed':targets['define_units'],'note':('incremental: at execute, cross-variant deltas may also be units retained from the two previous same-variant builds, accepted iff byte-identical to each previous receipt; otherwise deltas must be a subset of the define units') if (sel is not None and sel.get('incremental')) else 'deltas must be a subset of the define units'})
    write(root/'VARIANT-PAIR-RECEIPT.json',receipt)
    steps.append({'step':'write-variant-pair-receipt','path':str(root/'VARIANT-PAIR-RECEIPT.json')})
    return receipt


def acquire_build_lock():
    """Claude round-9 item 1: ONE build at a time. Fails loudly if another
    build/edit session holds the lock; never silently overlaps."""
    import fcntl
    lk=PAIR_ROOT/'.build.lock'
    lk.parent.mkdir(parents=True, exist_ok=True)
    fh=open(lk, 'w')
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        raise SystemExit('BUILD LOCK HELD: another build/edit session is running '
                         f'({lk}). One build at a time (Claude round-9). Wait or inspect the holder.')
    fh.write(f'{os.getpid()} {__import__("time").strftime("%Y-%m-%dT%H:%M:%S")}\n'); fh.flush()
    return fh

def main():
    _lock_fh=acquire_build_lock()
    parser=argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--plan',action='store_true',help='list every step/hash/gate; no toolchain execution')
    parser.add_argument('--execute',action='store_true',help='run the real twin-variant build')
    parser.add_argument('--incremental',action='store_true',help='cpp-only fresh set (agent EZ): recompile ONLY units whose SOURCE .cpp carries a CHANGES row; the header-consumer cascade is skipped and every other unit retains its object from a previous completed build (--base-build). Local iteration accelerator only, NOT upload-grade: the default full build remains the only upload-grade mode')
    parser.add_argument('--base-build',metavar='DIR',help='(requires --incremental) previous l26 variant dir — or a pair root containing l26-A*/l26-B* — to retain objects from; default: the most recent completed l26-A*/l26-B* per variant key (marker-key matched). --execute still refuses to overwrite existing variant dirs, so archive/rename the previous build first')
    args=parser.parse_args()
    if args.plan==args.execute:parser.error('choose exactly one of --plan / --execute')
    if args.base_build is not None and not args.incremental:parser.error('--base-build requires --incremental')
    execute=args.execute
    gates=[];steps=[]
    def record(name,ok,detail):
        entry={'gate':name,'ok':(bool(ok) if ok is not None else None),'detail':detail}
        with _PRINT_LOCK:
            gates.append(entry)
            print(json.dumps({'gate':name,'ok':(bool(ok) if ok is not None else 'PLANNED'),'detail':detail}),flush=True)
    root=PAIR_ROOT if execute else PAIR_ROOT/'plans'
    root.mkdir(parents=True,exist_ok=True)
    if execute:
        for variant in VARIANTS:
            if variant['dir'].exists():raise RuntimeError('refuse to overwrite '+str(variant['dir']))
    sel=compute_selection(record,tolerate_drift=not execute,incremental=args.incremental)
    if not execute and sel['pin_drift_rows']:
        print(json.dumps({'warning':'part CHANGES pin drift (parallel agents mid-flight; final --execute will hard-fail until hashes are refreshed)','rows':sel['pin_drift_rows']}),flush=True)
    # agent FG stamp generation (Claude hardening b+c, after the phantom
    # build): BEFORE any compile — plan cleans a stale stamp, execute stamps
    # provenance and refuses a not-generated stamp (G-stamp-present).
    generate_build_stamp(sel,record,execute,steps)
    if args.incremental:
        prepare_incremental(sel,record,args.base_build)
        # Operator safety warning (agent EZ requirement 3): the header rows
        # skipped by the cpp-only fresh set, their consumer counts, and the
        # upload-grade reminder are printed on EVERY incremental run.
        print(json.dumps({'warning':'INCREMENTAL MODE: header-replacement rows are SKIPPED for the fresh set; their consumers keep retained objects from the previous build',
                          'skipped_header_rows':sel['header_skip_rows'],
                          'risk':'mixed-header ODR hazard: retained objects were compiled against the previous build\'s effective headers, not recompiled against the current candidates; any retained unit whose inputs drifted is forced back into the fresh set (G-inc-retained-inputs-unchanged)',
                          'retained_from':{k:((v['dir'] or 'NONE — units fall back to the l25-A BASE objects (pre-variant-define; define-coverage gate will fail rather than build a non-variant runtime)')+' ['+v['source']+']') for k,v in sel['retained'].items()},
                          'fresh_units':len(sel['affected']),'fresh_units_full_mode':len(sel['affected_full']),'forced_fresh_units':sorted(sel.get('forced_fresh') or []),
                          'upload_grade':'NOT upload-grade: the default full build (without --incremental) remains the ONLY mode for upload-grade builds; --incremental must be passed explicitly for local iteration'}),flush=True)
    q=compose_copyin_probe(record,steps,execute)
    if q['composed_path'] is not None:
        ui_row=next(r for r in sel['changes'] if str(Path(r['candidate']).resolve())==str(Path(q['pins']['unrealinitializer']['base_path']).resolve()))
        sel['replacements'][ui_row['original']]=str(q['composed_path'])
        # the typescan-r4 candidate is now a second-hop intermediate consumed
        # only through the composed file; redirect token hits accordingly
        sel['override_intermediates']={str(Path(ui_row['candidate']).resolve()):str(q['composed_path'])}
        record('G2q-second-hop-override',True,{'unit':'unreal-unrealinitializer','typescan_row_original':ui_row['original'],'typescan_row_candidate':ui_row['candidate'],'composed':str(q['composed_path']),'composed_sha256':q['composed_sha256'],'mode':q['mode'],'composed_has_token':q['composed_has_token']})
    else:
        record('G2q-second-hop-override',False,{'note':'overlay composition unavailable; unit keeps the typescan-r4 candidate source for this PLAN run'})
    sel['copyin_probe']=q
    sel['driver_staged_sources']=q.get('staged_sources') or {}
    # l26: per-token target sets. The B skip token drives the DIAGNOSTIC
    # variant's define units; the CTOKEN engine-listener switch drives the
    # PRODUCTION variant A. Both tokens must be present in the tree or the
    # corresponding define would silently no-op the variant.
    targets_b=token_targets(sel,q['composed_path'] if q.get('composed_has_token') else None,TOKEN)
    targets_c=token_targets(sel,None,CTOKEN)
    targets_map={TOKEN:targets_b,CTOKEN:targets_c}
    # [L26][P2] the B skip-token presence gate runs ONLY when variant B is
    # in the ACTIVE set (env opt-in). With the P2 tree patch applied the
    # token is absent from every compiled source; an opt-in B must fail
    # HERE, loudly, rather than silently equal variant A.
    b_active=any(v['token']==TOKEN for v in VARIANTS)
    record('G4-token-present',(bool(targets_b['token_files']) if b_active else None),
           {'token':TOKEN,'token_files':targets_b['token_files'],
            'note':('variant B active (L26_BUILD_B=1); token presence enforced at execute'
                    if b_active else
                    'variant B not in the active build set (agent-FX P2 drop); skip-token presence not enforced this run')})
    if b_active and execute and not targets_b['token_files']:
        raise RuntimeError('token '+TOKEN+' not found in the tree: the listener agent guard has not landed (or the P2 tree patch removed the skip arms); refusing to build a variant B that would silently equal variant A')
    record('G4-ctoken-present',bool(targets_c['token_files']),{'token':CTOKEN,'token_files':targets_c['token_files']})
    if execute and not targets_c['token_files']:
        raise RuntimeError('token '+CTOKEN+' not found in the tree: the variant C engine-listener switch has not landed; variant A would silently build with the hand-rolled registration path')
    merged_targets={'token_files':sorted(set(targets_b['token_files'])|set(targets_c['token_files'])),
                    'token_units':sorted(set(targets_b['token_units'])|set(targets_c['token_units'])),
                    'shared_new_headers':sorted(set(targets_b['shared_new_headers'])|set(targets_c['shared_new_headers'])),
                    'conservative':targets_b['conservative'] or targets_c['conservative'],
                    'define_units':sorted(set(targets_b['define_units'])|set(targets_c['define_units']))}
    record('G5-define-units',True,{'token_units_B':targets_b['token_units'],'token_units_C':targets_c['token_units'],'conservative':merged_targets['conservative'],'define_units_union':merged_targets['define_units']})
    if args.incremental:
        # agent EZ safety: a variant define must reach every unit that
        # mentions its token — either a fresh compile now, or a retained
        # object PROVEN to already carry the define by the previous
        # same-variant build's marker define_units. Anything else would
        # silently build a non-variant runtime (e.g. retaining pre-define
        # l25-A objects); the gate fails the build instead.
        for v in VARIANTS:
            t=targets_map[v['token']];entry=sel['retained'][v['key']]
            prev_def=set(((entry.get('marker') or {}).get('define_units')) or [])
            unc=sorted(n for n in t['define_units'] if n not in sel['affected'])
            bad=sorted(n for n in unc if n not in prev_def)
            record('G-inc-define-units-covered-'+v['key'],not bad,
                   {'variant':v['key'],'token':v['token'],'define_units':t['define_units'],'not_fresh':unc,
                    'inherited_with_define_from_previous_build':sorted(n for n in unc if n in prev_def),'uncovered':bad,
                    'note':'variant defines must hit every token unit: fresh compile now, or a retained object proven (previous same-variant marker define_units) to already carry the define'})
    built={}
    if execute:
        # [agent FY concurrent twins] pass2 has no data dependency on pass1
        # (both read only the frozen BASE pass receipts and write to their own
        # pass dir; the overlay/marker are prepared up front), and variant B
        # never reads variant A's outputs — so the four (variant,pass) tasks
        # run concurrently in one pool of 4. Gates from the tasks are buffered
        # per task and merged deterministically (A-p1, A-p2, B-p1, B-p2) after
        # the join; per-variant finishers then run serially. Receipt content is
        # identical to the sequential driver (see module docstring).
        jobs,jobs_note=resolve_build_jobs()
        record('G-concurrent-twins-budget',True,{'build_jobs':jobs,'source':jobs_note,'knob':'L26_BUILD_JOBS env (integer >=1): caps GLOBAL clang/lld subprocess concurrency across all four pass tasks','pass_pool_workers':4,'compile_pool_workers_per_pass':2,'cpu_count':os.cpu_count(),'gate_merge_order':['A-pass1','A-pass2','B-pass1','B-pass2','finish-A','finish-B','pair'],'note':'--plan stays sequential/byte-identical; per-pass DEPENDENCY-GRAPH/COMPOSITION receipts are written by the serial finisher (pass2 graph cumulative over pass1 pins, historical content preserved)'})
        jobs_sem=threading.BoundedSemaphore(jobs)
        ctxs={v['key']:prepare_variant(v,sel,targets_map[v['token']],record,True,steps) for v in VARIANTS}
        passes={};task_gates={}
        with concurrent.futures.ThreadPoolExecutor(max_workers=4,thread_name_prefix='l26-pass') as pass_pool:
            futs={}
            for variant in VARIANTS:
                for passno in (1,2):
                    tgates,trec=make_task_recorder()
                    task_gates[(variant['key'],passno)]=tgates
                    futs[(variant['key'],passno)]=pass_pool.submit(build_pass,variant,ctxs[variant['key']],passno,sel,trec,True,jobs_sem)
            try:
                for key in futs:passes[key]=futs[key].result()
            except BaseException:
                # cancel queued (not yet started) pass tasks; running ones
                # finish their in-flight work first (same wait discipline the
                # per-pass compile pools already apply on failure), then the
                # original exception propagates and no receipts are written.
                pass_pool.shutdown(wait=False,cancel_futures=True)
                raise
            for key in task_gates:gates.extend(task_gates[key])
        for variant in VARIANTS:
            built[variant['key']]=finish_variant(variant,ctxs[variant['key']],passes[(variant['key'],1)],passes[(variant['key'],2)],sel,record,True,steps)
    else:
        built={v['key']:build_variant(v,sel,targets_map[v['token']],record,False,steps) for v in VARIANTS}
    pair_receipt(built,merged_targets,record,execute,steps,root,sel)
    plan={'mode':'execute' if execute else 'plan','driver':str(Path(__file__).resolve()),'driver_sha256':sha(Path(__file__).resolve()),'token':TOKEN,'ctoken':CTOKEN,
          'base':str(BASE),'variants':[{'key':v['key'],'dir':str(v['dir']),'artifact':v['artifact'],'token':v['token'],'define':v['define'],'label':v['label'],'unsafe':v['unsafe'],'safety':UNSAFE_NOTE if v['unsafe'] else None} for v in VARIANTS],
          'selected_units':len(sel['base']['selected_units']),'affected_units':len(sel['affected']),'retained_units':len(sel['base']['selected_units'])-len(sel['affected']),
          'incremental':(None if not args.incremental else {'fresh_units':len(sel['affected']),'fresh_units_full_mode':len(sel['affected_full']),'retained_units':len(sel['base']['selected_units'])-len(sel['affected']),'forced_fresh_units':sorted(sel.get('forced_fresh') or []),
                        'retained_from':{k:{'dir':v['dir'],'source':v['source']} for k,v in sel['retained'].items()},
                        'header_rows_skipped':sel['header_skip_rows'],
                        'upload_grade':'NOT upload-grade; the full build (default, without --incremental) is the only upload-grade mode'}),
          'header_aliases':len(sel['aliases']),'changes':len(sel['changes']),'added_headers':len(sel['added']),
          'copyin_probe':sel['copyin_probe']['receipt_section'],
          'build_stamp':sel.get('build_stamp'),
          'driver_staged_sources':dict(sel['driver_staged_sources']),
          **({'pass_concurrency':{'pass_pool_workers':4,'compile_pool_workers_per_pass':2,'build_jobs':jobs,'jobs_source':jobs_note,'knob':'L26_BUILD_JOBS env (integer >=1) caps global clang/lld subprocess concurrency','gate_merge_order':['A-pass1','A-pass2','B-pass1','B-pass2','finish-A','finish-B','pair']}} if execute else {}),
          'row_classifications':sel['row_classifications'],'unit_row_units':sel['unit_row_units'],
          'header_consumers':sel['header_consumers'],
          'header_routing_proof':(built['A']['routes'][0] if built['A'].get('routes') else None),
          'token_files':merged_targets['token_files'],'token_units':merged_targets['token_units'],'conservative':merged_targets['conservative'],
          'token_units_B':targets_b['token_units'],'token_units_C':targets_c['token_units'],'define_units_union':merged_targets['define_units'],
          'steps':steps,'gates':gates,
          'next':'run l26_packaging_driver.py --plan (already validated wiring), then after agents land: re-run this --plan, then --execute' if not execute else 'run l26_packaging_driver.py --prepare then --execute',
          'console_contact':False,'native_execution':False}
    write(root/('EXECUTION-RECEIPT.json' if execute else 'PLAN.json'),plan)
    # [gate law 2026-10-12] the verdict is printed AFTER the failed-gate check
    # (it used to print PASS first, then exit 1).
    EXPECTED_STATUS={'G2q-copyin-probe-pin-unrealinitializer.base'}
    _bad=[g for g in gates if g['ok'] is False and g['gate'] not in EXPECTED_STATUS]
    print(('PLAN ' if not execute else ('FAIL ' if _bad else 'PASS '))+json.dumps({'mode':plan['mode'],'incremental':bool(args.incremental),'affected_units':plan['affected_units'],'affected_units_full_mode':(len(sel['affected_full']) if args.incremental else plan['affected_units']),'token_files':len(merged_targets['token_files']),'steps':len(steps),'gates':len(gates)}),flush=True)
    if execute:
        # Claude round-9 item 2: expected-status gates are reported, not failures.
        EXPECTED_STATUS={'G2q-copyin-probe-pin-unrealinitializer.base'}
        bad=[g for g in gates if g['ok'] is False and g['gate'] not in EXPECTED_STATUS]
        exp=[g for g in gates if g['ok'] is False and g['gate'] in EXPECTED_STATUS]
        if exp: print('EXPECTED-STATUS (tolerated): '+json.dumps([g['gate'] for g in exp]),flush=True)
        if bad:raise SystemExit('gates failed: '+json.dumps(bad))

if __name__=='__main__':main()
