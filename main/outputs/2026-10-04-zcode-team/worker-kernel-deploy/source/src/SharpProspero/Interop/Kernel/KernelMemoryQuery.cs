// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Kernel;

/// <summary>
/// What a virtual-address query reports about the mapping covering an address: its bounds, what backs
/// it, how it is protected and the name it was tagged with.
/// </summary>
/// <remarks>
/// The seven kind bits share one byte, so they are read through <see cref="Flags"/> and the
/// <c>Is*</c> properties rather than being separate fields.
/// </remarks>
[StructLayout(LayoutKind.Explicit, Size = 72)]
public unsafe struct SceKernelVirtualQueryInfo
{
    /// <summary>The first address of the mapping.</summary>
    [FieldOffset(0)] public void* Start;

    /// <summary>One past the last address of the mapping.</summary>
    [FieldOffset(8)] public void* End;

    /// <summary>The physical offset the mapping is backed from, when direct memory backs it.</summary>
    [FieldOffset(16)] public long Offset;

    /// <summary>The protection bits, the <c>Prot*</c> values on <see cref="KernelMemory"/>.</summary>
    [FieldOffset(24)] public int Protection;

    /// <summary>The memory type, one of the <c>MemoryType*</c> values on <see cref="KernelMemory"/>.</summary>
    [FieldOffset(28)] public int MemoryType;

    /// <summary>The seven kind bits packed into one byte. Read them through the <c>Is*</c> properties.</summary>
    [FieldOffset(32)] public byte Flags;

    /// <summary>The name the range was tagged with, NUL-terminated ASCII, at most 32 bytes with its terminator.</summary>
    [FieldOffset(33)] public fixed byte Name[32];

    /// <summary>Which of the graphics-side address spaces the mapping belongs to.</summary>
    [FieldOffset(65)] public byte GpuMaskId;

    /// <summary>Flexible memory backs the mapping. Bit 0 of <see cref="Flags"/>.</summary>
    public bool IsFlexibleMemory => (Flags & 0x01) != 0;

    /// <summary>Direct memory backs the mapping. Bit 1 of <see cref="Flags"/>.</summary>
    public bool IsDirectMemory => (Flags & 0x02) != 0;

    /// <summary>The mapping is a thread stack. Bit 2 of <see cref="Flags"/>.</summary>
    public bool IsStack => (Flags & 0x04) != 0;

    /// <summary>The mapping came out of a memory pool. Bit 3 of <see cref="Flags"/>.</summary>
    public bool IsPooledMemory => (Flags & 0x08) != 0;

    /// <summary>The range has memory behind it rather than being a reservation. Bit 4 of <see cref="Flags"/>.</summary>
    public bool IsCommitted => (Flags & 0x10) != 0;

    /// <summary>The mapping is a partially resident graphics range. Bit 5 of <see cref="Flags"/>.</summary>
    public bool IsGpuPrt => (Flags & 0x20) != 0;

    /// <summary>The mapping is accounted to the automatic memory manager. Bit 6 of <see cref="Flags"/>.</summary>
    public bool IsAmmUsage => (Flags & 0x40) != 0;
}

/// <summary>What a direct-memory query reports about the region covering a physical offset.</summary>
[StructLayout(LayoutKind.Sequential, Size = 24)]
public struct SceKernelDirectMemoryQueryInfo
{
    /// <summary>The first physical offset of the region.</summary>
    public long Start;

    /// <summary>One past the last physical offset of the region.</summary>
    public long End;

    /// <summary>The memory type the region was reserved with.</summary>
    public int MemoryType;
}

/// <summary>How many blocks a memory pool holds and how many are still free.</summary>
[StructLayout(LayoutKind.Sequential, Size = 16)]
public struct SceKernelMemoryPoolBlockStats
{
    /// <summary>Free blocks that carry no cached contents.</summary>
    public int AvailableFlushedBlocks;

    /// <summary>Free blocks that still carry cached contents.</summary>
    public int AvailableCachedBlocks;

    /// <summary>Blocks handed out that carry no cached contents.</summary>
    public int AllocatedFlushedBlocks;

    /// <summary>Blocks handed out that still carry cached contents.</summary>
    public int AllocatedCachedBlocks;
}

public static unsafe partial class KernelMemory
{
    /// <summary>Bytes a range name may take, including its terminator. Value 32.</summary>
    public const int VirtualRangeNameSize = 32;

    /// <summary>
    /// Query flag: when the address is not inside a mapping, report the next mapping above it rather
    /// than failing. Walking the whole address space is this flag plus the end of each mapping. Value 1.
    /// </summary>
    public const int QueryFindNext = 1;

    /// <summary>Write the pages out and wait for them. Value 0.</summary>
    public const int MsyncSynchronous = 0;

    /// <summary>Start writing the pages out and return at once. Value 1.</summary>
    public const int MsyncAsynchronous = 1;

    /// <summary>Drop the cached copies of the pages. Value 2.</summary>
    public const int MsyncInvalidate = 2;

    /// <summary>Lock what is mapped now. Value 1.</summary>
    public const int MemoryLockCurrent = 1;

    /// <summary>Lock what gets mapped later as well. Value 2.</summary>
    public const int MemoryLockFuture = 2;

    /// <summary>No advice: undo whatever was set. Value 0.</summary>
    public const int AdviseNormal = 0;

    /// <summary>The range will be touched in no particular order. Value 1.</summary>
    public const int AdviseRandom = 1;

    /// <summary>The range will be walked from one end to the other. Value 2.</summary>
    public const int AdviseSequential = 2;

    /// <summary>The range is about to be read. Value 3.</summary>
    public const int AdviseWillNeed = 3;

    /// <summary>The range will not be read again soon. Value 4.</summary>
    public const int AdviseDontNeed = 4;

    /// <summary>Keep the range out of a crash report. Value 8.</summary>
    public const int AdviseNoCore = 8;

    /// <summary>Put the range back into a crash report. Value 9.</summary>
    public const int AdviseCore = 9;

    /// <summary>Map at exactly the address given rather than near it. Value 0x0010.</summary>
    public const int MapFixed = 0x0010;

    /// <summary>Fail rather than replace a mapping already covering the address. Value 0x0080.</summary>
    public const int MapNoOverwrite = 0x0080;

    /// <summary>The first address an application may map at. Value 0x1000000000.</summary>
    public const ulong MapAreaStart = 0x1000000000;

    /// <summary>One past the last address an application may map at. Value 0xfc00000000.</summary>
    public const ulong MapAreaEnd = 0xfc00000000;

    /// <summary>
    /// The mapping flag that asks for an alignment of two to the power
    /// <paramref name="shift"/> bytes, for a shift up to 31.
    /// </summary>
    public static int MapAligned(int shift) => shift << 24;

    /// <summary>On success writes the flexible memory the module was configured with, in bytes.</summary>
    /// <remarks>
    /// This is the ceiling the module was built with, not what is left;
    /// <see cref="sceKernelAvailableFlexibleMemorySize"/> reports what is left.
    /// </remarks>
    [LibraryImport(Lib)]
    public static partial int sceKernelConfiguredFlexibleMemorySize(nuint* sizeOut);

    /// <summary>
    /// Describes the direct-memory region covering <paramref name="offset"/>, writing a
    /// <see cref="SceKernelDirectMemoryQueryInfo"/> to <paramref name="info"/>. Pass
    /// <see cref="QueryFindNext"/> to step to the next region when the offset is inside no region.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDirectMemoryQuery(long offset, int flags, void* info, nuint infoSize);

    /// <summary>
    /// Reads the memory type of the direct-memory region covering <paramref name="start"/> and the
    /// bounds of that region.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelGetDirectMemoryType(
        long start, int* memoryType, long* regionStartOut, long* regionEndOut);

    /// <summary>
    /// Reports whether <paramref name="address"/> is inside a thread stack, and if so the bounds of it.
    /// </summary>
    /// <returns>Zero when it is a stack, or a negative error code when it is not.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelIsStack(void* address, void** start, void** end);

    /// <summary>
    /// Reads the bounds and protection of the mapping covering <paramref name="address"/>. Cheaper than
    /// a full query when only the protection is wanted.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelQueryMemoryProtection(void* address, void** start, void** end, int* protection);

    /// <summary>
    /// Tags the range at <paramref name="start"/> with <paramref name="name"/> (null-terminated UTF-8,
    /// at most <see cref="VirtualRangeNameSize"/> bytes with its terminator) so a memory report and a
    /// virtual query name it.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelSetVirtualRangeName(void* start, nuint length, byte* name);

    /// <summary>
    /// Reads how many page-table entries the module was given for each side and how many are left. A
    /// build that maps many small ranges runs out of these before it runs out of memory.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelGetPageTableStats(
        int* cpuTotal, int* cpuAvailable, int* gpuTotal, int* gpuAvailable);

    /// <summary>
    /// Writes the range at <paramref name="address"/> out to what backs it, or drops its cached copies,
    /// as <paramref name="flags"/> chooses.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelMsync(void* address, nuint length, int flags);

    /// <summary>
    /// Changes both the memory type and the protection of an existing mapping.
    /// <see cref="sceKernelMprotect"/> changes only the protection.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelMtypeprotect(void* address, nuint size, int memoryType, int protection);

    /// <summary>
    /// Reserves <paramref name="length"/> bytes of direct memory anywhere in the main pool, aligned to
    /// <paramref name="alignment"/>. Shorter than <see cref="sceKernelAllocateDirectMemory"/> when the
    /// caller does not care where the memory comes from.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelAllocateMainDirectMemory(
        nuint length, nuint alignment, int memoryType, long* physicalAddressOut);

    /// <summary>
    /// Maps direct memory as <see cref="sceKernelMapDirectMemory"/> does, tagging the mapping with
    /// <paramref name="name"/> so a memory report can name it.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelMapNamedDirectMemory(
        void** address, nuint length, int protection, int flags,
        long directMemoryStart, nuint alignment, byte* name);

    /// <summary>
    /// Releases a reserved region and fails rather than succeeding quietly when the range given is not
    /// exactly one that was reserved.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelCheckedReleaseDirectMemory(long start, nuint length);

    /// <summary>Reads the block counts of the module's memory pool.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelMemoryPoolGetBlockStats(SceKernelMemoryPoolBlockStats* output, nuint outputSize);

    // The five below carry the names the module publishes them under. The same module file publishes
    // them in a library of its own rather than in the kernel's, which the link settles from the name.
    // There is no kernel-prefixed spelling of these that an application may link: the header declares
    // one, but the link-time archives do not carry it, so these are the only way to reach them.

    /// <summary>
    /// Keeps the range at <paramref name="address"/> in memory so a touch of it never stalls.
    /// </summary>
    /// <returns>Zero on success, or -1.</returns>
    /// <remarks>
    /// A sandboxed application is not guaranteed the right to pin memory; treat a refusal as expected
    /// and carry on rather than failing the caller.
    /// </remarks>
    [LibraryImport(Lib)]
    public static partial int mlock(void* address, nuint length);

    /// <summary>Undoes a <see cref="mlock"/>.</summary>
    /// <returns>Zero on success, or -1.</returns>
    [LibraryImport(Lib)]
    public static partial int munlock(void* address, nuint length);

    /// <summary>
    /// Keeps the whole address space in memory, as <paramref name="flags"/> chooses between what is
    /// mapped now and what is mapped later.
    /// </summary>
    /// <returns>Zero on success, or -1.</returns>
    /// <remarks>Refused for a process that has not been granted the right; see <see cref="mlock"/>.</remarks>
    [LibraryImport(Lib)]
    public static partial int mlockall(int flags);

    /// <summary>Undoes a <see cref="mlockall"/>.</summary>
    /// <returns>Zero on success, or -1.</returns>
    [LibraryImport(Lib)]
    public static partial int munlockall();

    /// <summary>
    /// Tells the system how the range at <paramref name="address"/> will be used, one of the
    /// <c>Advise*</c> values. Advice only, never required for correctness.
    /// </summary>
    /// <returns>Zero on success, or -1.</returns>
    [LibraryImport(Lib)]
    public static partial int madvise(void* address, nuint length, int advice);
}
