"""Read-only validation of the retained matched archive codec closure.

This selects exact libarchive 3.8.7, XZ 5.8.4 and zlib 1.3.2 artifacts. It
builds, extracts, downloads and executes nothing, and returns no crypto owner.
Application consumers must use their selected curl/MbedTLS compile contract.
"""
import hashlib
import json
from pathlib import Path

DESCRIPTOR_SHA = "5a5b500101ec8a98b281f667dd63c9e07ab6901c466519631789c4174d2038bc"
NOTICES = {
    "LIBARCHIVE-COPYING": ("libarchive-release/COPYING", "30e556b3959e3985d66efefec5eaac51d4995053caa1d3cffe6eb916f146f229"),
    "XZ-COPYING": ("native-libarchive-lzma-experiment-1/sources/xz-5.8.4/COPYING", "616a3ad264ce29b8f1cb97e53037b139d406899ca8d1f799651e17bfa09830b8"),
    "XZ-COPYING-0BSD": ("native-libarchive-lzma-experiment-1/sources/xz-5.8.4/COPYING.0BSD", "0b01625d853911cd0e2e088dcfb743261034a091bb379246cb25a14cc4c74bf1"),
    "ZLIB-LICENSE": ("zlib-1.3.2-source/LICENSE", "e32ff4e00d9d94930537635291da39e7e612703334bf6fde8c7f1686fe8a45a2"),
}


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as incoming:
        for block in iter(lambda: incoming.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def prepare_archive(mode, profile, *, sdk=None, llvm_bin=None):
    """Validate the pinned family and return consumer flags/libs/identity/notices.

    profile is the explicit native-libarchive-lzma-experiment-1 directory.
    Retained absolute paths must bind to that actual family and its siblings;
    a relocated descriptor cannot cause unverified original inputs to be used.
    Native mode requires the same supplied SDK/LLVM roots as the retained proof.
    Returned archives are codecs only, ordered libarchive, liblzma, zlib. No
    legacy Mbed archive, caller/main, materializer or vendor mutation is added.
    This artifact validation is not native execution or a decoder heap bound.
    """
    if mode not in ("host", "native"):
        raise ValueError("archive dependency mode must be host or native")
    profile = Path(profile)
    if profile.is_symlink() or not profile.is_dir():
        raise ValueError("archive dependency profile must be a real directory")
    profile = profile.resolve()
    inputs, tool_aliases = {}, {}

    def checked(path, expected, size=None):
        path = Path(path)
        if path.is_symlink() or not path.is_file():
            raise ValueError("archive dependency is not a regular input: " + str(path))
        path = path.resolve()
        actual_size = path.stat().st_size
        if actual_size > 1024 * 1024 * 1024 or (size is not None and actual_size != size):
            raise ValueError("archive dependency input size mismatch: " + str(path))
        actual = digest(path)
        if actual != expected:
            raise ValueError("archive dependency input identity mismatch: " + str(path))
        inputs[str(path)] = {"bytes": actual_size, "sha256": actual}
        return path

    descriptor = checked(profile / "closure-3/descriptor.json", DESCRIPTOR_SHA)
    retained = json.loads(descriptor.read_text())
    if (retained["status"] != "PASS" or retained["native_executed"] or
            retained["external_program_symbols_linked"] or
            retained["compiler_arithmetic_helpers_imported"] or
            not retained["source_inputs_still_archive_corresponding"]):
        raise ValueError("archive retained proof is not the approved codec closure")

    def row(value, expected_path=None, tool_root=None):
        path = Path(value["path"])
        if expected_path is not None and path.resolve() != Path(expected_path).resolve():
            raise ValueError("archive dependency retained root mismatch: " + str(path))
        if tool_root is not None:
            # The pinned LLVM distribution uses clang/ld.lld/ranlib aliases.
            # Only these explicitly captured tool rows may follow an alias,
            # and its verified regular target must remain in that tool bin.
            target = path.resolve()
            if target.parent != Path(tool_root).resolve():
                raise ValueError("archive dependency tool target leaves its bin root")
            if path.is_symlink():
                tool_aliases[str(path)] = str(target)
            path = target
        return checked(path, value["sha256"], value["bytes"])

    # Bind supplied roots before consuming any recorded absolute archive paths.
    row(retained["source_acquisition"], profile / "source-acquisition.json")
    row(retained["recipe"], profile / "recipe.md")
    target = retained["targets"][mode]
    family = profile / mode
    archives = [row(target["libarchive"], family / "libarchive-build/.libs/libarchive.a"),
                row(target["liblzma"], family / "xz-build/src/liblzma/.libs/liblzma.a"),
                row(target["zlib"], profile.parent / "native-zip-experiment-1" / mode / "zlib-build/libz.a")]
    row(target["libarchive_config"], family / "libarchive-build/config.h")
    row(target["liblzma_config"], family / "xz-build/config.h")
    if target["link_order"] != ["libarchive", "liblzma", "zlib"]:
        raise ValueError("archive codec link order mismatch")
    include = profile.parent / "libarchive-release/libarchive"
    lzma_include = profile / "sources/xz-5.8.4/src/liblzma/api"
    if (Path(target["libarchive_headers"]).resolve() != include.resolve() or
            Path(target["liblzma_headers"]).resolve() != lzma_include.resolve() or
            [Path(p).resolve() for p in target["zlib_headers"]] !=
            [(profile.parent / "native-zip-experiment-1" / mode / "zlib-build").resolve(),
             (profile.parent / "zlib-1.3.2-source").resolve()]):
        raise ValueError("archive public include roots mismatch")
    header_paths = [include / "archive.h", include / "archive_entry.h", lzma_include / "lzma.h"]
    for header in header_paths:
        row(retained["headers"][str(header)], header)
    row(retained[mode + "_proof"])

    if mode == "native":
        if sdk is None or llvm_bin is None:
            raise ValueError("native archive validation needs explicit SDK and LLVM roots")
        sdk, llvm_bin = Path(sdk).resolve(), Path(llvm_bin).resolve()
        for value in retained["sdk_link_inputs"]:
            if not Path(value["path"]).is_relative_to(sdk):
                raise ValueError("native archive SDK root differs from supplied SDK")
            row(value)
        for value in retained["verified_llvm_tools"]:
            name = Path(value["path"]).name
            # ar/ranlib built the retained codec archives from a separately
            # captured copy of the same LLVM release. Consumer compiler/linker
            # tools must bind to the explicitly supplied LLVM bin instead.
            tool_root = (profile.parent / "tools/LLVM-21.1.8-macOS-ARM64/bin"
                         if name in ("llvm-ar", "llvm-ranlib") else llvm_bin)
            row(value, tool_root / name, tool_root=tool_root)
        for value in retained["used_native_header_correspondence"]:
            row(value)

    notices = {}
    for name, (relative, sha) in NOTICES.items():
        path = checked(profile.parent / relative, sha)
        notices[name] = {"path": str(path), **inputs[str(path)]}
    identity = {"schema_version": 1, "mode": mode, "descriptor_sha256": DESCRIPTOR_SHA,
                "inputs": dict(sorted(inputs.items())),
                "tool_aliases": dict(sorted(tool_aliases.items())),
                "reader_registry": "explicit_builtin_zip_tar_7zip_rar_rar5_none_gzip_xz",
                "additional_crypto_owner": False, "native_executed": False,
                "hard_decoder_heap_bound": False, "hard_parser_CPU_bound": False}
    return {"identity": identity,
            "include_dirs": [str(include)],
            "cflags": ["-DMB_ARCHIVE_BUILTIN_XZ=1", "-I" + str(include)],
            "link_archives": [str(path) for path in archives],
            "notices": notices,
            "attributions": [
                {"project": "libarchive", "version": "3.8.7", "license": "BSD-2-Clause",
                 "source": "https://github.com/libarchive/libarchive/tree/ded82291ab41d5e355831b96b0e1ff49e24d8939"},
                {"project": "XZ liblzma", "version": "5.8.4", "license": "0BSD",
                 "source": "https://github.com/tukaani-project/xz/tree/d3e650e63c110e830fd5391e7f8b45df0b91d3da"},
                {"project": "zlib", "version": "1.3.2", "license": "Zlib",
                 "source": "https://github.com/madler/zlib/tree/v1.3.2"}]}
