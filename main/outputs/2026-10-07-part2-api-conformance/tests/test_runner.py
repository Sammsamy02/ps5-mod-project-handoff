"""Run the real Lua artifact against controlled local doubles; never engine proof."""
import json
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

BASE = Path(__file__).resolve().parents[1]
LUA = shutil.which("lua")
PRELUDE = r'''
local calls = 0
local runner = dofile(arg[1])
local function config(ids)
    return {enabled=true, evidence="host-fixture", side="host", select=ids,
        emit=function(line) io.write(line, "\n") end,
        metadata={run_id="host-fixture-only", game_id="Townfall", content_build="host-only",
                  scenario_id="host-only", context_id="host-only", runtime_build="standalone-lua",
                  runtime_sha256=string.rep("0",64),
                  game_binary_sha256=string.rep("0",64), script_sha256=string.rep("0",64)},
        allow={}, context={},
        thread_guard=function() return {kind="host-fixture", in_game_thread=true,
            thread_id="fixture", known_game_thread_id="fixture", proof_id="fixture-only"} end}
end
local function live()
    return {IsValid=function() return true end}
end
'''


@unittest.skipUnless(LUA, "standalone Lua unavailable; runner checks pending")
class RunnerTests(unittest.TestCase):
    def run_lua(self, code, before=""):
        with tempfile.TemporaryDirectory() as temp:
            driver = Path(temp) / "driver.lua"
            driver.write_text(before + PRELUDE + code)
            result = subprocess.run([LUA, str(driver), str(BASE / "conformance.lua")],
                                    capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return [json.loads(line) for line in result.stdout.splitlines() if line.strip()]

    def case(self, records, case_id):
        return next(r for r in records if r.get("kind") == "case" and r.get("id") == case_id)

    def test_loading_and_disabled_configuration_have_no_engine_side_effect(self):
        # Calling engine APIs or registering callbacks while loading must fail.
        before = r'''
local forbidden_count=0
local function forbidden() forbidden_count=forbidden_count+1; error("load invoked engine") end
for _,name in ipairs({"print","require","FindFirstOf","FindAllOf","StaticFindObject",
 "StaticConstructObject","FName","FText","LoadAsset","RegisterKeyBind",
 "ExecuteInGameThread","RegisterLoadMapPreHook","CreateInvalidObject"}) do _G[name]=forbidden end
'''
        records = self.run_lua(r'''
assert(forbidden_count==0)
local session, reason = runner.new({enabled=false})
assert(session==nil and reason=="disabled")
assert(forbidden_count==0)
io.write('{"kind":"test","calls":0}\n')
''', before)
        self.assertEqual(records, [{"kind": "test", "calls": 0}])

    def test_presence_and_missing_context_are_not_behavioral_proof(self):
        # Turning available symbols into proved behavior or invoking a missing fixture must fail.
        records = self.run_lua(r'''
FindFirstOf=function() calls=calls+1; return live() end
FName=function() calls=calls+1; return {} end
local c=config({"database.findfirst","fname.add-default"})
c.allow.engine_read=true; c.allow.name_pool=true
local s=assert(runner.new(c)); s:run(); s:finish()
assert(calls==0)
''')
        self.assertEqual(self.case(records, "database.findfirst")["status"], "skipped")
        self.assertEqual(self.case(records, "fname.add-default")["status"], "skipped")
        binding = next(r for r in records if r.get("kind") == "binding" and r["name"] == "FName")
        self.assertTrue(binding["binding_present"])
        self.assertFalse(binding["behavior_proved"])

    def test_dangerous_calls_require_permission_thread_and_cleanup_context(self):
        # Removing any mutation permission, independent guard or cleanup gate must fail.
        records = self.run_lua(r'''
StaticConstructObject=function() calls=calls+1; return live() end
LoadAsset=function() calls=calls+1; return live(),true,true end
local c=config({"construct.umg","loadasset.fallback"})
c.context.construct={class=live(),outer=live(),name=live()}
c.context.asset={path="/Game/Test.Test_C"}
local s=assert(runner.new(c)); s:run(); s:finish(); assert(calls==0)
''')
        self.assertEqual(self.case(records, "construct.umg")["status"], "skipped")
        self.assertEqual(self.case(records, "loadasset.fallback")["status"], "skipped")
        records = self.run_lua(r'''
LoadAsset=function() calls=calls+1; return live(),true,true end
StaticFindObject=function() calls=calls+1;error("must not look up before a valid guard") end
local c=config({"loadasset.fallback"}); c.allow.load_asset=true
c.context.asset={path="/Game/Test.Test_C",cleanup_plan_id="fixture"}
c.thread_guard=function() return {kind="host-fixture",in_game_thread=false} end
local s=assert(runner.new(c)); s:run(); s:finish(); assert(calls==0)
''')
        self.assertEqual(self.case(records, "loadasset.fallback")["status"], "skipped")
        records = self.run_lua(r'''
StaticConstructObject=function() calls=calls+1;return live() end
LoadAsset=function() calls=calls+1;return live(),true,true end
StaticFindObject=function() calls=calls+1;return live() end
local c=config({"construct.umg","loadasset.fallback"});c.allow.construct=true;c.allow.load_asset=true
c.context.construct={class=live(),outer=live(),name=live(),cleanup_plan_id="fixture"}
c.context.asset={path="/Game/Test.Test_C",cleanup_plan_id="fixture"}
local s=assert(runner.new(c));s:run();s:finish();assert(calls==0)
''')
        self.assertEqual(self.case(records, "construct.umg")["status"], "skipped")
        self.assertEqual(self.case(records, "loadasset.fallback")["status"], "skipped")

    def test_unexpected_native_result_is_failed_not_missing_context(self):
        # After invoking an API, a wrong native result cannot be called a missing fixture.
        records = self.run_lua(r'''
FindFirstOf=function()calls=calls+1;return nil end
local c=config({"database.findfirst"});c.allow.engine_read=true;c.context.database=live()
local s=assert(runner.new(c));s:run();s:finish();assert(calls==1)
''')
        self.assertEqual(self.case(records, "database.findfirst")["status"], "failed")

    def test_engine_configuration_cannot_use_a_boolean_thread_claim(self):
        # Treating an unverified Lua boolean as native game-thread proof must fail.
        records = self.run_lua(r'''
FindFirstOf=function() calls=calls+1; return live() end
local c=config({"database.findfirst"}); c.allow.engine_read=true
c.evidence="engine"; c.side="ps5"
c.metadata.attestations={real_engine=true,release_immutable=true,
 independent_thread_witness=true,controlled_object_context=true}
c.thread_guard=function() return true end
c.context.database=live()
local s=assert(runner.new(c)); s:run(); s:finish(); assert(calls==0)
''')
        self.assertEqual(self.case(records, "database.findfirst")["status"], "skipped")

    def test_host_doubles_are_explicit_fixture_pass_only(self):
        # Promoting host doubles to proved engine behavior must fail.
        records = self.run_lua(r'''
local db=live(); FindFirstOf=function(name) assert(name=="ItemDatabaseSubsystem"); return db end
local c=config({"database.findfirst"}); c.allow.engine_read=true; c.context.database=db
local s=assert(runner.new(c)); s:run(); s:finish()
''')
        row = self.case(records, "database.findfirst")
        self.assertEqual(row["status"], "fixture-pass")
        self.assertEqual(row["evidence"], "host-fixture")
        self.assertFalse(records[0]["attestations"]["real_engine"])

    def test_writable_nested_values_are_snapshotted_read_back_and_restored(self):
        # Using a live alias as the saved value or forgetting nested restore must fail.
        records = self.run_lua(r'''
local widget=live(); widget.ShadowOffset={X=4,Y=5}; widget.Font={Size=12}
local c=config({"marshal.struct-write"}); c.allow.property_write=true
c.context.textblock=widget; c.context.textblock_owned=true
local s=assert(runner.new(c)); s:run(); s:finish()
assert(widget.ShadowOffset.X==4 and widget.ShadowOffset.Y==5 and widget.Font.Size==12)
''')
        row = self.case(records, "marshal.struct-write")
        self.assertEqual(row["status"], "fixture-pass")
        self.assertEqual(row["value"]["before"], {"shadow": {"X": 4, "Y": 5}, "font_size": 12})
        self.assertEqual(row["value"]["written"], {"shadow": {"X": 1, "Y": 1}, "font_size": 13})
        self.assertEqual(row["value"]["restored"], row["value"]["before"])

    def test_failed_write_readback_still_restores_original_values(self):
        # Returning early after failed verification and leaving a modified widget must fail.
        records = self.run_lua(r'''
local offset={X=4,Y=5}; local font={Size=12}; local reads=0
local widget=setmetatable(live(),{
 __index=function(_,key) if key=="ShadowOffset" then reads=reads+1; if reads==2 then error("readback failure") end; return offset end
                       if key=="Font" then return font end end,
 __newindex=function(_,key,value) if key=="ShadowOffset" then offset.X=value.X;offset.Y=value.Y else rawset(_,key,value) end end})
local c=config({"marshal.struct-write"}); c.allow.property_write=true
c.context.textblock=widget;c.context.textblock_owned=true
local s=assert(runner.new(c));s:run();s:finish()
assert(offset.X==4 and offset.Y==5 and font.Size==12)
''')
        row = self.case(records, "marshal.struct-write")
        self.assertEqual(row["status"], "failed")
        self.assertIn("readback failure", row["error"])
        self.assertTrue(row["value"]["cleanup_verified"])

    def test_cleanup_failure_is_preserved_as_failure(self):
        # A successful native return must not conceal failed cleanup.
        records = self.run_lua(r'''
StaticConstructObject=function() calls=calls+1; return live() end
local c=config({"construct.umg"});c.allow.construct=true
c.context.construct={class=live(),outer=live(),name=live(),cleanup_plan_id="owned-fixture"}
c.cleanup=function() error("fixture cleanup failed") end
local s=assert(runner.new(c));s:run();s:finish();assert(calls==1)
''')
        row = self.case(records, "construct.umg")
        self.assertEqual(row["status"], "failed")
        self.assertIn("fixture cleanup failed", row["error"])

    def test_registration_is_separate_from_delivered_callbacks(self):
        # Reporting registered/queued callbacks as delivered game-thread work must fail.
        records = self.run_lua(r'''
Key={F7=118,UP_ARROW=38,DOWN_ARROW=40,RETURN=13,LEFT_ARROW=37,RIGHT_ARROW=39}
RegisterKeyBind=function() end;RegisterLoadMapPreHook=function() end;ExecuteInGameThread=function() end
local c=config({"register_keys.accept","loadmap.accept","game_thread.delivery","register_keys.delivery","loadmap.delivery"})
c.allow.register_keys=true;c.allow.register_map_hook=true;c.allow.dispatch=true
c.context.registration_plan_id="isolated-fixture-session"
local s=assert(runner.new(c));s:run();s:finish()
''')
        self.assertEqual(self.case(records, "register_keys.accept")["status"], "fixture-pass")
        self.assertEqual(self.case(records, "game_thread.delivery")["status"], "skipped")
        self.assertEqual(self.case(records, "register_keys.delivery")["status"], "skipped")
        self.assertEqual(self.case(records, "loadmap.delivery")["status"], "skipped")

    def test_exact_trace_arguments_and_out_write_are_observed_with_context(self):
        # Losing the OUT table, argument order, or ignored-actor array must fail.
        records = self.run_lua(r'''
local pawn=live();local system=live()
system.LineTraceSingle=function(self,p,s,e,ch,complex,actors,debug,hit,ignore,c1,c2,time)
 assert(self==system and p==pawn and s.X==10 and e.Z==0 and ch==0 and complex==false)
 assert(#actors==1 and actors[1]==pawn and debug==0 and ignore==true and c1.A==0 and c2.A==0 and time==0)
 hit.ImpactPoint={X=10,Y=20,Z=0};hit.ImpactNormal={X=0,Y=0,Z=1};return true
end
local c=config({"marshal.trace-out"});c.allow.trace=true
c.context.trace={pawn=pawn,system=system,start={X=10,Y=20,Z=50},finish={X=10,Y=20,Z=0},
 expected={has_hit=true,point={X=10,Y=20,Z=0},normal={X=0,Y=0,Z=1}}}
local s=assert(runner.new(c));s:run();s:finish()
''')
        self.assertEqual(self.case(records, "marshal.trace-out")["status"], "fixture-pass")

    def test_callable_constructor_and_replace2_error_are_not_hidden(self):
        # Requiring type(function) for native callable constructors or swallowing errors must fail.
        records = self.run_lua(r'''
FName=setmetatable({}, {__call=function(_,text,mode)
 if mode==2 then error("invalid EFindName at 0x2345") end
 return {ToString=function()return text end}
end})
local c=config({"fname.add-default","fname.replace2"});c.allow.name_pool=true
c.context.name_plan_id="fixture-name-pool";c.context.known_name="ByteProperty"
local s=assert(runner.new(c));s:run();s:finish()
''')
        self.assertEqual(self.case(records, "fname.add-default")["status"], "fixture-pass")
        row = self.case(records, "fname.replace2")
        self.assertEqual(row["status"], "observed")
        self.assertIn("invalid EFindName at 0x2345", row["value"]["error"])

    def test_inert_stream_matches_frozen_manifest_and_remains_fixture_only(self):
        # Stale cases, malformed empty selections or promotion of an inert host run must fail.
        manifest=json.loads((BASE / "manifest.json").read_text())
        digest=hashlib.sha256((BASE / "conformance.lua").read_bytes()).hexdigest()
        self.assertEqual(manifest["script_sha256"],digest)
        for selected in ["nil", "{}"]:
            records=self.run_lua('local c=config('+selected+'); c.metadata.script_sha256="'+digest+'"; '
                                 'local s=assert(runner.new(c));s:run();s:finish()')
            self.assertEqual([r["id"] for r in records if r["kind"]=="case"],
                             [c["id"] for c in manifest["cases"]])
            self.assertEqual(records[-1]["case_count"],24)
            self.assertTrue(all(r["status"]=="skipped" for r in records if r["kind"]=="case"))
            self.assertIsInstance(records[0]["selected_case_ids"],list)
            with tempfile.TemporaryDirectory() as temp:
                path=Path(temp)/"fixture-only.jsonl"
                path.write_text("".join(json.dumps(r)+"\n" for r in records))
                result=subprocess.run(["python3",str(BASE/"compare.py"),"--manifest",str(BASE/"manifest.json"),
                                       "--reference",str(path),"--candidate",str(path)],capture_output=True,text=True)
                self.assertEqual(result.returncode,2,result.stderr)
                self.assertEqual(json.loads(result.stdout)["verdict"],"incomplete")


if __name__ == "__main__":
    unittest.main()
