"""Prepare the explicit retained curl 8.18/MbedTLS download dependency.

Only the two initialization-failure translation units are rebuilt. All vendor
sources, baseline archives, compiler wrappers and retained consumers stay read
only. This is a pinned artifact reuse route, not a clean dependency rebuild or
target execution. The caller must select this profile explicitly.
"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import shutil
import stat
import struct
import subprocess
import tarfile

HERE = Path(__file__).resolve().parent
NATIVE_HOST = HERE.parent
CONFIG_ID = "86adf3460965c831f05d27f490a309490a8a13efdb9d5fdb6eff453008e8d73f"
CURL_PIN = "2eebc58c4b8d68c98c8344381a9f6df4cca838fd"
MBED_PIN = "068ff080b369adfac81509f9b57b2afabaf82dc5"
FRAMEWORK_PIN = "dde0c4a0e448a0552f18817dcea633bb851fd288"
CURL_RELEASE_SHA = "40df79166e74aa20149365e11ee4c798a46ad57c34e4f68fd13100e2c9a91946"
MBED_RELEASE_SHA = "ca6bd316bbec49ef20088f39b8755fcaec0b7e45781506de55b17cd191ae6937"
PATCH_SHA = "5c0f9f334f679c6592b1fd3c2b269669e72060223992a399cd581eb10250c597"
CONFIG_HASHES = {
    "tls_config.h": "06e55140cd8847c716bef7b9cab079bd5c914c48c86ae39f2d17dfd6900b91b4",
    "psa_config.h": "72bfcc53d83c2f9f4c9cf0ed0a1d5c7570dad32db530eef23e83a34398e730b6",
    "tls_effective.h": "ffa4ac3ecc2af3cfdc90d39f9de0fd2764d32c81aff2f06a75cd5d0e9c69c16e",
    "clock_decl.h": "3b84248d8ed3c6682e1a823588821dfb7d9b613606b031846bbd9ec6a3726654",
}
SOURCE_HASHES = {
    "mbedtls.c": "8127975c23d0478a98c31d6f184758bc2845f177e2b0932effb537e3336c16d6",
    "vtls.c": "9d2d6bfa917ee33b45065a19d583456abc45ca3ca852917657725265dbba2b29",
}
MODE_HASHES = {
    "native": {
        "curl": "5934a99a9ad6ba9664f95910c1b3519d9f0079238407afa9ba1ca7f6323a259a",
        "mbedtls": "414e80fc0c41c957bd4ba24dace4b6dcf51e1c6de2abb68e606e6d28752d8fa5",
        "mbedx509": "26b80624f9365993e01808a033bb4dff40911c750ef81e374cefed8166a356b1",
        "mbedcrypto": "9af459de0ec0037f00ef8447063c2ca32a4df92fb10f77c7b77c3daefa14b17a",
        "curl_config": "ab4702852ff137d9666a61a997376d2fb7c908548e77e091cbb5f24b39975c95",
        "wrapper": "824b2fba090bd1016683a959a502f1921f6a56c2c357c3a546e0ea61dca6418f",
        "commands": "196de316b67df3463d6dfcfed6edb3a7f153286b13a822f710c53b23a784dfe4",
    },
    "host": {
        "curl": "20b66918c4ab6d63b767f4e9fb3e10f839c1a510876de4ee5a3082af54f48bb6",
        "mbedtls": "a458acfedfc50f8c1a82585ad9b4c774a53cfb21e9e237f784a38c252135a03a",
        "mbedx509": "f8bfeba026eb965ede298c8a977582a89ac4411e3e1803da7cdceff74c4ea9f1",
        "mbedcrypto": "c098df0720aa02bf07f9f7e236a96e2300c4621e22f14e38e6c4b9fdb7559b79",
        "curl_config": "b5ef284d20c621d10892526011f95163639547fa740a4feb3623de85a70e3000",
        "wrapper": "b16d393e297fcd6df476a21e05df40fbbc4e9ad1d83e5549bb8793f2cc802628",
        "commands": "50fba198327fac1886d024ed1e630c66fe11f851815b59cfb43439765273589d",
    },
}
NOTICE_HASHES = {
    "CURL-LICENSE": "e18f1989333b70044b2adfb7dc2f905d0119dbdcac3bc9f4bc9d540e3a29de5b",
    "MBEDTLS-LICENSE": "9b405ef4c89342f5eae1dd828882f931747f71001cfba7d114801039b52ad09b",
    "FRAMEWORK-LICENSE": "11402351e38392230bb8934ba1095c0c0049a296c0f8821f76e4672dff54b490",
}


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def _checked(path, expected, label):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or digest(path) != expected:
        raise ValueError("download dependency identity mismatch: " + label)
    return path.resolve()


def _crypto_validator():
    spec = importlib.util.spec_from_file_location(
        "modbridge_download_crypto_validator", NATIVE_HOST / "crypto_build.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _vendor_files(root):
    paths = [p for tree in (root / "include", root / "lib", root / "library")
             if tree.is_dir() for p in tree.rglob("*")
             if p.is_file() and p.suffix in (".h", ".c")]
    if not paths or len(paths) > 3000:
        raise ValueError("download vendor source closure exceeds bounds")
    for path in paths:
        if path.is_symlink() or path.stat().st_size > 16 * 1024 * 1024:
            raise ValueError("download vendor source is a link or oversized")
    return {p.relative_to(root).as_posix(): p for p in paths}


def _verify_vendor(root, archive, expected):
    """Compare actual compile inputs with a fixed release, without extraction."""
    _checked(archive, expected, "vendor release archive")
    wanted = _vendor_files(root)
    matched, seen, total = {}, set(), 0
    with tarfile.open(archive, "r:*") as incoming:
        prefix = None
        for member in incoming:
            parts = Path(member.name).parts
            if (not parts or member.name.startswith("/") or ".." in parts or
                    "\\" in member.name or member.name in seen):
                raise ValueError("download release archive has unsafe metadata")
            seen.add(member.name)
            total += member.size
            if len(seen) > 12000 or member.size > 16 * 1024 * 1024 or total > 256 * 1024 * 1024:
                raise ValueError("download release archive exceeds bounds")
            if prefix is None:
                prefix = parts[0]
            if parts[0] != prefix:
                raise ValueError("download release archive root mismatch")
            relative = "/".join(parts[1:])
            if relative not in wanted:
                continue
            if not member.isfile():
                raise ValueError("download vendor input is not a regular archive member")
            data = incoming.extractfile(member).read()
            actual = digest(wanted[relative])
            if hashlib.sha256(data).hexdigest() != actual:
                raise ValueError("download vendor source identity mismatch: " + relative)
            matched[relative] = actual
    if set(matched) != set(wanted):
        raise ValueError("download vendor input absent from pinned release")
    return matched


def _run(argv, output, commands, env):
    result = subprocess.run([str(x) for x in argv], cwd=output, env=env,
                            capture_output=True, text=True, timeout=60)
    commands.append({"argv": [str(x) for x in argv], "returncode": result.returncode,
                     "stdout": result.stdout, "stderr": result.stderr})
    (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    if result.returncode:
        raise ValueError("download dependency command failed; see " +
                         str(output / "commands.json"))
    return result.stdout


def _abi(macros):
    entries = {line.split()[1]: " ".join(line.split()[2:])
               for line in macros.splitlines() if line.startswith("#define ")}
    names = ("__x86_64__", "__ELF__", "__FreeBSD__", "__LP64__",
             "__SIZEOF_POINTER__", "__SIZEOF_LONG__", "__SIZEOF_SIZE_T__",
             "__BYTE_ORDER__", "__SSE2__")
    return {name: entries.get(name) for name in names}


def _includes(dep, mode, roots):
    entries = []
    names = shlex.split(dep.read_text().replace("\\\n", " ").split(":", 1)[1])
    for name in dict.fromkeys(names):
        path = Path(name).resolve()
        origin = next((label for root, label in roots if path.is_relative_to(root)), "host")
        if mode == "native" and origin == "host":
            raise ValueError("download native include closure contains a host header")
        entries.append({"path": str(path), "origin": origin, "sha256": digest(path)})
    return entries


def prepare_download(mode, output_root, profile, curl_source, *, sdk=None, llvm_bin=None):
    """Return the pinned, backported curl archive and matching consumer settings.

    output_root must be new and outside every retained dependency input. Native
    mode requires explicit SDK/LLVM locations. Neither mode links the retained
    experiment's entropy.o, clock_policy.o or one-shot consumer.
    """
    if mode not in MODE_HASHES:
        raise ValueError("download dependency mode must be host or native")
    profile, source = Path(profile).resolve(), Path(curl_source).resolve()
    output = Path(output_root).absolute()
    if output.is_symlink() or output.exists():
        raise ValueError("download dependency output must be fresh")
    output = output.resolve()
    mbed = profile.parent / ("mbedtls-" + MBED_PIN)
    retained = profile / mode
    for tree in (profile, source, mbed):
        if output == tree or output.is_relative_to(tree):
            raise ValueError("download output must be outside retained inputs")
    hashes = MODE_HASHES[mode]
    originals = {name: _checked(source / "lib/vtls" / name, expected, name)
                 for name, expected in SOURCE_HASHES.items()}
    config = {name: _checked(profile / name, expected, name)
              for name, expected in CONFIG_HASHES.items()}
    wrapper = _checked(retained / "cc.py", hashes["wrapper"], "compiler wrapper")
    if not wrapper.stat().st_mode & stat.S_IXUSR:
        raise ValueError("download compiler wrapper is not executable")
    curl_config = _checked(retained / "curl-build/lib/curl_config.h",
                           hashes["curl_config"], "curl generated configuration")
    command_file = _checked(retained / "curl-commands.json", hashes["commands"],
                            "retained compiler command")
    command_input = json.loads(command_file.read_text())
    # The retained wrapper embeds absolute forced-include paths. Matching its
    # bytes is insufficient if a copied profile would execute it against the
    # original, unvalidated tree rather than these supplied inputs.
    recorded_flags = command_input["forced_TU_flags"]
    expected_flags = ["-I" + str(profile), "-I" + str(mbed / "include"),
                      "-I" + str(mbed / "library"), str(mbed / "library/common.h"),
                      str(config["tls_effective.h"])]
    if (any(flag not in recorded_flags for flag in expected_flags) or
            command_input["environment"]["CC"] != str(wrapper) or
            command_input["configure"][0] != str(source / "configure")):
        raise ValueError("download wrapper/profile/Mbed/source binding mismatch")
    baseline = _checked(retained / "curl-build/lib/.libs/libcurl.a",
                        hashes["curl"], "curl baseline archive")
    libraries = [_checked(retained / ("lib" + name + ".a"), hashes[name], name)
                 for name in ("mbedtls", "mbedx509", "mbedcrypto")]
    patch = _checked(NATIVE_HOST / "dependency-patches/curl-8.18-init-failure.patch",
                     PATCH_SHA, "curl 8.18 initialization patch")
    notices = {"CURL-LICENSE": source / "COPYING", "MBEDTLS-LICENSE": mbed / "LICENSE",
               "FRAMEWORK-LICENSE": mbed / "framework/LICENSE"}
    notices = {name: _checked(path, NOTICE_HASHES[name], name)
               for name, path in notices.items()}
    curl_files = _verify_vendor(source, profile.parent / "curl-8.18.0.tar.xz", CURL_RELEASE_SHA)
    mbed_files = _verify_vendor(mbed, profile.parent / ("mbedtls-" + MBED_PIN + ".tar.gz"),
                                MBED_RELEASE_SHA)
    validator = _crypto_validator()
    env = validator._environment()
    for name in ("CFLAGS", "CPPFLAGS", "LDFLAGS", "LIBS", "CONFIG_SITE",
                 "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_PREFIX"):
        env.pop(name, None)
    env["GIT_CEILING_DIRECTORIES"] = str(output.parent)
    native_options, resource, tools = [], None, {}
    if mode == "native":
        if sdk is None or llvm_bin is None:
            raise ValueError("download native mode requires explicit SDK and LLVM paths")
        sdk, llvm_bin = Path(sdk).resolve(), Path(llvm_bin).resolve()
        if output == sdk or output.is_relative_to(sdk) or output.is_relative_to(llvm_bin.parent):
            raise ValueError("download output must be outside native tool inputs")
        sdk_files = validator._verify_sdk(sdk, llvm_bin.parents[2] / "sdk.zip")
        for name, expected in validator.TOOL_HASHES.items():
            tool = (llvm_bin / name).resolve()
            if not tool.is_relative_to(llvm_bin):
                raise ValueError("download LLVM tool link escapes its pinned package")
            _checked(tool, expected, "LLVM " + name)
        selected, manifest_sha = validator._read_llvm_selected_manifest()
        resource = Path(recorded_flags[recorded_flags.index("-resource-dir") + 1]).resolve()
        resource_files = {name[len("lib/clang/21/"):]: value for name, value in selected.items()
                          if name.startswith("lib/clang/21/")}
        validator._check_tree(resource, resource_files)
        ar = _checked(profile.parent / "tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ar",
                      selected["bin/llvm-ar"], "LLVM archiver")
        if (command_input["environment"]["LLVM_CONFIG"] != str(llvm_bin / "llvm-config") or
                repr(str(sdk / "bin/prospero-clang")) not in wrapper.read_text()):
            raise ValueError("download wrapper SDK/LLVM path mismatch")
        env["LLVM_CONFIG"] = str(llvm_bin / "llvm-config")
        native_options = ["-nostdinc", "-resource-dir", str(resource),
                          "-isystem", str(resource / "include")]
        tools = {"sdk_archive_sha256": validator.SDK_SHA,
                 "sdk_commit": validator.SDK_COMMIT, "sdk_members": sdk_files,
                 "llvm_tools": validator.TOOL_HASHES,
                 "llvm_selected_manifest_sha256": manifest_sha,
                 "llvm_selected_members": selected}
    else:
        if sdk is not None or llvm_bin is not None:
            raise ValueError("download host mode must not receive native tool paths")
        ar = Path("/usr/bin/ar")
        tools = {"compiler": {"path": "/usr/bin/clang", "sha256": digest("/usr/bin/clang")},
                 "archiver": {"path": str(ar), "sha256": digest(ar)}}
    originals_to_preserve = [*originals.values(), *config.values(), wrapper, curl_config,
                             command_file, baseline, *libraries, *notices.values()]
    preserved = {str(path): digest(path) for path in originals_to_preserve}
    output.mkdir(parents=True, exist_ok=False)
    commands = []
    private = output / "lib/vtls"
    private.mkdir(parents=True)
    for name, path in originals.items():
        shutil.copyfile(path, private / name)
    _run(["git", "apply", "--check", patch], output, commands, env)
    _run(["git", "apply", patch], output, commands, env)
    if any(digest(private / name) == SOURCE_HASHES[name] for name in originals):
        raise ValueError("download patch did not change both private units")
    effective = _run([wrapper, "-dM", "-E", "-x", "c", "/dev/null"], output, commands, env)
    (output / "effective-macros.txt").write_text(effective)
    abi = _abi(effective)
    if mode == "native":
        wrapper_abi = _run([sdk / "bin/prospero-clang", *native_options,
                            "-dM", "-E", "-x", "c", "/dev/null"], output, commands, env)
        if abi != _abi(wrapper_abi) or abi["__SIZEOF_POINTER__"] != "8":
            raise ValueError("download native compiler ABI mismatch")
    includes = ["-DHAVE_CONFIG_H", "-DBUILDING_LIBCURL", "-DCURL_STATICLIB",
                "-DCURL_HIDDEN_SYMBOLS", "-I" + str(source / "include"),
                "-I" + str(curl_config.parent), "-I" + str(source / "lib"),
                "-I" + str(source / "lib/vtls"), "-isystem", str(mbed / "include"),
                "-Qunused-arguments", "-Werror-implicit-function-declaration", "-O2",
                "-fvisibility=hidden"]
    if mode == "host":
        includes += ["-Werror=partial-availability"]
    archive = output / "libcurl.a"
    shutil.copyfile(baseline, archive)
    units, closures = {}, {}
    roots = [(output, "private_patched_source"), (profile, "retained_profile"),
             (source, "curl_source"), (mbed, "mbedtls_source")]
    if mode == "native":
        roots += [(sdk, "sdk"), (resource, "llvm_resource")]
    for name in originals:
        obj = output / ("libcurl_la-" + name[:-2] + ".o")
        dep = obj.with_suffix(".d")
        _run([wrapper, *includes, "-MD", "-MF", dep, "-c", private / name,
              "-o", obj], output, commands, env)
        if mode == "native":
            data = obj.read_bytes()
            if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (1, 62):
                raise ValueError("download patched unit is not native x86_64 relocatable ELF")
        units[obj.name] = digest(obj)
        closures[obj.name] = _includes(dep, mode, roots)
        _run([ar, "rcsD" if mode == "native" else "rcs", archive, obj], output, commands, env)
    members = _run([ar, "t", archive], output, commands, env).splitlines()
    for name in units:
        if members.count(name) != 1:
            raise ValueError("download patched archive member closure mismatch")
    if any(name in members for name in ("entropy.o", "clock_policy.o", "curl_attempt.o")):
        raise ValueError("download archive contains a retained experiment owner")
    if any(digest(path) != expected for path, expected in preserved.items()):
        raise ValueError("download preparation changed a retained input")
    if (_verify_vendor(source, profile.parent / "curl-8.18.0.tar.xz", CURL_RELEASE_SHA) != curl_files or
            _verify_vendor(mbed, profile.parent / ("mbedtls-" + MBED_PIN + ".tar.gz"),
                           MBED_RELEASE_SHA) != mbed_files):
        raise ValueError("download vendor closure changed during preparation")
    identity = {"schema_version": 1, "mode": mode, "config_id": CONFIG_ID,
                "curl_commit": CURL_PIN, "mbedtls_commit": MBED_PIN,
                "framework_commit": FRAMEWORK_PIN,
                "release_sha256": {"curl": CURL_RELEASE_SHA, "mbedtls": MBED_RELEASE_SHA},
                "configuration_sha256": CONFIG_HASHES, "baseline_archive_sha256": hashes["curl"],
                "archive_sha256": digest(archive), "mbedtls_archives_sha256": {
                    path.name: digest(path) for path in libraries},
                "patch_sha256": PATCH_SHA, "recipe_sha256": digest(__file__),
                "patched_unit_sha256": units, "include_closure": closures,
                "vendor_source_sha256": {"curl": curl_files, "mbedtls": mbed_files},
                "tools": tools, "abi_macros": abi, "retained_inputs_unchanged": True,
                "target_executed": False, "notice_sha256": NOTICE_HASHES,
                "dependency_rebuild_scope": "two curl 8.18 initialization units only"}
    (output / "manifest.json").write_text(json.dumps(identity, indent=2, sort_keys=True) + "\n")
    return {"archive": str(archive), "include_dirs": [str(profile), str(mbed / "include"),
                                                       str(mbed / "library"), str(source / "include")],
            "compile_definitions": ['MBEDTLS_CONFIG_FILE="tls_config.h"'],
            "forced_includes": [str(mbed / "library/common.h"), str(config["tls_effective.h"])],
            "native_compile_options": native_options,
            "link_archives": [str(archive), *map(str, libraries)],
            "link_options": (["-Wl,--gc-sections", "-Wl,-z,now", "-lkernel_web"] if mode == "native" else
                             ["-framework", "SystemConfiguration", "-framework", "CoreFoundation"]),
            "resource_dir": str(resource) if resource else None,
            "config_id": CONFIG_ID, "identity": identity,
            "attributions": {name: str(path) for name, path in notices.items()},
            "commands": commands, "evidence_manifest": str(output / "manifest.json")}
