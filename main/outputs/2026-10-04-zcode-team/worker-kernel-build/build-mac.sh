#!/bin/bash
# build-mac.sh — macOS (arm64) reproduction of build/build-kstuff.ps1 for kstuffSharp.
#
# Mirrors the three stages of the official PowerShell script:
#   1. Build the installer payload (NativeAOT compile linux-x64 + payload link).
#   2. Regenerate samples/kstuffSharp/InstallerBlob.g.cs from the installer ELF.
#   3. Build the loader payload (embeds the installer bytes).
#
# Differences vs the Windows/Linux original (documented in BUILD-GUIDE.md):
#   - bash instead of Windows PowerShell 5.1 (`pwsh` is not installed on this Mac
#     and build-kstuff.ps1 hardcodes `& powershell`, which does not exist on macOS).
#   - `-p:StripSymbols=false` on `dotnet publish`: the NativeAOT Unix targets abort
#     before the ilc step when llvm-objcopy is absent; the SharpProspero pipeline
#     only consumes the emitted .o, so stripping is irrelevant here.
#   - The publish's final host steps do NOT fail in this configuration
#     (OutputType=Library + NativeLib=Static), so exit code 0 is normal on macOS.
#
# Toolchain: the pinned local .NET SDK 10.0.401 osx-arm64 from the
# sharpprospero-host-tool-1 spike, with an isolated CLI home and NuGet cache.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/build-env/env.sh"

SRC="${W:?}/source"
CONFIG=Release
RID=linux-x64
GEN="$SRC/tools/SharpProspero.Bindings.Generator/SharpProspero.Bindings.Generator.csproj"

# --- helpers ---------------------------------------------------------------

# Runtime-pack version as recorded by restore (same logic as build-app.ps1:
# read downloadDependencies from obj/project.assets.json rather than guessing
# the newest cached folder).
pack_version() {
  python3 - "$1/obj/project.assets.json" <<'PY'
import json, sys
assets = json.load(open(sys.argv[1]))
name = "Microsoft.NETCore.App.Runtime.NativeAOT.linux-x64"
for fw in assets.get("project", {}).get("frameworks", {}).values():
    for dep in fw.get("downloadDependencies", []):
        if dep["name"] == name:
            rng = dep["version"].strip("[] ")
            print(rng.split(",")[0]); sys.exit(0)
sys.exit("runtime pack not recorded in assets")
PY
}

# Gather the runtime-support objects/archives the payload linker needs
# (same list and rules as build-app.ps1 -Payload: libbootstrapper.o for an
# executable-kind payload; libbootstrapperdll.o excluded).
gather_support() {
  local projdir="$1" ver native_dir support
  ver="$(pack_version "$projdir")"
  native_dir="$NUGET_PACKAGES/microsoft.netcore.app.runtime.nativeaot.linux-x64/$ver/runtimes/$RID/native"
  support="$projdir/obj/$CONFIG/net10.0/$RID/runtime-support"
  mkdir -p "$support"
  local names=(libbootstrapper.o libRuntime.WorkstationGC.a libRuntime.VxsortDisabled.a
    libeventpipe-disabled.a libstandalonegc-disabled.a libaotminipal.a libstdc++compat.a
    libSystem.Native.a libz.a libbrotlienc.a libbrotlidec.a libbrotlicommon.a
    libSystem.IO.Compression.Native.a)
  local n
  for n in "${names[@]}"; do
    if [ -f "$native_dir/$n" ]; then cp -f "$native_dir/$n" "$support/"; fi
  done
  local gathered
  gathered="$(ls -A "$support" 2>/dev/null || true)"
  [ -n "$gathered" ] || { echo "no runtime archives gathered" >&2; exit 1; }
  printf '    %s\n' $gathered
}

link_payload() { # link_payload <project-dir> <target-name> <elf-out> <sprx...::kernel-sprx>
  local projdir="$1" target="$2" elf="$3"; shift 3
  local obj support
  obj="$projdir/obj/$CONFIG/net10.0/$RID/native/$target.o"
  [ -f "$obj" ] || { echo "missing $obj (run the publish first)" >&2; exit 1; }
  support="$projdir/obj/$CONFIG/net10.0/$RID/runtime-support"

  local args=(link --kind payload --self-contained --obj "$obj")
  local a
  for a in "$@"; do
    case "$a" in
      kernel:*) args+=(--kernel-sprx "${a#kernel:}");;
      *)        args+=(--sprx "$a");;
    esac
  done
  for a in "$support"/*.o; do
    [ "$(basename "$a")" = libbootstrapperdll.o ] && continue
    args+=(--obj "$a")
  done
  for a in "$support"/*.a; do args+=(--lib "$a"); done
  args+=(--out "$elf")
  mkdir -p "$(dirname "$elf")"
  dotnet run --project "$GEN" -c $CONFIG -- "${args[@]}"
  [ -f "$elf" ] || { echo "payload link did not produce $elf" >&2; exit 1; }
}

build_payload() { # build_payload <csproj> <sprx flags...>
  local csproj="$1"; shift
  local projdir target support
  projdir="$(dirname "$csproj")"
  target="$(basename "$csproj" .csproj)"
  echo "== Compile ($target) =="
  dotnet publish "$csproj" -c $CONFIG -r $RID "-p:SharpProsperoRoot=$SRC" \
    -p:StripSymbols=false --nologo
  local obj="$projdir/obj/$CONFIG/net10.0/$RID/native/$target.o"
  [ -f "$obj" ] || { echo "no compiled object at $obj" >&2; exit 1; }
  echo "  Compiled $target.o for $RID."
  echo "== Runtime support ($target) =="
  gather_support "$projdir"
  support="$projdir/obj/$CONFIG/net10.0/$RID/runtime-support"
  echo "== Link (payload, $target) =="
  link_payload "$projdir" "$target" "$projdir/out/$target.elf" "$@"
}

# --- stage 1: installer payload --------------------------------------------
echo "==================== 1/3 Installer payload ===================="
build_payload "$SRC/samples/kstuffSharp-installer/InstallerApp.csproj" \
  libSceLibcInternal kernel:libkernel_sys.sprx

# --- stage 2: embed installer bytes into the loader -------------------------
echo "==================== 2/3 Embed installer ======================"
python3 - "$SRC/samples/kstuffSharp-installer/out/InstallerApp.elf" \
          "$SRC/samples/kstuffSharp/InstallerBlob.g.cs" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
out = ["// Auto-generated. Do not edit.", "namespace SampleApp;",
       "internal static class InstallerBlob", "{",
       "    public static System.ReadOnlySpan<byte> Data => new byte[]", "    {"]
# 32 bytes per line, same layout as build-kstuff.ps1
for i in range(0, len(data), 32):
    chunk = data[i:i+32]
    out.append("        " + ",".join(f"0x{b:02X}" for b in chunk) + ",")
out += ["    };", "}"]
open(sys.argv[2], "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
print(f"  Embedded {len(data)} bytes into {sys.argv[2]}")
PY

# --- stage 3: loader payload -------------------------------------------------
echo "==================== 3/3 Loader payload ========================"
build_payload "$SRC/samples/kstuffSharp/SampleApp.csproj" \
  libSceLibcInternal libSceSysmodule kernel:libkernel_sys.sprx

echo "==================== Complete =================================="
ls -l "$SRC/samples/kstuffSharp-installer/out/InstallerApp.elf" \
      "$SRC/samples/kstuffSharp/out/SampleApp.elf"
