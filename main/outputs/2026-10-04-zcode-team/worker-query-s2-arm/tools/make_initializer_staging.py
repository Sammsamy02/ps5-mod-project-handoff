#!/usr/bin/env python3
"""Generate the patched UnrealInitializer.cpp staging copy + review diff.

Base: the composed 'unreal-unrealinitializer' unit source (pinned at
sha256 66691a8860ce4230b65cfebc78b0e2e0fc7be283f86a24969261028e79b94b6b in the
composition base receipt). Every insertion is #ifdef __PROSPERO__-guarded so a
non-ProspERO compile of this file stays semantically identical. Anchors must
occur EXACTLY once. Writes only inside this worker output dir.
"""
import hashlib
import sys
from pathlib import Path

WHAT = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
SRC = WHAT / ".superpowers/native-dependency-spike/ue4ss-native-tick-initializer-retry-1/src/UnrealInitializer.cpp"
BASE_SHA = "66691a8860ce4230b65cfebc78b0e2e0fc7be283f86a24969261028e79b94b6b"
OUT = WHAT / "outputs/2026-10-04-zcode-team/worker-query-s2-arm"
STAGED = OUT / "staging/UnrealInitializer.cpp"
DIFF = OUT / "integration-patch/patches/01-unreal-initializer-s2-arm.diff"

DISARM_NOTE = (
    "            // Post-arm startup failure: close the gate first, then\n"
    "            // preserve the original failure (closed gate refuses and\n"
    "            // counts every later dispatch - honest negative evidence).\n"
)

text = SRC.read_text()
if hashlib.sha256(text.encode()).hexdigest() != BASE_SHA:
    sys.exit("base UnrealInitializer.cpp hash drift - refusing to patch")

def insert_after(anchor, addition, label):
    global text
    n = text.count(anchor)
    if n != 1:
        sys.exit(f"anchor {label}: expected 1 occurrence, found {n}")
    text = text.replace(anchor, anchor + addition)

def insert_before(anchor, addition, label):
    global text
    n = text.count(anchor)
    if n != 1:
        sys.exit(f"anchor {label}: expected 1 occurrence, found {n}")
    text = text.replace(anchor, addition + anchor)

# (1) include the harness contract header (Prospero composition only).
insert_after(
    "#include <Unreal/Hooks/Internal/DetourSubclasses.hpp>\n",
    "#ifdef __PROSPERO__\n#include <NativeQueryHarness.hpp>\n#endif\n",
    "include")

# (2) S2 arm line + ScanGame-level try. Placed after the whole
#     `#ifndef __PROSPERO__ ApplyDebugBuildOffsets(); #endif` block: on the
#     Prospero target that block preprocesses to NOTHING, so ArmLayoutGate is
#     still the statement immediately following InitializeVersionedContainer()
#     in the preprocessed serialized sequence (and the first statement after a
#     genuinely successful publish).
insert_after(
    "        InitializeVersionedContainer();\n"
    "#ifndef __PROSPERO__\n"
    "        // Exact native offsets already include target configuration effects.\n"
    "        ApplyDebugBuildOffsets();\n"
    "#endif\n",
    "#ifdef __PROSPERO__\n"
    "        // S2 ARM (worker-query-s2-arm). InitializeVersionedContainer()\n"
    "        // selects NativeProvidedVirtualVC on Prospero, whose\n"
    "        // set_virtual_offsets() calls\n"
    "        // PublishStagedNativeLayoutMaps((uint32_t)Version::Major,\n"
    "        // (uint32_t)Version::Minor, Version::NonShipping,\n"
    "        // Version::DebugBuild) and THROWS on every refusal mode. On\n"
    "        // Prospero this is the statement immediately after that publish\n"
    "        // call (the #ifndef offset block above preprocesses to nothing),\n"
    "        // so reaching it means the publish genuinely succeeded; arm the\n"
    "        // harness layout gate with exactly the consumed facts. Storage\n"
    "        // stays Unknown: the validated townfall-ps5-ue56 profile supplies\n"
    "        // no GUObjectArray storage claim (informational field only; the\n"
    "        // fixed/chunked dispatch is engine-version-driven).\n"
    "        RC::Platform::QueryHarness::Global().ArmLayoutGate({static_cast<uint32_t>(Version::Major), static_cast<uint32_t>(Version::Minor), static_cast<uint8_t>(Version::NonShipping ? 1 : 0), static_cast<uint8_t>(Version::DebugBuild ? 1 : 0), RC::Platform::QueryHarness::StorageLayout::Unknown, \"PublishStagedNativeLayoutMaps\"});\n"
    "        // Disarm-on-throw: any serialized-startup failure after a\n"
    "        // successful publish closes the gate before propagating.\n"
    "        try\n"
    "        {\n"
    "#endif\n",
    "arm+try")

# (3) close the ScanGame-level catch right after the last ScanGame statement.
insert_after(
    "        StaticStorage::bScanFullyCompleted = true;\n",
    "#ifdef __PROSPERO__\n"
    "        }\n"
    "        catch (...)\n"
    "        {\n"
    "            RC::Platform::QueryHarness::Global().DisarmLayoutGate();\n"
    "            throw;\n"
    "        }\n"
    "#endif\n",
    "scangame-catch")

# (4) Initialize-level try covering serialized startup after ScanGame().
insert_after(
    "        if (!StaticStorage::bScanFullyCompleted)\n"
    "        {\n"
    "            ScanGame();\n"
    "        }\n",
    "#ifdef __PROSPERO__\n"
    "        // The gate armed inside ScanGame() stays open for the remainder of\n"
    "        // serialized startup; any failure from here through engine\n"
    "        // readiness disarms before propagating (the wrapped region is not\n"
    "        // re-indented: byte-minimal reviewable diff against the pinned\n"
    "        // base, semantics unchanged).\n"
    "        try\n"
    "        {\n"
    "#endif\n",
    "initialize-try")

# (5) persistent per-tick harness dispatch callback registration.
insert_after(
    "            const auto initializer_callback = Hook::RegisterEngineTickPreCallback(HookedEngineTick, {false, false, STR(\"UE4SS\"), STR(\"GameThreadInitializer\")});\n"
    "            if (initializer_callback == Hook::ERROR_ID)\n"
    "            {\n"
    "                throw std::runtime_error(\"Native game-thread initializer callback registration failed\");\n"
    "            }\n",
    "            // Persistent harmless-query dispatch route (worker-query-s2-arm):\n"
    "            // the self-retiring GameThreadInitializer callback publishes the\n"
    "            // game-thread identity exactly once, but the query harness must\n"
    "            // observe EVERY genuine engine tick (two consecutive accepted\n"
    "            // reads). This callback never removes itself; refusal outcomes\n"
    "            // are counted inside the harness and never thrown from a Tick.\n"
    "            const auto query_dispatch_callback = Hook::RegisterEngineTickPreCallback(QueryHarnessEngineTickDispatch, {false, false, STR(\"UE4SS\"), STR(\"QueryHarnessDispatch\")});\n"
    "            if (query_dispatch_callback == Hook::ERROR_ID)\n"
    "            {\n"
    "                throw std::runtime_error(\"Query harness tick dispatch callback registration failed\");\n"
    "            }\n",
    "dispatch-register")

# (6) define the dispatch thunk right after HookedEngineTick.
insert_after(
    "        // Retire only after publication and any required callback retirement\n"
    "        // succeed. The callback remains owned by the forwarding hook snapshot.\n"
    "        CallbackIterationData.RemoveSelf();\n"
    "#endif\n"
    "    }\n",
    "\n"
    "#ifdef __PROSPERO__\n"
    "    // Per-tick entry for RC::Platform::QueryHarness: noexcept, no\n"
    "    // allocation, no I/O; all refusals are counted internally.\n"
    "    static auto QueryHarnessEngineTickDispatch([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] UEngine*, [[maybe_unused]] float, [[maybe_unused]] bool) -> void\n"
    "    {\n"
    "        RC::Platform::QueryHarness::Global().OnEngineTickDispatch();\n"
    "    }\n"
    "#endif\n",
    "dispatch-thunk")

# (7) close the Initialize-level catch right before its closing brace.
insert_before(
    "        PostInitialize(UnrealConfig);\n    }\n",
    "#ifdef __PROSPERO__\n"
    "        }\n"
    "        catch (...)\n"
    "        {\n"
    + DISARM_NOTE +
    "            RC::Platform::QueryHarness::Global().DisarmLayoutGate();\n"
    "            throw;\n"
    "        }\n"
    "#endif\n",
    "initialize-catch")

STAGED.parent.mkdir(parents=True, exist_ok=True)
STAGED.write_text(text)
print("staged sha256:", hashlib.sha256(text.encode()).hexdigest())
print("staged bytes:", len(text.encode()))

DIFF.parent.mkdir(parents=True, exist_ok=True)
import subprocess
r = subprocess.run(["diff", "-u", str(SRC), str(STAGED)], capture_output=True, text=True)
body = r.stdout.replace(str(SRC), "a/.superpowers/native-dependency-spike/ue4ss-native-tick-initializer-retry-1/src/UnrealInitializer.cpp")
body = body.replace(str(STAGED), "b/.superpowers/native-dependency-spike/ue4ss-native-tick-initializer-retry-1/src/UnrealInitializer.cpp")
DIFF.write_text(body)
print("diff lines:", len(body.splitlines()))
