#!/usr/bin/env python3
"""l25 twin-variant runtime build driver (Claude-approved l25 item 5, agent R prep).

Produces TWO build variants from one tree state, reusing build_runtime.py
helpers WITHOUT modifying that base driver:

  variant A "l25-A": normal delete-listener registration (no extra define).
  variant B "l25-B": diagnostic only. Token translation units compile with
      -DMODBRIDGE_L25_SKIP_DELETE_LISTENER (exact spelling) so the call site
      wrapped by the listener agent in
      typescan-r4/candidate/src/UnrealInitializer.cpp takes the #else
      skip+log branch and the delete-listener registration never runs.

Each variant is built as an independent pass1/pass2 twin exactly the way
build_runtime.py builds the single l25 runtime (same gates, same receipts,
same determinism discipline: per-variant composed-object hashes pinned in
COMPOSITION-RECEIPT/TWIN-RECEIPT). A cross-variant pair receipt proves the
two runtimes differ and confines per-unit object deltas to the units that
received the variant define.

--plan performs NO toolchain execution: it recomputes the selection from the
current CHANGES.json state, verifies every pin it can, and lists every step,
input hash and gate. The real build runs only with --execute after the
parallel listener/phase-tag/review-fix agents land their edits.

Rules honored: no console/network contact; never edits
typescan-r4/candidate/src or include/ (owned by the code agents); the base
driver build_runtime.py is imported, never rewritten.
"""
import argparse,concurrent.futures,copy,hashlib,importlib.util,json,os,subprocess,sys,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sys.dont_write_bytecode=True
_spec=importlib.util.spec_from_file_location('build_runtime_l25_base',HERE/'build_runtime.py')
br=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(br)
BASE=br.BASE;LLVM=br.LLVM
sha,pin,write,deps,rewrite_compile,run=br.sha,br.pin,br.write,br.deps,br.rewrite_compile,br.run

TOKEN='MODBRIDGE_L25_SKIP_DELETE_LISTENER'
PAIR_ROOT=ROOT/'outputs/2026-10-08-l25-build-variants'
QPART=HERE/'copyin-probe'
QCOMPOSED=PAIR_ROOT/'staging-copyin-probe'/'UnrealInitializer.cpp'
VARIANTS=(
 {'key':'A','dir':PAIR_ROOT/'l25-A','artifact':'runtime-l25-A.o',
  'define':None,'registration':'normal',
  'label':'l25-A normal delete-listener registration',
  'unsafe':False,
  'status':'MAC_ONLY_L25_VARIANT_A_COMPLETE_LINK',
  'twin_status':'MAC_ONLY_IDENTICAL_L25_VARIANT_A_RUNTIME_PAIR'},
 {'key':'B','dir':PAIR_ROOT/'l25-B','artifact':'runtime-l25-B.o',
  'define':'-D'+TOKEN,'registration':'skipped (diagnostic only)',
  'label':'l25-B delete-listener registration SKIPPED via '+TOKEN,
  'unsafe':True,
  'status':'MAC_ONLY_L25_VARIANT_B_COMPLETE_LINK_SKIP_DELETE_LISTENER',
  'twin_status':'MAC_ONLY_IDENTICAL_L25_VARIANT_B_RUNTIME_PAIR_SKIP_DELETE_LISTENER'},
)
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

# ---------------------------------------------------------------- selection
def compute_selection(record,tolerate_drift=False):
    """Mirror of the build_runtime.main() selection stage (same asserts).
    Plan mode passes tolerate_drift=True so mid-flight edits by the parallel
    code agents are REPORTED (gate G2 fails, plan continues) instead of
    crashing; --execute keeps the hard pin discipline."""
    oldsel=json.loads((BASE/'SOURCE-SELECTION.json').read_text())
    approved_header_identities={(Path(p['path']).name,p['sha256']) for p in oldsel['source_pins'] if Path(p['path']).suffix in ['.h','.hpp','.inc','.inl']}
    allpins=sum((oldsel[k] for k in ('source_pins','receipt_pins','artifact_pins')),[])
    drift=[p['path'] for p in allpins if sha(p['path'])!=p['sha256']]
    record('G1-base-selection-pins',not drift,{'pins':len(allpins),'drift':drift})
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
        if sha(u['dependency'])!=u['dependency_sha256']:raise RuntimeError('base dependency drift '+u['name'])
        ds=deps(u['dependency']);unitdeps[u['name']]=ds;universe.update(ds)
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
                    layer_rank=lambda path: next((i for i,layer in enumerate(('/2026-10-08-l24-runtime/','/2026-10-08-l23-runtime/')) if layer in path),99)
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
    affected=[]
    for u in base['selected_units']:
        if str(Path(u['source']).resolve()) in replacements or any(str(p) in aliases or str(p.resolve()) in aliases for p in unitdeps[u['name']]):affected.append(u['name'])
    include_dirs=sorted(set(declared_include_dirs)|{str(Path(p).parent) for p in added}|{str(Path(x['candidate']).parent) for x in changes if Path(x['candidate']).suffix in ['.hpp','.h']})
    # Z9 loud no-op guard: every CHANGES row must key on an original the l24
    # build actually consumed (unit source, dep-universe path, inherited
    # overlay target, or alias-identical (name,digest) universe file). A row
    # keyed anywhere else (e.g. the port-root LuaUObject.hpp) would silently
    # compile nothing. Non-code lineage files (.json/.ini/.md) are reported
    # but allowed; every code row must classify or the build stops.
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
    if noop and not tolerate_drift:raise RuntimeError('CHANGES rows keyed on originals the l24 build never consumed (silent no-ops): '+repr(noop))
    unit_row_units=sorted({unit_sources[str(Path(r['original']).resolve())] for r in changes if str(Path(r['original']).resolve()) in unit_sources})
    missing=[n for n in unit_row_units if n not in affected]
    record('G2s-unit-source-rows-affected',not missing,{'units_from_cpp_rows':unit_row_units,'missing_from_affected':missing})
    if missing and not tolerate_drift:raise RuntimeError('unit-source rows whose units are not in the affected set: '+repr(missing))
    presence={u:(u in affected) for u in O_UNITS}
    record('G2t-listener-units-affected',all(presence.values()),{'presence':presence,'note':'unreal-uobjectarray/unreal-flat-classlistener were inherited l22 objects in the l24 receipt; O rows must move them to fresh. lua-luauobject rides the UObjectArray.hpp (name,digest) alias'})
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
    record('G12b-header-consumers-recompiled',not coverage,{'consumers_per_header':header_consumers,'gaps':coverage})
    if coverage and not tolerate_drift:raise RuntimeError('replaced-header consumers left retained (mixed-header hazard): '+repr(coverage))
    return {'oldsel':oldsel,'approved_header_identities':approved_header_identities,'changes':changes,'added':added,
            'replacements':replacements,'header_specs':header_specs,'base':base,'unitdeps':unitdeps,
            'aliases':aliases,'merged_aliases':merged_aliases,'affected':affected,'include_dirs':include_dirs,
            'pin_drift_rows':drift_rows,'row_classifications':classifications,'unit_row_units':unit_row_units,
            'header_consumers':header_consumers}

def header_route_check(sel,argv):
    """Z13 E3 guard (resolution half): simulate clang's include lookup for
    every replaced-header row against a unit compile argv — -I directories in
    argv order, then the merged vfs overlay map — and prove every reachable
    spelling resolves to the l25 candidate, never the port-root original.
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
                 'replaced_original_sha256':digest,'is_l25_candidate':ok}
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
                        'wired_at':'packaging (l25_packaging_driver.py substitutes BUILD-INPUTS outer_bridge)'},
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
            'receipt_section':{'part':'copyin-probe','author':'agent-Q (2026-10-08); patch re-anchored by agent-R same day (struct hunk moved to the stable pre-L13AdmissionOps landmark after the execute-order failure; content byte-identical, see MANIFEST reanchor_note)','wiring':'outputs/2026-10-08-l25-runtime/copyin-probe/WIRING.md',
                'unrealinitializer':{'base':{'path':ui['base_path'],'sha256':ui['base_sha256']},
                    'patch':{'path':str(QPART/ui['patch']),'sha256':ui['patch_sha256']},
                    'ready_candidate':{'path':ui['candidate_path'],'sha256':ui['candidate_sha256']},
                    'composed':{'path':str(QCOMPOSED),'sha256':composed_sha},'mode':mode,
                    'definition_order':ue_ev},
                'runtime_start_bridge':{'base':{'path':bridge['base_path'],'sha256':bridge['base_sha256']},
                    'patch':{'path':str(QPART/bridge['patch']),'sha256':bridge['patch_sha256']},
                    'ready_candidate':{'path':bridge['candidate_path'],'sha256':bridge['candidate_sha256']},
                    'definition_order':br_ev,
                    'wired_at':'packaging (l25_packaging_driver.py substitutes BUILD-INPUTS outer_bridge; adopted whole-file, never rebased)'},
                'import_delta':man.get('import_delta'),'console_contact':False}}

def token_targets(sel,extra_hit=None):
    """Files mentioning TOKEN and the translation units that must receive the
    variant define. extra_hit adds the composed copyin-probe overlay (its
    token presence is known from the composed BYTES, so plan mode works
    before the file is persisted). Hits on an overridden second-hop
    intermediate (the typescan-r4 candidate that the composed file replaces)
    are redirected to the composed path so they resolve to the unit that
    actually compiles them.
    Contract: the guard lives only in UnrealInitializer.cpp (O's switch wraps
    BOTH registrations, lines ~544-579). Defensive fallback: any token hit
    outside the BASE dependency universe that is not itself a replaced source
    is treated as a shared new header, and the define is then applied to
    every recompiled unit (superset; the macro only guards the one call
    site)."""
    redirect=sel.get('override_intermediates',{})
    raw=set()
    for p in sorted(HERE.rglob('*')):
        if not p.is_file() or p.suffix not in SCAN_SUFFIXES:continue
        if any(part in SCAN_SKIP_PARTS for part in p.relative_to(HERE).parts):continue
        try:data=p.read_bytes()
        except OSError:continue
        if TOKEN.encode() in data:raw.add(str(p.resolve()))
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

# ---------------------------------------------------------------- building
def build_variant(variant,sel,targets,record,execute,steps):
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
            'token':TOKEN,'delete_listener_registration':variant['registration'],
            'unsafe':variant['unsafe'],'safety':UNSAFE_NOTE if variant['unsafe'] else 'normal production configuration',
            'console_contact':False,'native_execution':False,
            'token_units':targets['token_units'],'define_units':sorted(define_units),
            'conservative_define_scope':targets['conservative'],
            'copyin_probe_wiring':sel['copyin_probe']['receipt_section']}
    results=[];variant_pins={};route_summaries=[]
    for passno in [1,2]:
        parent=json.loads((BASE/f'pass{passno}/COMPOSITION-RECEIPT.json').read_text())
        units=copy.deepcopy(parent['selected_units'])
        pdir=out/f'pass{passno}';objects=pdir/'objects'
        if execute:pdir.mkdir();objects.mkdir()
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
            routes,route_violations=header_route_check(sel,argv)
            route_agg['units'].append(u['name'])
            if route_violations:
                route_agg['violations'].append({'variant':variant['key'],'pass':passno,'unit':u['name'],'violations':route_violations})
                if execute:raise RuntimeError('header routing violation (Z13 E3 class) in '+u['name']+': '+repr(route_violations))
            for k,v in routes.items():route_agg['representative'].setdefault(k,v)
            if execute:
                run(argv,objects,u['name'],env)
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
                       argv=argv,selection='l25_variant_'+variant['key']+'_fresh_selected_source_or_header_dependent',
                       LLVM_CONFIG=str(LLVM/'llvm-config'))
            if u['name'] in define_units:new['variant_define']=variant['define']
            return new,ds
        done={};route_agg={'units':[],'violations':[],'representative':{}}
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
                        for p in ds:variant_pins[str(p.resolve())]=pin(p)
                        if count%25==0:print(json.dumps({'variant':variant['key'],'pass':passno,'compiled':count,'total':len(sel['affected'])}),flush=True)
        else:
            for u in units:
                u,ds=compile_unit(u);done[u['name']]=u
        units=[done[u['name']] for u in units]
        record('G12-header-routing-resolves-to-l25-candidates',not route_agg['violations'],
               {'variant':variant['key'],'pass':passno,'units_checked':len(route_agg['units']),
                'representative_routes':route_agg['representative'],'violations':route_agg['violations']})
        if route_agg['violations'] and execute:raise RuntimeError('header routing violations: '+repr(route_agg['violations'][:3]))
        route_summaries.append(route_agg['representative'])
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
            cmd=['-Map='+str(pdir/'runtime.map') if a.startswith('-Map=') else a for a in cmd]
            input_paths=[a for i,a in enumerate(cmd) if a.endswith(('.o','.a')) and i!=cmd.index('-o')+1]
            record('G6-link-input-census',len(input_paths)==len(set(input_paths))==391 and all(input_paths.count(u['object'])==1 for u in units),{'variant':variant['key'],'pass':passno,'inputs':len(input_paths)})
            for p in link['fixed_inputs']:assert sha(p['path'])==p['sha256']
            run(cmd,pdir,'link',env)
            after=run([LLVM/'llvm-nm','-u',runtime],pdir,'undefined',env)
            before=(BASE/f'pass{passno}/undefined.stdout').read_text()
            symbols=lambda s:{tuple(line.split()[-2:]) for line in s.splitlines() if line.split()}
            record('G7-import-delta-none',symbols(after)==symbols(before),{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout')})
            if symbols(after)!=symbols(before):raise RuntimeError('raw import delta must be declared before packaging')
            write(pdir/'LINK-COMMAND.json',{'argv':cmd,'ordered_inputs':input_paths,'fixed_inputs':link['fixed_inputs']})
            write(pdir/'COMPILE-RECEIPT.json',{'fresh_units':[{'name':u['name'],'command':pin(objects/(u['name']+'.command.json')),'object':pin(u['object'])} for u in units if u['name'] in sel['affected']],'retained_units':len(units)-len(sel['affected']),'header_overlay':pin(overlay)})
            write(pdir/'DEPENDENCY-GRAPH.json',{'selected_dependencies':[{'name':u['name'],'dependency':pin(u['dependency'])} for u in units],'new_effective_dependency_pins':list(variant_pins.values()),'driver_staged_source_pins':[pin(Path(k)) for k in sorted(sel['driver_staged_sources'])],'header_aliases':sel['aliases'],'added_headers':[pin(p) for p in sel['added']]})
            write(pdir/'IMPORT-DELTA.json',{'baseline':sorted([list(t) for t in symbols(before)]),'current':sorted([list(t) for t in symbols(after)]),'added':[],'removed':[],'baseline_artifact':pin(BASE/f'pass{passno}/undefined.stdout'),'qualification':'Raw runtime symbols; final PRX import identities checked during packaging.'})
            receipt={'status':variant['status'],'variant':marker,'pass':passno,'object':pin(runtime),'selected_units':units,'selected_count':len(units),'full_link_inputs':391,
                     'parent_composition':pin(BASE/f'pass{passno}/COMPOSITION-RECEIPT.json'),'fresh_current_pass':len(sel['affected']),'retained_current_units':len(units)-len(sel['affected']),
                     'scope':'l25 variant '+variant['key']+' accepted local code; '+variant['label']+'; no console proof',
                     'link_command':pin(pdir/'LINK-COMMAND.json'),'header_overlay':pin(overlay),'compile_receipt':pin(pdir/'COMPILE-RECEIPT.json'),'dependency_graph':pin(pdir/'DEPENDENCY-GRAPH.json'),'import_delta':pin(pdir/'IMPORT-DELTA.json'),
                     'baseline_undefined_count':len(symbols(before)),'candidate_undefined_count':len(symbols(after)),'actual_undefined_added':[],'actual_undefined_removed':[],
                     'native_execution':False,'menu_success':False,'console_contact':False}
            write(pdir/'COMPOSITION-RECEIPT.json',receipt)
            results.append({'runtime':pin(runtime),'composition':pin(pdir/'COMPOSITION-RECEIPT.json')})
            print(json.dumps({'variant':variant['key'],'pass':passno,'runtime':pin(runtime),'imports_changed':False}),flush=True)
        else:
            link_argv=json.loads(Path(parent['link_command']['path']).read_text())['argv']
            record('G6-link-input-census',None,{'variant':variant['key'],'pass':passno,'planned_inputs':391,'note':'387 retained pinned objects + fresh objects; exact census enforced at execute'})
            record('G7-import-delta-none',None,{'variant':variant['key'],'pass':passno,'baseline':str(BASE/f'pass{passno}/undefined.stdout'),'baseline_sha256':sha(BASE/f'pass{passno}/undefined.stdout'),'note':'llvm-nm -u equality with BASE enforced at execute'})
            steps.append({'step':'link-runtime','variant':variant['key'],'pass':passno,'output':str(runtime),'link_command_source':parent['link_command']['path'],'argv_shape':link_argv[:3]+['...','-o',str(runtime)]})
            steps.append({'step':'llvm-nm-undefined-gate','variant':variant['key'],'pass':passno,'object':str(runtime),'baseline':{'path':str(BASE/f'pass{passno}/undefined.stdout'),'sha256':sha(BASE/f'pass{passno}/undefined.stdout')}})
            results.append({'runtime':{'path':str(runtime),'sha256':'PENDING-BUILD'},'composition':{'path':str(pdir/'COMPOSITION-RECEIPT.json'),'sha256':'PENDING-BUILD'}})
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
        selection['source_pins']+=list(actual_pins.values())+[pin(x['candidate']) for x in sel['changes']]+[pin(p) for p in sel['added']]+[pin(overlay)]+[pin(sel['copyin_probe']['composed_path']),pin(QPART/'MANIFEST.json')]
        selection['receipt_pins']+=[x['composition'] for x in results]
        selection['artifact_pins']+=[x['runtime'] for x in results]
        selection.update(parent_selection=pin(BASE/'SOURCE-SELECTION.json'),native_execution=False,selected_changes=sel['changes'],l25_variant=marker)
        write(out/'ACTUAL-DEPENDENCIES.json',list(actual_pins.values()));write(out/'SOURCE-SELECTION.json',selection)
        write(out/'CHANGE-SELECTION.json',{'files':sel['changes'],'added_headers':sel['added'],'aliases':sel['aliases'],'affected_units':sel['affected'],'retained_units':len(sel['base']['selected_units'])-len(sel['affected']),'no_console_contact':True,'l25_variant':marker,'copyin_probe_wiring':sel['copyin_probe']['receipt_section'],'second_hop_source_overrides':{'unreal-unrealinitializer':str(sel['copyin_probe']['composed_path'])},'driver_staged_sources':{k:v for k,v in sel['driver_staged_sources'].items()}})
        write(out/'VARIANT-MARKER.json',marker)
        write(out/'TWIN-RECEIPT.json',{'status':variant['twin_status'],'variant':marker,'passes':results,'source_selection':pin(out/'SOURCE-SELECTION.json'),
            'fresh_units_each_pass':len(sel['affected']),'selected_units':len(sel['base']['selected_units']),'full_link_inputs':391,'new_imports':[],'console_contact':False,'native_execution':False,
            'copyin_probe_wiring':sel['copyin_probe']['receipt_section']})
    else:
        steps.append({'step':'write-variant-receipts','variant':variant['key'],'dir':str(out),'files':['TWIN-RECEIPT.json','SOURCE-SELECTION.json','CHANGE-SELECTION.json','ACTUAL-DEPENDENCIES.json','VARIANT-MARKER.json'],'passes':results})
    return {'marker':marker,'results':results,'routes':route_summaries}

def pair_receipt(built,targets,record,execute,steps,root):
    a,b=built['A'],built['B']
    receipt={'status':'MAC_ONLY_L25_VARIANT_PAIR_A_VS_B_RECEIPT','token':TOKEN,
             'contract_call_site':'typescan-r4/candidate/src/UnrealInitializer.cpp wrapped by the listener agent in #if !defined('+TOKEN+')',
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
        record('G10-object-deltas-confined-to-define-units',set(deltas)<=allowed,{'deltas':deltas,'allowed':sorted(allowed)})
        if not set(deltas)<=allowed:raise RuntimeError('unexpected cross-variant object deltas: '+repr(deltas))
        receipt['per_unit_object_deltas']=deltas
    else:
        record('G9-variants-differ',None,{'note':'enforced at execute; fails if the skip-define has no effect'})
        record('G10-object-deltas-confined-to-define-units',None,{'allowed':targets['define_units'],'note':'deltas must be a subset of the define units'})
    write(root/'VARIANT-PAIR-RECEIPT.json',receipt)
    steps.append({'step':'write-variant-pair-receipt','path':str(root/'VARIANT-PAIR-RECEIPT.json')})
    return receipt

def main():
    parser=argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--plan',action='store_true',help='list every step/hash/gate; no toolchain execution')
    parser.add_argument('--execute',action='store_true',help='run the real twin-variant build')
    args=parser.parse_args()
    if args.plan==args.execute:parser.error('choose exactly one of --plan / --execute')
    execute=args.execute
    gates=[];steps=[]
    def record(name,ok,detail):
        gates.append({'gate':name,'ok':(bool(ok) if ok is not None else None),'detail':detail})
        print(json.dumps({'gate':name,'ok':(bool(ok) if ok is not None else 'PLANNED'),'detail':detail}),flush=True)
    root=PAIR_ROOT if execute else PAIR_ROOT/'plans'
    root.mkdir(parents=True,exist_ok=True)
    if execute:
        for variant in VARIANTS:
            if variant['dir'].exists():raise RuntimeError('refuse to overwrite '+str(variant['dir']))
    sel=compute_selection(record,tolerate_drift=not execute)
    if not execute and sel['pin_drift_rows']:
        print(json.dumps({'warning':'part CHANGES pin drift (parallel agents mid-flight; final --execute will hard-fail until hashes are refreshed)','rows':sel['pin_drift_rows']}),flush=True)
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
    targets=token_targets(sel,q['composed_path'] if q.get('composed_has_token') else None)
    record('G4-token-present',bool(targets['token_files']),{'token':TOKEN,'token_files':targets['token_files']})
    if execute and not targets['token_files']:
        raise RuntimeError('token '+TOKEN+' not found in the tree: the listener agent guard has not landed; refusing to build a variant B that would silently equal variant A')
    record('G5-define-units',True,{'token_units':targets['token_units'],'conservative':targets['conservative'],'define_units':targets['define_units']})
    built={}
    for variant in VARIANTS:built[variant['key']]=build_variant(variant,sel,targets,record,execute,steps)
    pair_receipt(built,targets,record,execute,steps,root)
    plan={'mode':'execute' if execute else 'plan','driver':str(Path(__file__).resolve()),'driver_sha256':sha(Path(__file__).resolve()),'token':TOKEN,
          'base':str(BASE),'variants':[{'key':v['key'],'dir':str(v['dir']),'artifact':v['artifact'],'define':v['define'],'label':v['label'],'unsafe':v['unsafe'],'safety':UNSAFE_NOTE if v['unsafe'] else None} for v in VARIANTS],
          'selected_units':len(sel['base']['selected_units']),'affected_units':len(sel['affected']),'retained_units':len(sel['base']['selected_units'])-len(sel['affected']),
          'header_aliases':len(sel['aliases']),'changes':len(sel['changes']),'added_headers':len(sel['added']),
          'copyin_probe':sel['copyin_probe']['receipt_section'],
          'driver_staged_sources':dict(sel['driver_staged_sources']),
          'row_classifications':sel['row_classifications'],'unit_row_units':sel['unit_row_units'],
          'header_consumers':sel['header_consumers'],
          'header_routing_proof':(built['A']['routes'][0] if built['A'].get('routes') else None),
          'token_files':targets['token_files'],'token_units':targets['token_units'],'conservative':targets['conservative'],
          'steps':steps,'gates':gates,
          'next':'run l25_packaging_driver.py --plan (already validated wiring), then after agents land: re-run this --plan, then --execute' if not execute else 'run l25_packaging_driver.py --prepare then --execute',
          'console_contact':False,'native_execution':False}
    write(root/('EXECUTION-RECEIPT.json' if execute else 'PLAN.json'),plan)
    print(('PASS ' if execute else 'PLAN ')+json.dumps({'mode':plan['mode'],'affected_units':plan['affected_units'],'token_files':len(targets['token_files']),'steps':len(steps),'gates':len(gates)}),flush=True)
    if execute:
        bad=[g for g in gates if g['ok'] is False]
        if bad:raise SystemExit('gates failed: '+json.dumps(bad))

if __name__=='__main__':main()
