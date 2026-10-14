#pragma once
// CAP13 managed route — host-shipped managed images. PLACEHOLDER STAGE.
//
// This slot holds the BYTE-EXACT, hash-pinned images of Mbm.ModSdk.dll and
// Mbm.ModLoader.dll, generated exactly the way cap10's ProbeImage.hpp is
// generated from the real compiled Probe.dll:
//   1. compile managed/Mbm.ModSdk.cs and managed/ModLoader.cs with the pinned
//      cap10 recipe (Roslyn csc, /nostdlib+ /noconfig, Mono v4.7.2 reference
//      mscorlib) — see cap10/evidence/COMPILE.json for the proven recipe;
//   2. run the cap10 tools/assembly_preflight.py discipline on each artifact
//      (mscorlib-only closure check: any System.Core/System.dll reference
//      must FAIL the preflight — design section 3a constraint);
//   3. regenerate this header from the exact bytes + sha256 of both DLLs,
//      and record both hashes in the rung receipt (M2, design section 5.1).
//
// Until that happens this placeholder EXPOSES NO BYTES. open_admitted_image
// refuses size-0 inputs, so host_start fails cleanly at stage 7/8 with
// CAP13_STAGE — a visible, precise refusal, never a fake success.
#include "MbmManagedBridge.hpp"
#if defined(__PROSPERO__) || defined(MODBRIDGE_CAP13_MANAGED_HOST_VALIDATION)
namespace RC::Cap13Managed
{
    namespace ModSdkImage
    {
        // PLACEHOLDER: regenerate from the real Mbm.ModSdk.dll compile.
        // Expected identity once generated: sha256 recorded in the M2 receipt;
        // image name becomes "modbridge-cap13-<sha8>-Mbm.ModSdk.dll".
        inline ModImageInput input()
        {
            ModImageInput in{};
            in.assembly_name = "Mbm.ModSdk";
            in.sha8 = "00000000"; // placeholder — never admitted while fake
            in.bytes = nullptr;   // FAIL CLOSED: no bytes staged yet
            in.size = 0;
            return in;
        }
    }
    namespace ModLoaderImage
    {
        // PLACEHOLDER: regenerate from the real Mbm.ModLoader.dll compile.
        inline ModImageInput input()
        {
            ModImageInput in{};
            in.assembly_name = "Mbm.ModLoader";
            in.sha8 = "00000000"; // placeholder — never admitted while fake
            in.bytes = nullptr;   // FAIL CLOSED: no bytes staged yet
            in.size = 0;
            return in;
        }
    }
}
#endif
