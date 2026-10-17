# SOURCE-FACTS — UE 5.6.1 file/IoStore source cross-check for the pak mount design

Second, independent source (official UE 5.6.1 source) for the FPakPlatformFile
vtable slot meanings and the file/IoStore boot path, to corroborate the static
RE pass `../PAK-PIN-PASS.md` + `../pak-pin-pass-pins.json`.

- Date 2026-10-12. Read-only, Mac-only. No build, no console, no console network.
- Source basis: pinned commit `6978b63c8951e57d97048d8424a0bebd637dde1d`,
  `Build.version` = **UE 5.6.1**, CompatibleChangelist 43139311 (local only,
  git-ignored, blob-sha1 verified). Epic source stays local — not copied out,
  only short excerpts quoted.
- All paths below are relative to
  `outputs/2026-10-08-l23-layout-generic/official-source/Engine/Source/Runtime/`
  (abbreviations: **GPF.h** = `Core/Public/GenericPlatform/GenericPlatformFile.h`;
  **PFM.h/.cpp** = `Core/{Public,Private}/HAL/PlatformFileManager.{h,cpp}`;
  **Pak.h/.cpp** = `PakFile/{Public,Private}/IPlatformFilePak.{h,cpp}`;
  **LEL.cpp** = `Launch/Private/LaunchEngineLoop.cpp`;
  **IDFB.cpp** = `PakFile/Private/IoDispatcherFileBackend.cpp` (legacy FFileIoStore);
  **FIDB.cpp** = `PakFile/Private/FileIoDispatcherBackend.cpp` (new backend);
  **GPIOD-pak.cpp** = `PakFile/Private/GenericPlatformIoDispatcher.cpp`;
  **GPIOD-core.cpp** = `Core/Internal/IO/GenericPlatformIoDispatcher.cpp`;
  **PIOD.h** = `Core/Internal/IO/PlatformIoDispatcher.h`;
  **IDFBT.h** = `PakFile/Internal/IoDispatcherFileBackendTypes.h`).

> **Headline for cap-07.** Source and the RE pass agree *exactly* on IPlatformFile
> slots `+0x00 … +0x88` (dtor pair through FileSize) — 9 of 9 hard RE pins there
> match. **Past +0x88 the game's vtable diverges from this 5.6.1 header**: RE pins
> OpenRead at +0xb8 and IterateDirectory at +0x1c0, but 5.6.1 declaration order
> puts OpenRead(IFileHandle*) at +0xf8 and IterateDirectory(visitor&) at +0x130.
> Townfall was built on a 5.6 whose IPlatformFile tail has ~8 fewer virtuals
> between FileSize and OpenRead than released 5.6.1 (MarkOfTheWeb / FileJournal /
> FFileOpenResult-OpenRead / DeleteFiles(TArrayView) / IsSymlink are post-split
> additions). **Consequence: the binary (PAK-PIN-PASS) is ground truth for slot
> offsets ≥ +0x90; 5.6.1 source corroborates only ShouldBeUsed/Initialize/
> GetLowerLevel/SetLowerLevel/GetName/FileExists/FileSize and the member/boot
> facts.** Of the four slots the hook replaces, only **FileExists (+0x80)** and
> **FileSize (+0x88)** are source-confirmed at offset; **OpenRead (+0xb8)** and
> **IterateDirectory (+0x1c0)** rest on RE alone.

---

## 1. IPlatformFile virtual order + Itanium vtable slots (shipping/non-editor/non-Win)

**Config:** The IPlatformFile body (GPF.h:337–948) contains **no `#if` blocks**
— every virtual is unconditionally compiled in all configs. The only `#if` that
touches layout is in FPlatformFileManager (item 2), not here. `UE_DEPRECATED(5.6,…)`
on OpenMapped (GPF.h:755) does **not** remove its slot. Nested-class virtuals
(FDirectoryVisitor @548/555/566, FDirectoryStatVisitor @590/597/608,
IFileServerMessageHandler @892/895/898) are separate vtables and excluded.
IPlatformFile has **no data members** and is the root (no base), so vtable base =
after offset-to-top(0)+typeinfo. `~IPlatformFile()` is declared first (GPF.h:346)
and is virtual → Itanium emits complete dtor (D1) at +0x00, deleting dtor (D0) at
+0x08. Total = **72 slots** for 5.6.1 (RE game vtable = 74).

Itanium slot table (offset = slot×8), `src L` = GPF.h line, **RE §2** = PAK-PIN-PASS label at that offset:

| off | src decl (5.6.1) | src L | RE §2 @ off | verdict |
|---|---|---|---|---|
| +0x00 | ~IPlatformFile complete dtor D1 | 346 | dtor | AGREE |
| +0x08 | ~IPlatformFile deleting dtor D0 | 346 | dtor | AGREE |
| +0x10 | `void SetSandboxEnabled(bool)` | 353 | stub (void) 0xb290 | AGREE (shape) |
| +0x18 | `bool IsSandboxEnabled() const` | 362 | stub (ret false) 0xe0 | AGREE (shape) |
| +0x20 | `bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const` | 371 | **ShouldBeUsed** | **AGREE** |
| +0x28 | `bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)=0` | 382 | **Initialize** (assert str) | **AGREE** |
| +0x30 | `void InitializeAfterSetActive()` | 387 | stub (InitAfterIoReaders-shaped) | name differs; slot agrees |
| +0x38 | `void InitializeAfterProjectFilePath()` | 392 | stub (InitAfterProjectFilePath-shaped), called by chain walk | **AGREE** |
| +0x40 | `void MakeUniquePakFilesForTheseFiles(const TArray<TArray<FString>>&)` | 397 | real, "FString-arg", unlabeled [GAP] | **AGREE** — arg is TArray<TArray<FString>>& (RDI=this, RSI=&TArray) |
| +0x48 | `void InitializeNewAsyncIO()` | 402 | — | (source) |
| +0x50 | `void AddLocalDirectories(TArray<FString>&)` | 407 | folded 0x25340 | plausible |
| +0x58 | `void BypassSecurity(bool)` | 415 | folded 0x25380 | plausible |
| +0x60 | `void Tick()` | 424 | — | (source) |
| +0x68 | `IPlatformFile* GetLowerLevel()=0` | 426 | **GetLowerLevel** `[rdi+8]` | **AGREE** |
| +0x70 | `void SetLowerLevel(IPlatformFile*)=0` | 428 | **SetLowerLevel** `[rdi+8]=rsi` | **AGREE** |
| +0x78 | `const TCHAR* GetName() const =0` | 430 | **GetTypeName**→L"PakFile" | **AGREE** (GetName override) |
| +0x80 | `bool FileExists(const TCHAR* Filename)=0` | 432 | **FileExists** | **AGREE** |
| +0x88 | `int64 FileSize(const TCHAR* Filename)=0` | 434 | **FileSize** | **AGREE** |
| +0x90 | `bool DeleteFile(const TCHAR* Filename)=0` | 436 | "bool dir-family" (guess) | **DISAGREE** (version skew begins) |
| +0x98 | `bool DeleteFiles(const TArrayView<const TCHAR*>&)` | 438 | folded | — |
| +0xa0 | `bool IsReadOnly(const TCHAR*)=0` | 450 | dir/move family | — |
| +0xa8 | `bool MoveFile(const TCHAR* To, const TCHAR* From)=0` | 452 | dir/move family | — |
| +0xb0 | `bool SetReadOnly(const TCHAR*, bool)=0` | 454 | dir/move family | — |
| +0xb8 | `FDateTime GetTimeStamp(const TCHAR*)=0` | 456 | **OpenRead** (FPakFile ctor call site) | **DISAGREE** — game's OpenRead ≠ source offset |
| +0xc0 | `void SetTimeStamp(const TCHAR*, FDateTime)=0` | 458 | OpenWrite-class | — |
| +0xc8 | `FDateTime GetAccessTimeStamp(const TCHAR*)=0` | 460 | handle-releasing variant | **DISAGREE** |
| +0xd0 | `FString GetFilenameOnDisk(const TCHAR*)=0` | 462 | handle-releasing variant | **DISAGREE** |
| +0xd8 | `ESymlinkResult IsSymlink(const TCHAR*)` | 465 | pure forwarder 0xe3240 | — |
| +0xe0 | `bool HasMarkOfTheWeb(FStringView, FString*)` | 476 | — | — |
| +0xe8 | `bool SetMarkOfTheWeb(FStringView, bool, const FString*)` | 488 | — | — |
| +0xf0 | `FFileOpenResult OpenRead(const TCHAR*, EOpenReadFlags)` [overload A] | 508 | — | — |
| +0xf8 | `IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite=false)=0` [B] | 519 | (RE sees this at +0xb8) | **DISAGREE on offset** |
| +0x100 | `IFileHandle* OpenReadNoBuffering(const TCHAR*, bool)` | 521 | — | — |
| +0x108 | `IFileHandle* OpenWrite(const TCHAR*, bool, bool)=0` | 527 | heavily used by IoStore, unlabeled [GAP] | **DISAGREE** (see note) |
| +0x110 | `bool DirectoryExists(const TCHAR*)=0` | 530 | used by pak scan + IoStore, unlabeled [GAP] | **DISAGREE** |
| +0x118 | `bool CreateDirectory(const TCHAR*)=0` | 532 | pure forwarder | — |
| +0x120 | `bool DeleteDirectory(const TCHAR*)=0` | 534 | — | — |
| +0x128 | `FFileStatData GetStatData(const TCHAR*)=0` | 537 | — | — |
| +0x130 | `bool IterateDirectory(const TCHAR*, FDirectoryVisitor&)=0` | 626 | (RE sees IterateDirectory at +0x1c0) | **DISAGREE on offset** |
| +0x138 | `bool IterateDirectoryStat(const TCHAR*, FDirectoryStatVisitor&)=0` | 634 | — | — |
| +0x140…+0x170 | FileJournal* family (7 virtuals) | 642–717 | — | post-split additions |
| +0x180 | `FFileOpenAsyncResult OpenAsyncRead(const TCHAR*, EOpenReadFlags)` [A] | 729 | — | — |
| +0x188 | `IAsyncReadFileHandle* OpenAsyncRead(const TCHAR*, bool)` [B] | 737 | — | — |
| +0x190 | `void SetAsyncMinimumPriority(EAsyncIOPriorityAndFlags)` | 744 | — | — |
| +0x198 | `IMappedFileHandle* OpenMapped(const TCHAR*)` [deprecated 5.6] | 756 | — | — |
| +0x1a0 | `FOpenMappedResult OpenMappedEx(const TCHAR*, EOpenReadFlags, int64)` | 766 | — | — |
| +0x1a8 | `void GetTimeStampPair(...)` | 768 | — | — |
| +0x1b0 | `FDateTime GetTimeStampLocal(const TCHAR*)` | 771 | — | — |
| +0x1b8 | `bool IterateDirectory(const TCHAR*, FDirectoryVisitorFunc)` | 779 | — | — |
| +0x1c0 | `bool IterateDirectoryStat(const TCHAR*, FDirectoryStatVisitorFunc)` | 787 | **IterateDirectory** (per-folder scan) | **DISAGREE** (src=IterateDirectoryStat) |
| +0x1c8 | `bool IterateDirectoryRecursively(const TCHAR*, FDirectoryVisitor&)` | 795 | IterateDirectoryRecursively-class | **AGREE by name** (coincidental; offsets shifted) |
| +0x1d0…+0x240 | StatRecursively/FindFiles/Copy/Convert/SendMessage/PublicFiles/Throttle | 803–944 | — | — |

**Requested slots, full signatures (register note: in SysV the `const TCHAR*`
filename is RSI — RDI is always `this`):**
- **+0x10** `SetSandboxEnabled(bool bInEnabled)` → bInEnabled in SIL/ESI.
- **+0x18** `IsSandboxEnabled() const` → no filename.
- **+0x30** `InitializeAfterSetActive()` → no args.
- **+0x38** `InitializeAfterProjectFilePath()` → no args.
- **+0x40** `MakeUniquePakFilesForTheseFiles(const TArray<TArray<FString>>& InFiles)`
  → RSI = `const TArray<TArray<FString>>*` (not a TCHAR*). (This is the RE "FString-arg" slot — it is the only pre-+0x88 real impl with an FString-bearing argument, which is why the shape matched.)
- **+0xc8** source = `GetAccessTimeStamp(const TCHAR* Filename)` → RSI = filename.
  **But at +0xc8 in the game this offset is past the divergence** — do not trust.
- **+0xd0** source = `GetFilenameOnDisk(const TCHAR* Filename)` → RSI = filename.
  Past divergence — do not trust.
- **+0x108** source = `OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)`
  → RSI = filename. **RE sees +0x108 heavily used by IoStore** (async/open-class).
  In the game's shifted layout +0x108 is most likely an Open*-class slot whose
  first real arg is a `const TCHAR*` filename in RSI — but the exact identity must
  come from the binary, not this offset in 5.6.1 source.
- **+0x110** source = `DirectoryExists(const TCHAR* Directory)` → RSI = directory.
  RE: used by pak scan + IoStore. Game identity from binary.

---

## 2. FPlatformFileManager layout + methods (PFM.h / PFM.cpp)

Non-polymorphic class (no vtable). Members (PFM.h:18–26):

| off | member | note |
|---|---|---|
| +0x00 | `TopmostPlatformFile` | active chain head. `#if USE_ATOMIC_PLATFORM_FILE` → `TAtomic<IPlatformFile*>` else raw `IPlatformFile*`; **8 bytes either way, offset unchanged** |
| +0x08 | `PhysicalPlatformFile` | the physical/default file |

- **Confirms RE §1 / resolves RE §7(2):** `[mgr+0]` = TopmostPlatformFile (chain
  head) and **`[mgr+8]` = PhysicalPlatformFile**. The RE hypothesis that `[mgr+8]`
  is "the engine-wide FPakPlatformFile holder" is **refuted** — the 2-call-site
  accessor RE labelled 0xb8aa0 is `GetPlatformPhysical()` (PFM.h:40). The pak is
  reached by walking the chain, not from `[mgr+8]`.
- `Get()` PFM.cpp:185 — Meyers singleton: `static FPlatformFileManager Singleton;`
  (matches RE's guard-byte + BSS object at 0x932a428).
- `GetPlatformFile()` PFM.cpp:22 — returns `*TopmostPlatformFile`, lazily sets it
  to `IPlatformFile::GetPlatformPhysical()` if null (RE 0xb8a70 `mov rax,[rdi]`).
- `GetPlatformPhysical()` PFM.cpp:31 — returns `*PhysicalPlatformFile` (`[mgr+8]`).
- `SetPlatformFile(IPlatformFile&)` PFM.cpp:41 — `TopmostPlatformFile = &New;`
  then `New.InitializeAfterSetActive()` (slot +0x30).
- `FindPlatformFile(const TCHAR* Name)` PFM.cpp:53 — walks chain via
  `GetLowerLevel()`, matches `GetName()`.
- `InitializeNewAsyncIO()` PFM.cpp:167 — walks chain calling each
  `InitializeNewAsyncIO()` (slot +0x48).

---

## 3. FPakPlatformFile member layout (Pak.h:1979–2018)

`class FPakPlatformFile : public IPlatformFile`. No `#if` or data member before
LowerLevel (lines 1981–2009 are nested structs + a friend decl). Layout:

| off | member | size | note |
|---|---|---|---|
| +0x00 | vptr (IPlatformFile vtable) | 8 | |
| +0x08 | `IPlatformFile* LowerLevel` (Pak.h:2010) | 8 | **AGREE** RE GetLowerLevel `[this+8]` |
| +0x10 | `TArray<FPakListEntry> PakFiles` (2012) | 16 | **the real mounted-pak list**; entry = 16 B |
| +0x20 | `TArray<FPakListDeferredEntry> PendingEncryptedPakFiles` (2014) | 16 | entry = 0x38 B |
| +0x30 | `bool bSigned` (2016) | +pad | |
| +0x38 | `mutable FTransactionallySafeCriticalSection PakListCritical` (2018) | — | RE §3 "PakListCritical"; type is AutoRTFM-safe critsec, **not** plain FCriticalSection |

Entry structs:
- `FPakListEntry` (Pak.h:1981): `{ uint32 ReadOrder; TRefCountPtr<FPakFile> PakFile; }`
  → ReadOrder@+0x00, PakFile(ptr)@+0x08, **size 16 B**.
- `FPakListDeferredEntry` (Pak.h:2000): `{ FString Filename; FString Path;
  uint32 ReadOrder; FGuid EncryptionKeyGuid; int32 PakchunkIndex; }`
  → Filename@+0x00(16), Path@+0x10(16), ReadOrder@+0x20, EncryptionKeyGuid@+0x24(16),
  PakchunkIndex@+0x34; **size 0x38 B**.

**Reconciliation with RE §3 (important correction):** RE reported "MountedPaks at
`[this+0x20]`, 0x38-byte entries, FString@+0x10, priority@+0x20, FGuid@+0x24".
That `0x38`-byte record with {FString@0, FString@0x10, uint32@0x20, FGuid@0x24,
int32@0x34} is **byte-exact FPakListDeferredEntry**, which 5.6.1 places at
**+0x20 = PendingEncryptedPakFiles**, *not* the mounted list. The real mounted
list (`PakFiles`, 16-byte entries) is at **+0x10**. So either (a) RE mislabeled
the deferred/encrypted-pending list as MountedPaks, or (b) the game build (which
has a different vtable tail) also reorders these members. **Phase-0 must read
both the +0x10 and +0x20 TArrays live and classify entries by size (16 B =
mounted FPakListEntry, 0x38 B = deferred).** The hook/enumeration must not assume
+0x20 is the mounted list.

---

## 4. Boot order for the file layers + IoStore (LEL.cpp / Pak.cpp)

`FEngineLoop::PreInit(CmdLine)` LEL.cpp:4140 → `PreInitPreStartupScreen` (:4152)
→ `PreInitPostStartupScreen` (:4159).

`PreInitPreStartupScreen` (LEL.cpp:1588), in order:
1. `FIoDispatcher::Initialize()` — LEL.cpp:1948.
2. `LaunchCheckForFileOverride(CmdLine,…)` — LEL.cpp:1971 → builds the chain.
3. `FPlatformFileManager::Get().InitializeNewAsyncIO()` — LEL.cpp:1995.
4. `UE::DerivedData::IoStore::InitializeIoDispatcher()` — LEL.cpp:2715.
5. `FIoDispatcher::InitializePostSettings()` — LEL.cpp:2729.

`LaunchCheckForFileOverride` (LEL.cpp:762): gets chain top (:767); tries wrappers
StorageServerClient(:776)/StreamingFile(:790)/NetworkFile(:801); then
**`ConditionallyCreateFileWrapper(TEXT("PakFile"),…)` at :837 → `SetPlatformFile`
at :841**; then CachedReadFile(:847), SandboxFile(:854), ProfileFile(:865),
SimpleProfileFile(:873), FileReadStats(:882), FileOpenLog(:891), LogFile(:902).

`ConditionallyCreateFileWrapper` (LEL.cpp:723): `WrapperFile =
GetPlatformFile(Name)` (:733); if `WrapperFile->ShouldBeUsed(Current,CmdLine)`
(:734) then **`WrapperFile->Initialize(Current, CmdLine)`** (:740). So
**FPakPlatformFile::Initialize runs here** (for the "PakFile" module).

`FPakPlatformFile::Initialize` (Pak.cpp:5609): `LowerLevel = Inner` (:5617);
then **if `FIoDispatcher::IsInitialized()` (:5650)**:
- `GlobalUTocPath = "<ProjectContentDir>/Paks/global.utoc"` (:5652);
- picks backend: if `UE::FPlatformIoDispatcher::TryGet()` →
  `MakeFileIoDispatcherBackend()` (:5665, **new** path) else
  `CreateIoDispatcherFileBackend()` (:5669, **legacy** path);
- `IoDispatcher.Mount(IoDispatcherFileBackend)` (:5671);
- `PackageStoreBackend = MakeShared<FFilePackageStoreBackend>()` (:5672);
  `FPackageStore::Get().Mount(PackageStoreBackend)` (:5673);
- `IoDispatcherFileBackend->Mount(global.utoc, 0, FGuid(), FAESKey())` (:5677).
Then (unconditional): `GetPakFolders(FCommandLine::Get(), PakFolders)` (:5708);
**`MountAllPakFiles(PakFolders, StartupPaksWildcard)`** (:5709); bind
`FCoreDelegates::OnMountAllPakFiles` (:5715).

`GetPakFolders` Pak.cpp:5539; `FindAllPakFiles` Pak.cpp:5524; `MountAllPakFiles`
Pak.cpp:6313 → `FindAllPakFiles` (:6331), `GetPakOrderFromPakFilePath` (:6370),
`Mount` each.

`PreInitPostStartupScreen` (LEL.cpp:3244): obtains the pak layer via
`FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName())`
(:3343) for later DLC/on-demand work.

---

## 5. How the generic IoStore backend opens/reads .utoc/.ucas (item 5 — key)

Two backends exist in 5.6.1; FPakPlatformFile::Initialize selects by whether a
platform `FPlatformIoDispatcher` exists (Pak.cpp:5663–5669).

**A. Legacy FFileIoStore** (IDFB.cpp + GPIOD-pak.cpp). Container data partitions
open through an abstract `IFileIoStorePlatformImpl::OpenContainer/CloseContainer`
(pure virtual, IDFBT.h:936–937). The **generic** impl:
- `FGenericFileIoStoreImpl::OpenContainer` (GPIOD-pak.cpp:58): gets
  `FPlatformFileManager::Get().GetPlatformFile()` (:63) and calls
  **`Ipf->OpenReadNoBuffering(ContainerFilePath)`** (:75), stores the returned
  `IFileHandle*` as the opaque container handle (:80). Reads via
  `FileHandle->Seek`/`Read` (:154/:159). TOC (.utoc header) read: IDFB.cpp:883–884
  `GetPlatformFile()` + **`Ipf.OpenRead(*Partition.FilePath)`**; memory-mapped
  path IDFB.cpp:841–843 uses `Ipf` + OpenMappedEx. `OpenContainer` call site:
  IDFB.cpp:685; reopen after suspend IDFB.cpp:966.

**B. New FFileIoDispatcherBackend** (FIDB.cpp + GPIOD-core.cpp). Partitions open
through `UE::FPlatformIoDispatcher::Get().OpenFile(...)` (FIDB.cpp:286) returning
an `FIoFileHandle`; `IPlatformIoDispatcher::OpenFile` is pure virtual (PIOD.h:249).
The **generic** impl `FGenericPlatformIoDispatcher::OpenFile` (GPIOD-core.cpp:66):
`Ipf.FileSize(Filename)` (:71) + **`Ipf.OpenReadNoBuffering(Filename)`** (:81),
reads via `Handle->Seek`/`Read` (:219/:224). TOC read: FIDB.cpp:340–341
`GetPlatformFile()` + **`Ipf.OpenRead(*Partition.Filename)`**.

**Verdict for Phase-0:** on BOTH generic paths the .utoc/.ucas files are opened
through **`FPlatformFileManager::Get().GetPlatformFile()` (the chain top)** using
the IPlatformFile virtuals **OpenReadNoBuffering** (data partitions),
**OpenRead** (TOC), **FileSize**, and **OpenMappedEx** (mmap). Reads themselves go
through the returned **IFileHandle** (its own vtable), not an IPlatformFile slot.
→ A data-only vtable copy on the chain-top platform-file instance **will** observe
utoc/ucas opens *iff the PS5 build uses the generic impl*. **If the PS5 provides
its own `IFileIoStorePlatformImpl` / `IPlatformIoDispatcher`** (the override hooks
at IDFBT.h:936 and PIOD.h:249 — likely for raw-handle performance) **those opens
bypass IPlatformFile entirely and a vtable hook will not see them.** Phase-0
logging must therefore watch OpenRead / OpenReadNoBuffering / FileSize / OpenMappedEx
on the chain top AND detect their *absence* during container load (= platform
override in use). (The PS5 platform impl is out of scope and was not sought.)

---

## 6. Does a container mounted after boot merge into the package store? — YES

`FPakPlatformFile::Mount(const FPakMountArgs&, …)` (Pak.cpp:5928). After the pak
record is built, if IoStore is active and the pak has a companion `.utoc`:
- `UtocPath = ChangeExtension(InPakFilename, ".utoc")` (Pak.cpp:6033), FileExists
  check (:6034);
- `MountResult = IoDispatcherFileBackend->Mount(UtocPath, PakOrder, …)` (:6047);
- on success `Pak->IoContainerHeader = MkUnique<FIoContainerHeader>(…)` (:6054)
  then **`PackageStoreBackend->Mount(Pak->IoContainerHeader.Get(), PakOrder)`**
  (**Pak.cpp:6055**) — this is the late merge into the package store.
- Optional-segment container merge (`PackageStoreBackend->Mount(…)` :6066) is
  **`#if WITH_EDITOR`** only → not in shipping.
- Delegate: `FCoreDelegates::OnPakFileMounted2.Broadcast(*Pak)` (Pak.cpp:6130/6134).

The backend is `FFilePackageStoreBackend` (Pak.cpp:5672), registered once at init
via `FPackageStore::Get().Mount(PackageStoreBackend)` (Pak.cpp:5673); each
later pak mount adds its container header to that backend. So **a post-boot
`FPakPlatformFile::Mount` of a .pak+.utoc merges the container into the already-
resolved package store** via `FFilePackageStoreBackend::Mount(header, order)`.
(`FFilePackageStoreBackend` itself lives in CoreUObject AsyncLoading2 /
`Core/Public/IO/PackageStore.h`; its internal `Mount` body was not opened this
pass — name it as a one-more-file fetch if the merge internals are needed.)

---

## 7. Net corroboration for the hook design (PAK-PIN-PASS §6)

| Hook slot (RE) | Source corroboration |
|---|---|
| FileExists +0x80 | **CONFIRMED** at +0x80 (slot16) |
| FileSize +0x88 | **CONFIRMED** at +0x88 (slot17) |
| OpenRead +0xb8 | RE only — 5.6.1 OpenRead(IFileHandle*) is slot31/+0xf8; game vtable tail differs. Trust the binary. |
| IterateDirectory +0x1c0 | RE only — 5.6.1 IterateDirectory(visitor&) is slot38/+0x130. Trust the binary. |
| GetLowerLevel +0x68 / SetLowerLevel +0x70 / GetName +0x78 / ShouldBeUsed +0x20 / Initialize +0x28 | **CONFIRMED** |
| `[mgr+8]` = FPakPlatformFile? | **REFUTED** — it is PhysicalPlatformFile (PFM.h:21/25) |
| MountedPaks @ +0x20, 0x38-byte | **REMAPPED** — +0x20/0x38B = PendingEncryptedPakFiles; real mounted PakFiles @ +0x10/16B |
| utoc/ucas read seam | Generic path = OpenReadNoBuffering/OpenRead on chain top (item 5); platform may override & bypass — Phase-0 decides |
