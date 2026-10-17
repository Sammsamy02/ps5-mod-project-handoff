#!/usr/bin/env python3
"""worker-bridge-r13-polish: replicate the comp6 driver's build_overlay into
THIS worker's staging dir (real-build vintage headers from canonical sources),
so the r13 bridge TU can be syntax-gated with the exact dry-build prospero argv.
Mirrors worker-comp6-driver/build_runtime_composition6.py::build_overlay
(symlink lineage mirror + 31 staged headers + lifecycle units); no gates that
require the composition run are executed here."""
import json
import shutil
import sys
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
TEAM = ROOT / 'outputs/2026-10-04-zcode-team'
SPIKE = ROOT / '.superpowers/native-dependency-spike'
OWNER = SPIKE / 'ue4ss-native-tick-file-owner-1'
LINEAGE = SPIKE / 'native-full-runtime-aligned-entry-1'
CR1 = SPIKE / 'native-coherent-runtime-1'
UONB = SPIKE / 'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first'
TEAM_WIRING = TEAM / 'worker-module-start-wiring'
W = OWNER / 'include'

# verbatim from build_runtime_composition6.py DEFAULT_STAGE_HEADERS (30 rows
# here: the ODR Platform header source resolves to the port path, as the
# driver does when --odr-staged-header is unset)
DEFAULT_STAGE_HEADERS = [
    ('runtime_start_bridge.h', TEAM_WIRING / 'runtime_start_bridge.h'),
    ('modforge_activation.h', W / 'modforge_activation.h'),
    ('activated_mod_source.hpp', W / 'activated_mod_source.hpp'),
    ('live_module_facts.hpp', W / 'live_module_facts.hpp'),
    ('lmf_to_native_loader_facts.hpp', W / 'lmf_to_native_loader_facts.hpp'),
    ('NativeLoaderContext.hpp', W / 'NativeLoaderContext.hpp'),
    ('runtime_ack_supplier.hpp', TEAM / 'worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.hpp'),
    ('runtime_owner.hpp', W / 'runtime_owner.hpp'),
    ('ue4ss_backend.hpp', W / 'ue4ss_backend.hpp'),
    ('Helpers/Common.hpp', UONB / 'Helpers/include/Helpers/Common.hpp'),
    ('Helpers/UETargetModules.hpp', CR1 / 'source/port/first/include/Helpers/UETargetModules.hpp'),
    ('OwnedMappedCandidateSelection.hpp', W / 'OwnedMappedCandidateSelection.hpp'),
    ('OwnedMappedImageCaller.hpp', SPIKE / 'ue4ss-native-ps-scan-provider-1/include/OwnedMappedImageCaller.hpp'),
    ('OwnedMappedResolverCollector.hpp', SPIKE / 'ue4ss-native-mapped-candidate-selection-1/include/OwnedMappedResolverCollector.hpp'),
    ('OwnedMappedScanSource.hpp', W / 'OwnedMappedScanSource.hpp'),
    ('OwnedSerializedElfSource.hpp', W / 'OwnedSerializedElfSource.hpp'),
    ('Platform/NativeModSource.hpp', CR1 / 'source/port/ue4ss/include/Platform/NativeModSource.hpp'),
    ('Platform/NativeProgramContext.hpp', CR1 / 'source/port/ue4ss/include/Platform/NativeProgramContext.hpp'),
    ('PsMappedImageCaller.hpp', SPIKE / 'ue4ss-native-ps-scan-abi-1/include/PsMappedImageCaller.hpp'),
    ('PsScanAbi.h', SPIKE / 'ue4ss-native-ps-scan-abi-1/include/PsScanAbi.h'),
    ('SigScanner/Common.hpp', UONB / 'SinglePassSigScanner/include/SigScanner/Common.hpp'),
    ('SigScanner/SinglePassSigScanner.hpp', CR1 / 'source/port/first/include/SigScanner/SinglePassSigScanner.hpp'),
    ('Unreal/PlatformModuleSnapshot.hpp', CR1 / 'source/port/unreal/include/Unreal/PlatformModuleSnapshot.hpp'),
    ('live_module_base_scan.hpp', W / 'live_module_base_scan.hpp'),
    ('live_module_os_ops.hpp', W / 'live_module_os_ops.hpp'),
    ('patternsleuth_mapped_candidates.h', SPIKE / 'ue4ss-native-mapped-candidates-1/include/patternsleuth_mapped_candidates.h'),
    ('polyhook2/Enums.hpp', SPIKE / 'ue4ss-unreal-native-build-1/source-dependencies/polyhook/polyhook2/Enums.hpp'),
    ('polyhook2/OwnedMemory.hpp', SPIKE / 'ue4ss-native-read-lease-1/include/polyhook2/OwnedMemory.hpp'),
    ('polyhook2/PolyHookOs.hpp', SPIKE / 'ue4ss-polyhook-platform-port-1/patched/include/polyhook2/PolyHookOs.hpp'),
    ('sce_module_abi.hpp', W / 'sce_module_abi.hpp'),
]

def main() -> None:
    my = TEAM / 'worker-bridge-r13-polish'
    overlay = my / 'staging/lineage-overlay-2-r13'
    if overlay.exists():
        shutil.rmtree(overlay)
    overlay.mkdir(parents=True)
    for child in LINEAGE.iterdir():
        if child.name == 'lifecycle':
            continue
        (overlay / child.name).symlink_to(child)
    lc = overlay / 'lifecycle'
    lc.mkdir()
    for child in (LINEAGE / 'lifecycle').iterdir():
        (lc / child.name).symlink_to(child)
    manifest = {'headers': {}, 'units': {}}
    import hashlib
    def sha(p: Path) -> str:
        h = hashlib.sha256()
        h.update(p.read_bytes())
        return h.hexdigest()
    for rel, src in DEFAULT_STAGE_HEADERS:
        dest = overlay / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        manifest['headers'][rel] = {'source': str(src), 'sha256': sha(src)}
    entry = TEAM_WIRING / 'runtime_module_entry_r4.c'
    bridge = TEAM_WIRING / 'runtime_start_bridge_r13.cpp'
    shutil.copy2(entry, lc / 'runtime_module_entry.c')
    (lc / 'runtime_module_entry.c.sha256').write_text(sha(entry) + '\n')
    manifest['units']['lifecycle/runtime_module_entry.c'] = {'source': str(entry), 'sha256': sha(entry)}
    shutil.copy2(bridge, lc / 'runtime_start_bridge.cpp')
    manifest['units']['lifecycle/runtime_start_bridge.cpp'] = {'source': str(bridge), 'sha256': sha(bridge)}
    (my / 'staging/overlay-manifest-r13.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f"overlay ready: {overlay} ({len(manifest['headers'])} headers, {len(manifest['units'])} units)")

if __name__ == '__main__':
    sys.exit(main())
