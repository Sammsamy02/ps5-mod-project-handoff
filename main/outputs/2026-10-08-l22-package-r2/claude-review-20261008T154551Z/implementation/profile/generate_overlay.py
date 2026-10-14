#!/usr/bin/env python3
"""Local-only l22 overlay generator. Reads retained inputs; writes only beside this script."""
from pathlib import Path
import json,hashlib,runpy,re
root=Path(__file__).resolve().parent; ws=root.parents[2]
old=ws/'outputs/2026-10-07-l19-local-profile-r4'; audit=ws/'outputs/2026-10-08-l21-audit'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
q=lambda x:json.dumps(x)
p=json.loads((old/'profiles/townfall-build-facts.json').read_text())
a=json.loads((audit/'allocator/postfetch/candidate-table.json').read_text())
assert len(a['entries'])==17 and p['image']['sha256']=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
sources=[audit/'allocator/postfetch/candidate-table.json',audit/'allocator/postfetch/macro-assumptions.json',audit/'allocator/postfetch/source-pins.json',audit/'allocator/REPORT-SOURCE-CORROBORATION.md',audit/'mod-api/MENU-STARTUP-MISSING-JOIN.json',audit/'mod-api/OFFICIAL-SCO-COMPARISON.json',audit/'mod-api/sco-parameter-layout.json']
p['evidence']['l22_allocator']={'promotion_authority':'user-approved l22 directive and docs/superpowers/plans/2026-10-08-l22-runtime.md','image_sha256':p['image']['sha256'],'official_commit':'6978b63c8951e57d97048d8424a0bebd637dde1d','scope':a['scope'],'conditional_correspondence':a['conditional_correspondence'],'actual_game_macro_captured':False,'accepted_exec_commands':1,'rejected_exec_disabled_reason':'extra FExec overload shifts all17 methods by8; disagrees with independently classified image anchors','entries':a['entries'],'source_pins':{str(s.relative_to(ws)):sha(s) for s in sources},'live_slot_contents_measured':False,'native_success_claimed':False}
for e in a['entries']:
 assert e['source_default_branch']==1 and e['source_default_image_slot']['target_pt_load_flags']&1
 p['required_vtables'].append({'name':'FMalloc::'+e['method'],'offset':int(e['conditional_source_byte_offsets']['UE_ALLOW_EXEC_COMMANDS=1'],16),'width':8,'source':f"l21 allocator/postfetch/candidate-table.json:{e['method']}; Epic6978b63c8951e57d97048d8424a0bebd637dde1d exec-enabled default + image {p['image']['sha256']}; {e['status']}"})
# Reuse the original pure generator functions, with output root explicitly redirected.
g=runpy.run_path(str(old/'profiles/generate_profile.py'));generate=g['generate'];generate.__globals__['root']=root
digest=generate(p);(root/'profiles/townfall-build-facts.json').write_text(json.dumps(p,indent=2)+'\n')
compiled=(old/'units/CompiledNativeLayoutDeclarations.hpp').read_text()
strings=[json.loads(x.rstrip(',')) for x in compiled.splitlines() if x.startswith('"')]
assert len(strings)==4 and '[FMalloc]' not in strings[1]
strings[1]+='\n[FMalloc]\n'+''.join(f"{e['method']}={e['conditional_source_byte_offsets']['UE_ALLOW_EXEC_COMMANDS=1']}\n" for e in a['entries'])
strings[2]='compiled-audited-townfall-l22';strings[3]=digest
(root/'units/CompiledNativeLayoutDeclarations.hpp').write_text('#pragma once\nnamespace RC::Platform::NativeLayouts {\ninline constexpr CompiledNativeLayoutDeclarations kCompiledNativeLayoutDeclarations{\n'+',\n'.join(q(s) for s in strings)+'\n};\n}\n')
join=json.loads((audit/'mod-api/MENU-STARTUP-MISSING-JOIN.json').read_text())['all_joined_keys'];assert len(join)==63 and len({x['name'] for x in join})==63
pins={x['name']:x for k in ['required_members','required_vtables'] for x in p[k]}
rows=[]
for e in join:
 c=e['scoped_classification'];d='Required' if c=='startup_menu_source_dependency' else 'Deferred' if c=='startup_menu_runtime_discovery_not_profile_offset' else 'Conditional'
 kind='Runtime' if d=='Deferred' else 'Member' if e['inventory_kind']=='member' else 'Vtable'
 pin=pins.get(e['name']);rows.append(' {'+','.join([q(e['name']),'Kind::'+kind,'Disposition::'+d,str(bool(pin)).lower(),str(pin['offset'] if pin else 0),q(c)])+'}')
header='''#pragma once
#include <cstdint>
#include <string>
#include <vector>
// Exact63 selected startup/menu/spawn dependencies from the pinned l21 join.
// Only staged declarations are inspected. No game object reads or map insertion.
namespace rc::joinedlayout {
enum class Kind { Member, Vtable, Runtime };
enum class Disposition { Required, Conditional, Deferred };
struct Key { const char* name; Kind kind; Disposition disposition; bool pinned; uint32_t offset; const char* source_classification; };
inline constexpr Key kKeys[]={
ROWS
};
static_assert(sizeof(kKeys)/sizeof(kKeys[0])==63);
inline const char* disposition(Disposition d) { return d==Disposition::Required?"startup-required":d==Disposition::Conditional?"conditional":"conditional/runtime-discovered"; }
inline const char* kind(Kind k) { return k==Kind::Member?"member":k==Kind::Vtable?"vtable":"runtime-discovery"; }
template<class Collection> inline const typename Collection::value_type* find(const Collection* values,const char* name) {
 if(values)for(const auto& value:*values)if(value.name==name)return &value;
 return nullptr;
}
template<class Collection> inline bool conditional_absent(Kind kind,const char* name,const Collection* values) {
 for(const auto& key:kKeys)if(key.kind==kind&&key.disposition==Disposition::Conditional&&std::string(key.name)==name)return !find(values,name);
 return false;
}
struct Check { const Key* key; bool present=false,accepted=true; const char* status; std::string detail; };
struct Result { bool ok=true; unsigned required=0,conditional=0,deferred=0,missing_required=0; std::vector<Check> checks; };
template<class Collection> inline Result evaluate(const Collection* members,const Collection* vtables) {
 Result result;
 for(const auto& key:kKeys) {
  if(key.disposition==Disposition::Deferred) {
   ++result.deferred;
   result.checks.push_back({&key,false,true,"DEFERRED","discovered by TypeChecker after reflection startup; not a layout-map key; no value invented"});
   continue;
  }
  const bool required=key.disposition==Disposition::Required;
  if(required)++result.required;else ++result.conditional;
  const auto* entry=find(key.kind==Kind::Member?members:vtables,key.name);
  const bool matches=entry&&(!key.pinned||entry->offset==key.offset);
  const char* status=!entry?"MISSING":!matches?"MISMATCH":key.pinned?"PRESENT":"PRESENT_UNPINNED";
  const bool accepted=!required||matches;
  if(!accepted){result.ok=false;++result.missing_required;}
  result.checks.push_back({&key,entry!=nullptr,accepted,status,required?"required before seeded startup/menu calls": "conditional path; existing lazy checks remain authoritative when used"});
 }
 return result;
}
}
'''.replace('ROWS',',\n'.join(rows))
(root/'units/JoinedLayoutPreflight.hpp').write_text(header)
original=ws/'outputs/2026-10-08-r5-thread-final-integration/thread/UnrealInitializer.cpp'
s=original.read_text().replace('#include "RuntimeBuildProfile.hpp"','#include "RuntimeBuildProfile.hpp"\n#include "JoinedLayoutPreflight.hpp"\n#include "CodeSignatureProfile.hpp"')
for plural,kind,singular in [('members','Member','member'),('vtables','Vtable','vtable')]:
 oldloop=f'''                for(const auto& pin:rc::runtimeprofile::kRequired{plural.title()})
                    rc::buildfacts::require_layout(facts,"{singular}",pin,staged?&staged->{plural}:nullptr);'''
 newloop=f'''                for(const auto& pin:rc::runtimeprofile::kRequired{plural.title()})
                    if(!rc::joinedlayout::conditional_absent(rc::joinedlayout::Kind::{kind},pin.name,staged?&staged->{plural}:nullptr))
                        rc::buildfacts::require_layout(facts,"{singular}",pin,staged?&staged->{plural}:nullptr);'''
 assert oldloop in s;s=s.replace(oldloop,newloop)
needle='                if(!facts.ok) throw std::runtime_error("[BuildProfile] collect-all preflight refused: "+failures);'
assert needle in s
s=s.replace(needle,'''                // L22 JOINED PREFLIGHT: the staged maps are loaded, but engine maps
                // are not yet published. Run once before any seeded engine call.
                const auto joined=rc::joinedlayout::evaluate(staged?&staged->members:nullptr,staged?&staged->vtables:nullptr);
                for(const auto& check:joined.checks)
                {
                    Output::send(STR("[LayoutPreflight] key={} kind={} disposition={} status={} detail={}\\n"),
                        ensure_str(check.key->name),ensure_str(rc::joinedlayout::kind(check.key->kind)),
                        ensure_str(rc::joinedlayout::disposition(check.key->disposition)),ensure_str(check.status),ensure_str(check.detail));
                    if(!check.accepted) failures+=std::string("joined.")+check.key->name+"; ";
                }
                Output::send(STR("[LayoutPreflight] checked={} startup-required={} conditional-map={} runtime-deferred={} missing-required={}\\n"),
                    joined.checks.size(),joined.required,joined.conditional,joined.deferred,joined.missing_required);
                if(!facts.ok||!joined.ok) throw std::runtime_error("[BuildProfile] collect-all preflight refused: "+failures);''')
s=s.replace('''            // ---- Phase 5: static_construct_object_internal (code constant; no
            // data-side validation exists — assigned at the consumer, bypassing
            // the mapped pipeline's permits_code admission per SLOTS-FEASIBILITY
            // section 4b) ----''','''            // ---- Phase 5: SCO static identity plus empty-callback ABI admission.
            // l21 official/source + pinned image evidence; target ABI assertions
            // live in UObjectGlobals.hpp. A successful native call is not claimed.''')
s=s.replace('''            // Read-path guard: SCO is a code constant, call-only by design;
            // identity/ABI stay unverified precisely because text reads are
            // refused on exec-only pages.''','''            // Code remains call-only. Identity uses the already authenticated
            // startup cache; this does not read the live executable mapping.''')
needle='            UObjectGlobals::SetupStaticConstructObjectInternalAddress(reinterpret_cast<void*>(sco_runtime));'
assert s.count(needle)==1
s=s.replace(needle,'''            if(admission_budget.expired(admission_ops.now_ms()))
                seed_refused("SCO shared admission deadline expired before binding");
            if(!admission_ops.code_identity(sco_runtime,sizeof(rc::codeprofile::kScoPrefix))||
               !admission_ops.cached_code_signature(sco_runtime,rc::codeprofile::kScoPrefix,sizeof(rc::codeprofile::kScoPrefix)))
                seed_refused("slot 4 static_construct_object: authenticated cached image/signature admission failed");
'''+needle)
s=s.replace('(code; identity and ABI unverified)\\n"),\n                         reinterpret_cast<void*>(sco_runtime));','(code; statically_verified_empty_callback; native success not observed)\\n"),\n                         reinterpret_cast<void*>(sco_runtime));')
(root/'units/UnrealInitializer.cpp').write_text(s)
print('Generated17 allocator facts; exact63 preflight; digest='+digest)
