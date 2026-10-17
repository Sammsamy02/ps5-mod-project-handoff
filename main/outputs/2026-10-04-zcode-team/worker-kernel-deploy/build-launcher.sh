#!/bin/bash
# Build only the LauncherApp payload (stage-1 style, adapted from build-mac.sh)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/build-env/env.sh"
SRC="${W:?}/source"
CONFIG=Release
RID=linux-x64
GEN="$SRC/tools/SharpProspero.Bindings.Generator/SharpProspero.Bindings.Generator.csproj"

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

projdir="$SRC/samples/launcherapp"
target=LauncherApp

echo "== Compile =="
dotnet publish "$projdir/LauncherApp.csproj" -c $CONFIG -r $RID "-p:SharpProsperoRoot=$SRC" \
  -p:StripSymbols=false --nologo
obj="$projdir/obj/$CONFIG/net10.0/$RID/native/$target.o"
[ -f "$obj" ] || { echo "no compiled object at $obj"; exit 1; }
echo "  Compiled $target.o"

echo "== Runtime support =="
ver="$(pack_version "$projdir")"
native_dir="$NUGET_PACKAGES/microsoft.netcore.app.runtime.nativeaot.linux-x64/$ver/runtimes/$RID/native"
support="$projdir/obj/$CONFIG/net10.0/$RID/runtime-support"
mkdir -p "$support"
names=(libbootstrapper.o libRuntime.WorkstationGC.a libRuntime.VxsortDisabled.a
  libeventpipe-disabled.a libstandalonegc-disabled.a libaotminipal.a libstdc++compat.a
  libSystem.Native.a libz.a libbrotlienc.a libbrotlidec.a libbrotlicommon.a
  libSystem.IO.Compression.Native.a)
for n in "${names[@]}"; do
  [ -f "$native_dir/$n" ] && cp -f "$native_dir/$n" "$support/"
done
ls -A "$support" >/dev/null || { echo "no runtime archives"; exit 1; }

echo "== Link =="
args=(link --kind payload --self-contained --obj "$obj")
args+=(--sprx libSceLibcInternal)
args+=(--sprx libSceSystemService)
args+=(--kernel-sprx libkernel_sys.sprx)
for a in "$support"/*.o; do
  [ "$(basename "$a")" = libbootstrapperdll.o ] && continue
  args+=(--obj "$a")
done
for a in "$support"/*.a; do args+=(--lib "$a"); done
args+=(--out "$projdir/out/$target.elf")
mkdir -p "$projdir/out"
dotnet run --project "$GEN" -c $CONFIG -- "${args[@]}"
[ -f "$projdir/out/$target.elf" ] || { echo "no elf produced"; exit 1; }
echo "== OK: $projdir/out/$target.elf =="
