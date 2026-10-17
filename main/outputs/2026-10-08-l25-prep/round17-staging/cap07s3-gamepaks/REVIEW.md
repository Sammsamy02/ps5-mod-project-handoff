# cap07s3-gamepaks: independent review (Mac-only)

Reviewer: one independent reviewer agent, 2026-10-13. Read-only. Nothing was built for the PS5 and nothing touched the console.

**VERDICT: CHANGES REQUIRED.** There is one small blocker (B1, diagnosability). Everything else is correct.
After B1 (and ideally S1) the patch is approvable without another review round.

## What I checked (all Mac-only)

- `base/` matches the live tree byte for byte. All five sha256 values equal `BASE-SHA256SUMS` and typescan-r4.
- SDK facts: ps5-payload-sdk v0.43, target `x86_64-sie-ps5`, libc++ `_LIBCPP_VERSION 180100`. The predefined macros
  show `__WCHAR_WIDTH__ 16` and `__WCHAR_TYPE__ unsigned short`.
- PS5 compiler `-fsyntax-only -std=c++23 -Wall -Wextra -Wshadow-all` on the new header (via gamepaks_test.cpp): clean.
- Host: the staged gamepaks_test passes 17/17 under ASan+UBSan. My extra test passes 8/8 under ASan+UBSan. It covers
  a trailing-slash game dir plus census, `~mods` as a file, `ENGINE` in upper case, an unreadable project folder, an
  unreadable `paks` folder (errno 13), a symlinked sibling, and a sibling with 4097 entries. The source is in the
  appendix.
- fs_throw_gate on `new/`: 5 files, 32 fs call sites, 20 error_code overloads (+2 vs base), 0 approved,
  12 violations. All 12 are the same pre-existing LuaMod.cpp `exists()` sites that base has. They show as "new" only
  because baseline keys are path-based. None is in the patched code. Re-run on the live tree after merge.

## Answers

1. **libc++ 18 iterator semantics.**
   - `increment(ec)`: the ErrorHandler clears ec. When advance fails, `__imp_` is reset, so the iterator equals end and
     ec is set. The loop exits and the post-loop `if(list_ec)` (:61) reports it. The in-loop check (:50) can never
     fire, but it does no harm.
   - Constructor error: the iterator equals end and ec is set.
   - The `n>4096` bound counts every entry, including filtered ones. Good.
   - DT_UNKNOWN: libc++ caches the type as `none`, i.e. `_Empty`. `is_symlink(ec)` then calls `symlink_status` (lstat)
     and `is_directory(ec)` calls `status` (stat). Any failure comes back in ec and gives
     "entry type unavailable: <path> (errno N)". That is diagnosable.
   - At the named levels, the name filter runs before the type query, so only the candidate entry is ever stat'ed.
   - Note: launch J's failure is equally explained by /data being invisible after start, so "lstat fails on PS5" is
     not proven either way.
   - Policy: **keep aborting on an entry-type error at the /app0 level.** Skipping that entry could turn "several"
     into "one", which is a guess.
   - **Keep skip-and-count for unlistable children.** sce_sys and sce_module may legitimately refuse a listing, and
     making that fatal would block every game. But the miss line must name each child and its errno (B1).
2. **StringType / lifetime.**
   - `File::StringType = std::basic_string<wchar_t>` (FORCE_U16 applies only to `__linux__`), and wchar_t is 16-bit on
     PS5. The function returns by value. The implicit `path(const Source&)` temporary lives until the end of the call's
     full-expression, and the callee stores no reference, so nothing dangles.
   - The conversion is libc++ `__narrow_to_utf8<16>` (UTF-16 to UTF-8). It is lossless for ASCII. The same call
     already converts `get_mods_directory()` the same way, and UE4SSProgram.cpp:2136 does too.
   - Any throw (bad surrogate or bad_alloc) happens during argument evaluation in `request_pak_mount_once`. That is
     inside its `try/catch(...)` (LuaMod.cpp:7101-7110), not in the noexcept body.
   - `path::string()` on POSIX is a plain copy of the narrow native string, and `pak_log` masks non-ASCII as '?'.
     Safe.
3. **Source selection.**
   - The configured source is checked first with stat+ec. Any error or a non-directory is logged, then the code falls
     through to the game `~mods`. If both miss, it logs `disabled: no pakmods directory (game paks: <why>)` and returns
     (opt-out).
   - snprintf truncation is safe, and the `.string().c_str()` temporaries live through the call.
   - All new code sits inside the existing try. The throws from `discover_requests` are still caught, and the
     `attempted` flag is unchanged.
   - The build marker literal is still present: the new format string begins with it. The other declared `[PAKMOUNT]`
     markers ("pre-call refuse: ", "stopped: ", "status=%u call=%u ret=0x%llx") are also still present.
   - One behaviour change: a root-shape mismatch now falls through to game paks instead of refusing. That is
     acceptable.
4. **Preconditions.** The output comes from directory_iterator, so it is normalized, including for a game dir with a
   trailing slash (tested).
   - `valid_absolute_path` allows '~'. It rejects only control characters, '\\', and empty, `.` or `..` segments.
   - The parent-listing check matches `~mods` by its on-disk spelling.
   - The census and mount() see `/app0/townfall/content/paks/~mods/<name>.pak` (tested end to end on the host).
   - Info: MountPak receives the native `/app0/...` form, not the "engine-form path" mentioned in the J-K-L report's
     redesign. That is not proven on the console. A failure is safe: UE logs a warning, ret is 0, there is no Mounted
     verdict, and the status line shows it.
5. **Crash, hang and cost.** I found no crash or hang path.
   - Townfall cost is once per process: /app0 (about 5 entries), each child (sce_sys has tens), content, paks (about
     50, listed twice), and `~mods`. That is well under 1k readdir entries plus a few stats, i.e. milliseconds.
   - The theoretical worst case is 4096 children × 4096 entries (N3).
6. **Call sites and includes.**
   - The only compiled call site is candidate/src/LuaMod.cpp:7103, inside `#ifdef __PROSPERO__`.
   - The other matches are not compiled: the backups typescan-r4/LuaMod.cpp.pre-pakmount-trigger-20261013 and
     NativePakMountAdapter.inl.pre-fsfix-20261014, and the intake harness
     intake/gpt-7-10-v2-20261013/gates/inc-only/NativePakMountAdapter.cpp, which keeps its own one-argument Impl copy.
   - No typescan-r4 test references the function, and no gate checks the mangled symbol.
   - The patch adds no #include. `std::vector` comes in via RuntimePakMount.hpp.
   - UnrealInitializer.cpp does not include NativePakMountEntry.hpp, so a signature mismatch would only show at link
     time. The two signatures match today.

## Findings

**B1 — blocker — new/include/PakRequestProducer.hpp:76-87.** A miss caused by an unlistable child does not name the
child or its errno.

Host demo with the project folder at chmod 0: `no project Content folder in …/r4 (children=2 unreadable=1)`. From that
line you cannot tell whether it was townfall or sce_sys, or which errno. Self-test 134 failed in exactly this blind
spot (its `igd_has_content_child` swallows listing errors). So the next launch could miss without saying why, which
breaks the "one log line" requirement.

Replace lines 76-87 with:

```cpp
 // A child that cannot be listed (a system folder of the package) is skipped and counted, never fatal. A miss names
 // every child examined with its Content-hit count or its listing error, so one log line explains it.
 size_t unreadable=0;std::string seen;
 for(const auto& child:children){
  std::vector<std::filesystem::path> hits;std::string child_why;
  const bool listed=list_child_dirs(child,"content",hits,child_why);
  if(seen.size()<360)seen+=" "+child.filename().string()+(listed?"="+std::to_string(hits.size()):" ["+child_why+"]");
  if(!listed){++unreadable;continue;}
  content.insert(content.end(),hits.begin(),hits.end());
 }
 if(content.size()!=1){
  why=std::string(content.empty()?"no":"several")+" project Content folder in "+game_dir.string()+" (children="+
      std::to_string(children.size())+" unreadable="+std::to_string(unreadable)+":"+seen+")";
  return {};
 }
```

Verified in scratch with ASan+UBSan: 17/17 + 8/8. The PS5 `-fsyntax-only -Wshadow-all` check is clean. Example output:
`(children=2 unreadable=1: townfall [listing unavailable: …/townfall (errno 13)] sce_sys=0)`. The worst-case line is
about 580 characters, which fits in `line[640]`, and snprintf truncates safely beyond that. The existing tests still
pass because they match substrings.

**S1 — should — MARKERS.txt, marker `cap07s3-gamepaks-listing`.** The text `listing unavailable` is already in the
current R16d binary, because `discover_requests` contains "configured root listing unavailable" (I grep'd the frozen
3b4c202b PRX). So this marker cannot prove the new code is in the build. Replace it with:

```python
        ('cap07s3-gamepaks-miss', ' project Content folder in ', 'utf-8', 'PakRequestProducer.hpp game_pak_mods_directory miss reason'),
```

That literal is absent from R16d.

**N1 — nit — NativePakMountAdapter.inl:191.** The `char line[320];` inside the mount loop shadows the outer
`line[640]`. That is legal, and -Wshadow is not in the build flags, but deleting the inner declaration is clearer.

**N2 — nit — NativePakMountAdapter.inl:168.** When stat succeeds but the entry is not a directory, the line prints
`(errno 0)`. Consider also printing `int(configured_status.type())`.

**N3 — nit — PakRequestProducer.hpp:75.** There is no total budget for the walk, so the game thread could in theory
spend 4096×4096 entries. A cap on /app0 children (for example `if(children.size()>256){why="too many children: "+…;return {};}`)
would bound it. Real UE packages have about 5.

**N4 — nit — PakRequestProducer.hpp:29.** The comment says "Nothing here throws", but string building can throw
bad_alloc. The caller's try catches it, so only the comment needs a tweak.

**N5 — nit.** `unique_child_dir(dir,nullptr,…)` would be undefined behaviour (`string + nullptr`). It is never called
that way. A one-line comment or a guard would cover it.

**Note for the Stellar Blade step (not this launch).** PC Unreal auto-mounts `Paks/~mods` itself. On an engine build
that does the same, this source would mount the same files a second time under a different path spelling.
`FPakPlatformFile::Mount`'s duplicate check compares the exact filename string, so it would not catch the repeat.
Launch L shows this UE 5.6.1 build does not auto-mount, so Townfall is not affected. Before the blind test, the
generic runtime needs an "engine already mounted ~mods" check.

## Appendix — reviewer extra host test (Mac-only)

Build:

```
clang++ -std=c++20 -fsanitize=address,undefined -I new/include -I typescan-r4/include extra_test.cpp typescan-r4/candidate/src/RuntimePakMount.cpp
```

Run it with an empty scratch dir. Result on the patch as staged: 8/8 PASS, plus
`INFO unreadable-project names child+errno: NO`. With B1 applied it prints `yes`.

```cpp
#include "PakRequestProducer.hpp"
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
namespace fs = std::filesystem;
using namespace ModBridge::PakMount;
static int run=0,failed=0;
static void check(bool ok,const char* n,const std::string& d){++run;if(!ok)++failed;std::printf("%s %s %s\n",ok?"PASS":"FAIL",n,d.c_str());}
static void touch(const fs::path& p){fs::create_directories(p.parent_path());std::ofstream(p)<<"x";}
int main(int argc,char**argv){
 if(argc!=2)return 2;const fs::path root=argv[1];std::string why;
 { auto app=root/"r1";touch(app/"eboot.bin");fs::create_directories(app/"sce_sys");
  touch(app/"townfall/content/paks/~mods/m_P.pak");touch(app/"townfall/content/paks/~mods/m_P.utoc");touch(app/"townfall/content/paks/~mods/m_P.ucas");
  fs::path slash=app.string()+"/";why.clear();auto got=game_pak_mods_directory(slash,why);
  check(!got.empty()&&got.lexically_normal()==got&&valid_absolute_path(got.string()),"trailing-slash",got.string()+" "+why);
  if(!got.empty()){auto rows=discover_requests(got);check(rows.size()==1&&valid_absolute_path(rows[0].pak_path_abs)&&rows[0].pak_path_abs.find("//")==std::string::npos,"trailing-slash-census",rows.empty()?"none":rows[0].pak_path_abs);} }
 { auto app=root/"r2";touch(app/"townfall/content/paks/~mods");why.clear();auto got=game_pak_mods_directory(app,why);
  check(got.empty()&&why.find("no ~mods folder")!=std::string::npos,"tilde-mods-file",why); }
 { auto app=root/"r3";fs::create_directories(app/"ENGINE/Content/Paks/~mods");why.clear();auto got=game_pak_mods_directory(app,why);
  check(got.empty(),"ENGINE-excluded",why); }
 { auto app=root/"r4";fs::create_directories(app/"sce_sys");fs::create_directories(app/"townfall/content/paks/~mods");
  ::chmod((app/"townfall").c_str(),0);why.clear();auto got=game_pak_mods_directory(app,why);::chmod((app/"townfall").c_str(),0755);
  const bool names_child=why.find("townfall")!=std::string::npos&&why.find("errno")!=std::string::npos;
  check(got.empty(),"unreadable-project-miss",why);
  std::printf("INFO unreadable-project names child+errno: %s\n",names_child?"yes":"NO (diagnosability gap)"); }
 { auto app=root/"r5";fs::create_directories(app/"townfall/content/paks/~mods");::chmod((app/"townfall/content/paks").c_str(),0);
  why.clear();auto got=game_pak_mods_directory(app,why);::chmod((app/"townfall/content/paks").c_str(),0755);
  check(got.empty()&&why.find("listing unavailable")!=std::string::npos&&why.find("errno 13")!=std::string::npos,"unreadable-paks-errno",why); }
 { auto app=root/"r6";fs::create_directories(app/"townfall/content/paks/~mods");fs::create_directories(root/"r6out/content");
  fs::create_directory_symlink(root/"r6out",app/"other");why.clear();auto got=game_pak_mods_directory(app,why);
  check(got==app/"townfall/content/paks/~mods","symlink-sibling-ignored",got.string()+" "+why); }
 { auto app=root/"r7";fs::create_directories(app/"townfall/content/paks/~mods");fs::create_directories(app/"big");
  for(int i=0;i<4097;++i)std::ofstream(app/"big"/("f"+std::to_string(i)))<<"";
  why.clear();auto got=game_pak_mods_directory(app,why);
  check(got==app/"townfall/content/paks/~mods","bound-sibling-skipped",got.string()+" "+why); }
 std::printf("RUN %s run=%d failed=%d\n",(run>0&&failed==0)?"PASS":"FAIL",run,failed);
 return (run>0&&failed==0)?0:1;
}
```

## Integrator closure (2026-10-14)

- B1 applied as given (children named with Content-hit count or listing error). S1 applied (marker
  `cap07s3-gamepaks-miss` = ' project Content folder in '). N1 (inner buffer removed), N2 (`type=%d` added to the
  configured-miss line and its marker), N3 (`children>256` = miss), N4 (comment), N5 (null-name guard) applied.
- Re-run: staged host test 17/17, reviewer extra test 8/8 (`INFO unreadable-project names child+errno: yes`), both
  under ASan+UBSan. no_detour PASS; fs_throw on include 6 sites / 0 violations.
- Stellar Blade note (engine that auto-mounts ~mods) recorded for the blind-test step; not this launch.
