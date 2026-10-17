// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK
//
// Reader and producer for the signed-executable container. A signed module or executable wraps an
// ELF in a container: a header, a segment table, the ELF header and program headers, extended info,
// and the segment data. A container whose per-segment digest and signature slots are zero-filled is
// accepted by a development console without a real signature; the extended-info digest is a SHA-256
// over the embedded ELF. This type reads such a container back to a plaintext ELF and produces one
// from an ELF, so the inspector and the toolchain handle a signed file the same way as an unsigned
// one. Every container shares one header magic; a container signed for retail marks its data segments
// encrypted, and those cannot be read without the matching key.
//
// A module has to be wrapped in this container to launch. An executable left as a plain ELF is turned
// away by the loader before any of its code runs, so the build pipeline wraps what it ships.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;

namespace SharpProspero.Prx;

/// <summary>One entry of a signed container's segment table.</summary>
/// <param name="Flags">Raw 64-bit flags word.</param>
/// <param name="FileOffset">Offset of the segment data within the container.</param>
/// <param name="FileSize">Stored size of the segment.</param>
/// <param name="MemSize">In-memory size of the segment.</param>
public readonly record struct SelfSegment(ulong Flags, ulong FileOffset, ulong FileSize, ulong MemSize)
{
    /// <summary>Segment id (bits 20..35); a program-header index for a data segment.</summary>
    public int Id => (int)((Flags >> 20) & 0xFFFF);

    /// <summary>Whether the segment is ordered.</summary>
    public bool Ordered => (Flags & 0x1) != 0;

    /// <summary>Whether the segment data is encrypted.</summary>
    public bool Encrypted => (Flags & 0x2) != 0;

    /// <summary>Whether the segment is covered by a signature or digest.</summary>
    public bool Signed => (Flags & 0x4) != 0;

    /// <summary>Whether the segment data is deflate-compressed.</summary>
    public bool Compressed => (Flags & 0x8) != 0;

    /// <summary>Whether the segment is stored in fixed-size blocks. A data segment sets this.</summary>
    public bool Blocked => (Flags & 0x800) != 0;
}

/// <summary>The extended information that follows the program headers in a signed container.</summary>
/// <param name="AuthorityId">Program authority id. A developer-accepted container uses the 0x31.. prefix.</param>
/// <param name="ProgramType">Program type.</param>
/// <param name="AppVersion">Application version.</param>
/// <param name="FirmwareVersion">Firmware version.</param>
/// <param name="Digest">SHA-256 of the embedded ELF.</param>
public sealed record SelfExtInfo(ulong AuthorityId, ulong ProgramType, ulong AppVersion, ulong FirmwareVersion, byte[] Digest);

/// <summary>A parsed signed container.</summary>
/// <param name="ProgramType">Container header program/key type field.</param>
/// <param name="HeaderSize">Size of the header region.</param>
/// <param name="MetaSize">Size of the metadata footer.</param>
/// <param name="FileSize">Total file size recorded in the header.</param>
/// <param name="Segments">Decoded segment table.</param>
/// <param name="Elf">The embedded ELF header and program-header region.</param>
/// <param name="ExtInfo">Extended info, when present.</param>
public sealed record SelfImage(
    uint ProgramType,
    int HeaderSize,
    int MetaSize,
    ulong FileSize,
    IReadOnlyList<SelfSegment> Segments,
    byte[] Elf,
    SelfExtInfo? ExtInfo);

/// <summary>The result of a container integrity check.</summary>
/// <param name="HasDigest">Whether the container carried an extended-info digest to check against.</param>
/// <param name="Matches">Whether the recomputed digest equals the stored one.</param>
/// <param name="Stored">The digest stored in the extended info.</param>
/// <param name="Computed">The digest recomputed from the embedded ELF.</param>
public readonly record struct SelfIntegrity(bool HasDigest, bool Matches, byte[] Stored, byte[] Computed);

/// <summary>Options for <see cref="SelfContainer.Sign"/>.</summary>
public sealed class SelfSignOptions
{
    /// <summary>Application version written to the extended info.</summary>
    public ulong AppVersion { get; init; }

    /// <summary>Firmware version written to the extended info.</summary>
    public ulong FirmwareVersion { get; init; }

    /// <summary>
    /// Overrides the program authority id. When null the developer id is written, which is what a
    /// container carries when it is accepted without a real signature.
    /// </summary>
    public ulong? AuthorityId { get; init; }

    /// <summary>
    /// Normalizes the ELF header before wrapping (machine to x86-64, a System V or GNU OS/ABI to
    /// FreeBSD, an unset type to executable) so a plain ELF is accepted as a module. Only the
    /// 0x40-byte header changes, and the container embeds and digests that normalized ELF, so the
    /// digest stays consistent. Defaults to <see langword="true"/>.
    /// </summary>
    public bool NormalizeHeader { get; init; } = true;
}

/// <summary>
/// Reads and produces the signed-executable container used by modules and executables.
/// </summary>
/// <remarks>
/// Layout, little-endian scalars:
/// <list type="bullet">
/// <item>Header, 0x20 bytes: magic (<c>0x1D3D154F</c> or <c>0xEEF51454</c>), version, mode, endian and attribute bytes,
/// program type, header size, metadata size, file size, segment count, flags.</item>
/// <item>Segment table at 0x20, one 0x20-byte entry per segment: flags, file offset, file size,
/// memory size. Content comes in pairs, a zero-filled digest segment then the data segment; the
/// digest segment holds one 0x20-byte slot per 0x4000-byte block of the data it describes.</item>
/// <item>The ELF header and program headers, copied verbatim.</item>
/// <item>Extended info, 0x40 bytes, after the program headers: authority id, program type, versions,
/// and the SHA-256 of the embedded ELF.</item>
/// <item>A zero-filled metadata footer, then the segment data.</item>
/// </list>
/// </remarks>
public static class SelfContainer
{
    /// <summary>
    /// The container header magic this type writes, at file offset 0x00. Two magics appear on modules,
    /// and each goes together with a particular signature-area size in the metadata region. Modules
    /// carrying either pairing run; one magic with the other's size matches no module measured, so the
    /// two are written together and never separately. This magic pairs with <see cref="SignatureSize"/>.
    /// </summary>
    public const uint Magic = 0xEEF51454;

    /// <summary>
    /// The other container header magic a reader accepts. It pairs with a signature area 0x100 bytes
    /// smaller than <see cref="SignatureSize"/>, so this type reads it but does not write it.
    /// </summary>
    public const uint AlternateMagic = 0x1D3D154F;

    private const int ContainerHeaderSize = 0x20;
    private const int SegEntrySize = 0x20;
    private const int ExtInfoSize = 0x40;
    private const int ControlRegionSize = 0x30;
    // The metadata region that follows the header: one block per segment, then a footer, then the
    // signature area. Its size follows the segment count; a constant that happens to be right for one
    // particular count is wrong for every other, and a module whose metadata region is the wrong size
    // or whose footer sits at the wrong offset is refused while it is being loaded, before any of its
    // code runs and without anything being written to the log.
    private const int MetaBlockSize = 0x50;
    private const int MetaFooterSize = 0x50;
    // The signature area closes the metadata region out. Its size goes with the header magic rather
    // than with the segment count, and it stays zero-filled: nothing reads what occupies it, but the
    // region still has to reach its full length, because the segment data starts where it ends.
    internal const int SignatureSize = 0x200;

    /// <summary>The metadata region size for <paramref name="segmentCount"/> segment-table entries.</summary>
    internal static int MetaSize(int segmentCount) =>
        segmentCount * MetaBlockSize + MetaFooterSize + SignatureSize;

    // A data segment is stored in fixed-size blocks and its paired segment holds one digest slot per
    // block, so the pair's size follows from the data size rather than being fixed. The block size is
    // fixed at 0x4000: the loader divides the data size by it to get a block count and then requires
    // the paired segment to be exactly that many slots, so a pair sized for a single block turns away
    // every segment larger than one block. The segment flags carry a matching block-size selector in
    // bits 12..15 (value 2, its log2 less 12) which the data flags below set for consistency.
    private const int SegmentBlockSize = 0x4000;
    private const int DigestSlotSize = 0x20;
    // Where the marker sits inside the footer, which itself follows the per-segment blocks.
    private const int FooterMarkerOffset = 0x30;
    private const uint DefaultProgramType = 0x00000101;

    private const int ElfHeaderSize = 0x40;
    private const int ElfPhdrSize = 0x38;

    // ELF header field offsets used by the writer and the normalizer.
    private const int EiOsAbi = 0x07;
    private const int OffType = 0x10;
    private const int OffMachine = 0x12;
    private const int OffPhOff = 0x20;
    private const int OffPhEntSize = 0x36;
    private const int OffPhNum = 0x38;
    private const ushort MachineX8664 = 0x003E;
    private const byte OsAbiSystemV = 0x00;
    private const byte OsAbiGnu = 0x03;
    private const byte OsAbiFreeBsd = 0x09;

    /// <summary>
    /// Program authority id a container carries when it is accepted without a real signature. The same
    /// value is used for an executable and for a library; it does not vary with the module type.
    /// </summary>
    public const ulong DeveloperAuthorityId = 0x3100000000000002;

    // Program-header types whose file content becomes a container data segment.
    private const uint PtLoad = 0x00000001;
    private const uint PtModuleData = 0x61000000;
    private const uint PtRelro = 0x61000010;
    private const uint PtComment = 0x6FFFFF00;
    // The record segment that holds the component version entries. It is never mapped, so the container
    // keeps its bytes after the last stored segment rather than as a segment of its own.
    private const uint PtVersionRecords = 0x6FFFFF01;

    /// <summary>Returns whether the buffer begins with a signed-container header.</summary>
    public static bool IsSelf(ReadOnlySpan<byte> data)
    {
        if (data.Length < ContainerHeaderSize)
            return false;
        uint magic = BinaryPrimitives.ReadUInt32LittleEndian(data);
        return magic is Magic or AlternateMagic;
    }

    /// <summary>Returns whether the buffer begins with an ELF header.</summary>
    public static bool IsElf(ReadOnlySpan<byte> data) =>
        data.Length >= ElfHeaderSize && data[0] == 0x7F && data[1] == (byte)'E' && data[2] == (byte)'L' && data[3] == (byte)'F';

    /// <summary>
    /// Returns whether any data segment of a container stores its bytes encrypted. Every container
    /// shares one magic; whether the contents can be read is a per-segment property, so a container
    /// signed for a development console and one signed for retail are told apart by this, not by the
    /// header.
    /// </summary>
    public static bool HasEncryptedSegments(ReadOnlySpan<byte> data)
    {
        if (!IsSelf(data))
            return false;
        int segCount = BinaryPrimitives.ReadUInt16LittleEndian(data[0x18..]);
        for (int i = 0; i < segCount; i++)
        {
            int e = ContainerHeaderSize + i * SegEntrySize;
            if (e + SegEntrySize > data.Length)
                break;
            if ((BinaryPrimitives.ReadUInt64LittleEndian(data[e..]) & 0x2) != 0)
                return true;
        }
        return false;
    }

    /// <summary>Classifies a file as one of the module and executable forms.</summary>
    public static ModuleForm Classify(ReadOnlySpan<byte> data)
    {
        if (IsSelf(data))
            return HasEncryptedSegments(data) ? ModuleForm.SignedEncrypted : ModuleForm.SignedPlaintext;
        if (IsElf(data))
            return ModuleForm.UnsignedElf;
        return ModuleForm.Unknown;
    }

    /// <summary>Parses a signed container.</summary>
    /// <exception cref="PrxFormatException">The buffer is not a structurally valid container.</exception>
    public static SelfImage Parse(ReadOnlySpan<byte> data)
    {
        if (!Validate(data, out string? error))
            throw new PrxFormatException(error!);

        uint programType = BinaryPrimitives.ReadUInt32LittleEndian(data[0x08..]);
        int headerSize = BinaryPrimitives.ReadUInt16LittleEndian(data[0x0C..]);
        int metaSize = BinaryPrimitives.ReadUInt16LittleEndian(data[0x0E..]);
        ulong fileSize = BinaryPrimitives.ReadUInt64LittleEndian(data[0x10..]);
        int segCount = BinaryPrimitives.ReadUInt16LittleEndian(data[0x18..]);

        var segments = new List<SelfSegment>(segCount);
        for (int i = 0; i < segCount; i++)
        {
            int e = ContainerHeaderSize + i * SegEntrySize;
            segments.Add(new SelfSegment(
                BinaryPrimitives.ReadUInt64LittleEndian(data[e..]),
                BinaryPrimitives.ReadUInt64LittleEndian(data[(e + 0x08)..]),
                BinaryPrimitives.ReadUInt64LittleEndian(data[(e + 0x10)..]),
                BinaryPrimitives.ReadUInt64LittleEndian(data[(e + 0x18)..])));
        }

        int elfStart = ContainerHeaderSize + segCount * SegEntrySize;
        SelfExtInfo? extInfo = null;
        byte[] elf = [];
        if (IsElf(data[elfStart..]))
        {
            int phnum = BinaryPrimitives.ReadUInt16LittleEndian(data[(elfStart + OffPhNum)..]);
            int elfLen = ElfHeaderSize + phnum * ElfPhdrSize;
            if (elfStart + elfLen <= data.Length)
            {
                elf = data.Slice(elfStart, elfLen).ToArray();
                int extStart = AlignUp(elfStart + elfLen, 0x10);
                if (extStart + ExtInfoSize <= headerSize && extStart + ExtInfoSize <= data.Length)
                {
                    extInfo = new SelfExtInfo(
                        BinaryPrimitives.ReadUInt64LittleEndian(data[extStart..]),
                        BinaryPrimitives.ReadUInt64LittleEndian(data[(extStart + 0x08)..]),
                        BinaryPrimitives.ReadUInt64LittleEndian(data[(extStart + 0x10)..]),
                        BinaryPrimitives.ReadUInt64LittleEndian(data[(extStart + 0x18)..]),
                        data.Slice(extStart + 0x20, 0x20).ToArray());
                }
            }
        }

        return new SelfImage(programType, headerSize, metaSize, fileSize, segments, elf, extInfo);
    }

    /// <summary>Validates the header and segment table of a signed container.</summary>
    public static bool Validate(ReadOnlySpan<byte> data, out string? error)
    {
        if (data.Length < ContainerHeaderSize) { error = "Buffer is smaller than the container header."; return false; }
        uint magic = BinaryPrimitives.ReadUInt32LittleEndian(data);
        if (magic is not (Magic or AlternateMagic)) { error = "File is not a signed container."; return false; }

        int headerSize = BinaryPrimitives.ReadUInt16LittleEndian(data[0x0C..]);
        int segCount = BinaryPrimitives.ReadUInt16LittleEndian(data[0x18..]);
        long tableEnd = ContainerHeaderSize + (long)segCount * SegEntrySize;
        if (tableEnd > data.Length) { error = "The segment table overruns the buffer."; return false; }
        if (headerSize > data.Length) { error = "The header size exceeds the buffer."; return false; }
        error = null;
        return true;
    }

    /// <summary>
    /// Reconstructs the plaintext ELF embedded in a signed container. The stored ELF header and
    /// program headers are copied out, then each data segment is written back at the file offset its
    /// program header records, inflating a deflate-stored segment first. The result is a flat ELF
    /// whose dynamic table, symbol table and parameter block read directly.
    /// </summary>
    /// <param name="data">A signed container.</param>
    /// <returns>The reconstructed ELF file bytes.</returns>
    /// <exception cref="PrxFormatException">
    /// The container is not valid, carries no readable ELF header, or stores encrypted segment data,
    /// which cannot be recovered without its key.
    /// </exception>
    public static byte[] ExtractElf(ReadOnlySpan<byte> data)
    {
        SelfImage image = Parse(data);
        if (image.Elf.Length < ElfHeaderSize)
            throw new PrxFormatException("The container carries no readable ELF header.");

        int phnum = BinaryPrimitives.ReadUInt16LittleEndian(image.Elf.AsSpan(OffPhNum));
        int phTableEnd = ElfHeaderSize + phnum * ElfPhdrSize;
        if (phTableEnd > image.Elf.Length)
            throw new PrxFormatException("The stored program-header table is incomplete.");

        // Size the output to the furthest extent any data segment writes, never below the header
        // region that is copied first.
        long size = image.Elf.Length;
        foreach (SelfSegment seg in image.Segments)
        {
            if (!seg.Blocked)
                continue; // a digest segment, not payload
            int index = seg.Id;
            if (index < 0 || index >= phnum)
                continue;
            int ph = ElfHeaderSize + index * ElfPhdrSize;
            ulong pOffset = BinaryPrimitives.ReadUInt64LittleEndian(image.Elf.AsSpan(ph + 0x08));
            ulong pFilesz = BinaryPrimitives.ReadUInt64LittleEndian(image.Elf.AsSpan(ph + 0x20));
            // Reject a program header that places a segment past a readable range before the extent
            // sizes the output; the arithmetic below and the casts in the next pass must not wrap.
            if (pOffset > int.MaxValue || pFilesz > (ulong)int.MaxValue - pOffset)
                throw new PrxFormatException("A program header places a segment outside a readable range.");
            size = Math.Max(size, (long)(pOffset + pFilesz));
        }

        // A program header can reserve file space no data segment carries - a non-loaded note in the
        // tail. Its bytes are not stored, but the reconstructed file must still reach that extent so a
        // digest taken over the whole image round-trips; the zero-initialized output supplies the fill.
        for (int i = 0; i < phnum; i++)
        {
            int ph = ElfHeaderSize + i * ElfPhdrSize;
            ulong pOffset = BinaryPrimitives.ReadUInt64LittleEndian(image.Elf.AsSpan(ph + 0x08));
            ulong pFilesz = BinaryPrimitives.ReadUInt64LittleEndian(image.Elf.AsSpan(ph + 0x20));
            if (pOffset > int.MaxValue || pFilesz > (ulong)int.MaxValue - pOffset)
                continue;
            size = Math.Max(size, (long)(pOffset + pFilesz));
        }

        var output = new byte[size];
        foreach (SelfSegment seg in image.Segments)
        {
            if (!seg.Blocked)
                continue;
            if (seg.Encrypted)
                throw new PrxFormatException(
                    "A container segment is encrypted; a signed retail module cannot be read without its key.");

            int index = seg.Id;
            if (index < 0 || index >= phnum)
                continue;
            int ph = ElfHeaderSize + index * ElfPhdrSize;
            ulong pOffset = BinaryPrimitives.ReadUInt64LittleEndian(image.Elf.AsSpan(ph + 0x08));
            ulong pFilesz = BinaryPrimitives.ReadUInt64LittleEndian(image.Elf.AsSpan(ph + 0x20));

            if (!RangeInBounds(seg.FileOffset, seg.FileSize, data.Length))
                throw new PrxFormatException("A container segment overruns the file.");
            byte[] stored = data.Slice((int)seg.FileOffset, (int)seg.FileSize).ToArray();

            // pOffset and pFilesz were bounded when the output was sized, so these casts do not wrap.
            byte[] payload = seg.Compressed ? Inflate(stored, (int)pFilesz) : stored;
            int copy = (int)Math.Min((ulong)payload.Length, pFilesz);
            if (!RangeInBounds(pOffset, (ulong)copy, size))
                throw new PrxFormatException("A container segment is placed outside the reconstructed file.");
            payload.AsSpan(0, copy).CopyTo(output.AsSpan((int)pOffset));
        }

        // The version records sit after the last stored segment rather than in a segment of their own,
        // so put them back where the program header places them. Without this the rebuilt module is
        // short exactly those bytes and no digest over it agrees with the one the container carries.
        (int versionOffset, int versionLength) = FindVersionRecords(image.Elf, phnum, size);
        if (versionLength > 0)
        {
            int tail = LastStoredEnd(image.Segments);
            if (tail > 0 && RangeInBounds((ulong)tail, (ulong)versionLength, data.Length)
                && RangeInBounds((ulong)versionOffset, (ulong)versionLength, size))
                data.Slice(tail, versionLength).CopyTo(output.AsSpan(versionOffset));
        }

        // The stored ELF header and program headers are authoritative for the first region.
        image.Elf.AsSpan(0, phTableEnd).CopyTo(output);
        return output;
    }

    // Where the last stored segment ends, which is where the version records begin.
    private static int LastStoredEnd(IReadOnlyList<SelfSegment> segments)
    {
        long end = 0;
        foreach (SelfSegment seg in segments)
        {
            if (seg.FileOffset > int.MaxValue || seg.FileSize > (ulong)int.MaxValue - seg.FileOffset)
                continue;
            end = Math.Max(end, (long)(seg.FileOffset + seg.FileSize));
        }
        return end > int.MaxValue ? 0 : (int)end;
    }

    /// <summary>
    /// Rebuilds the image the container carries and compares its SHA-256 with the digest stored in the
    /// extended info, so a reader holding nothing but the container can tell whether it is intact. The
    /// result's <c>HasDigest</c> is false when the container carries no extended-info digest.
    /// </summary>
    /// <remarks>
    /// The digest covers what the container carries, not the file it was made from. Those differ by the
    /// record-keeping a module holds between and after its stored segments, which the container leaves
    /// behind, so checking against the original file reports a mismatch for every real module.
    /// </remarks>
    /// <param name="data">The container file bytes.</param>
    public static SelfIntegrity CheckIntegrity(ReadOnlySpan<byte> data)
    {
        SelfImage image = Parse(data);
        if (image.ExtInfo is not SelfExtInfo ext || ext.Digest is null || ext.Digest.Length != 32)
            return new SelfIntegrity(false, false, [], []);
        byte[] computed = SHA256.HashData(ExtractElf(data));
        bool matches = computed.AsSpan().SequenceEqual(ext.Digest);
        return new SelfIntegrity(true, matches, ext.Digest, computed);
    }

    /// <summary>
    /// Wraps an ELF in a signed container whose digest and signature slots are zero-filled, which a
    /// development console accepts. The extended-info digest is a SHA-256 over the embedded ELF, so
    /// the output is fully determined by the input and the options.
    /// </summary>
    /// <param name="elf">The input ELF file bytes.</param>
    /// <param name="options">Version and authority overrides.</param>
    /// <returns>The container file bytes.</returns>
    /// <exception cref="PrxFormatException">The input is not a supported ELF.</exception>

    public static byte[] Sign(byte[] elf, SelfSignOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(elf);
        options ??= new SelfSignOptions();
        if (!IsElf(elf))
            throw new PrxFormatException("Input is not an ELF file.");
        if (elf[4] != 2)
            throw new PrxFormatException("Only 64-bit ELF modules are supported.");

        // Normalize on a private copy so the caller's buffer is never mutated; the container embeds
        // and digests this normalized ELF.
        if (options.NormalizeHeader)
        {
            elf = (byte[])elf.Clone();
            NormalizeHeader(elf);
        }

        int phoff = (int)BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(OffPhOff));
        int phentSize = BinaryPrimitives.ReadUInt16LittleEndian(elf.AsSpan(OffPhEntSize));
        int phnum = BinaryPrimitives.ReadUInt16LittleEndian(elf.AsSpan(OffPhNum));
        if (phentSize != ElfPhdrSize)
            throw new PrxFormatException($"Unexpected ELF program-header size {phentSize}.");
        if (phoff + phnum * ElfPhdrSize > elf.Length)
            throw new PrxFormatException("The ELF program headers overrun the file.");
        // The header region embeds the ELF header and its program headers as one contiguous block, so
        // the program-header table must directly follow the 0x40-byte ELF header.
        if (phoff != ElfHeaderSize)
            throw new PrxFormatException(
                $"The ELF program-header table must follow the ELF header at 0x{ElfHeaderSize:X} (e_phoff is 0x{phoff:X}).");

        var selected = SelectSegments(elf, phoff, phnum);
        if (selected.Count == 0)
            throw new PrxFormatException("The ELF has no loadable segment content.");

        int segCount = selected.Count * 2;
        int afterSeg = ContainerHeaderSize + segCount * SegEntrySize;
        int elfHdrLen = ElfHeaderSize + phnum * ElfPhdrSize;
        int extInfoStart = AlignUp(afterSeg + elfHdrLen, 0x10);
        int headerSize = extInfoStart + ExtInfoSize + ControlRegionSize;
        int metaSize = MetaSize(segCount);
        int dataStart = headerSize + metaSize;

        // The header stores headerSize and metaSize as u16 fields; an ELF with enough program headers
        // to overflow them cannot be represented, so fail rather than truncate the sizes.
        if (headerSize > ushort.MaxValue || metaSize > ushort.MaxValue)
            throw new PrxFormatException(
                $"The ELF has too many segments to sign (header 0x{headerSize:X}, meta 0x{metaSize:X} exceed the 16-bit container fields).");

        // Assign segment file offsets: the digest segment for a content segment, then the content
        // itself padded to 16, per pair. A digest segment carries one slot per block of its content.
        var segOffsets = new int[segCount];
        var digestSizes = new int[selected.Count];
        int cursor = dataStart;
        for (int k = 0; k < selected.Count; k++)
        {
            digestSizes[k] = DigestSize(selected[k].FileSize);
            segOffsets[k * 2] = cursor;
            cursor += digestSizes[k];
            segOffsets[k * 2 + 1] = cursor;
            cursor = AlignUp(cursor + selected[k].FileSize, 0x10);
        }
        int fileSize = cursor;

        // A module keeps its version records in a segment that is never mapped, so no container segment
        // carries them and the offset the program header records means nothing once the module is
        // wrapped. The records follow the last stored segment instead, starting where it ends rather
        // than on the next boundary, and they sit past the size the header declares.
        int versionStart = selected.Count == 0
            ? cursor
            : segOffsets[^1] + selected[^1].FileSize;
        (int versionOffset, int versionLength) = FindVersionRecords(elf, phnum, elf.Length);

        var buffer = new byte[Math.Max(fileSize, versionStart + versionLength)];
        Span<byte> span = buffer.AsSpan();

        BinaryPrimitives.WriteUInt32LittleEndian(span, Magic);
        span[0x04] = 0;    // version
        span[0x05] = 1;    // mode
        span[0x06] = 1;    // endian
        span[0x07] = 0x12; // attr
        BinaryPrimitives.WriteUInt32LittleEndian(span[0x08..], DefaultProgramType);
        BinaryPrimitives.WriteUInt16LittleEndian(span[0x0C..], (ushort)headerSize);
        BinaryPrimitives.WriteUInt16LittleEndian(span[0x0E..], (ushort)metaSize);
        BinaryPrimitives.WriteUInt64LittleEndian(span[0x10..], (ulong)fileSize);
        BinaryPrimitives.WriteUInt16LittleEndian(span[0x18..], (ushort)segCount);
        BinaryPrimitives.WriteUInt16LittleEndian(span[0x1A..], 0x0022); // flags

        for (int k = 0; k < selected.Count; k++)
        {
            int digestEntry = ContainerHeaderSize + (k * 2) * SegEntrySize;
            int dataEntry = ContainerHeaderSize + (k * 2 + 1) * SegEntrySize;
            int dataTableIndex = k * 2 + 1;

            ulong digestFlags = ((ulong)dataTableIndex << 20) | 0x10004;
            WriteSegment(span, digestEntry, digestFlags, (ulong)segOffsets[k * 2],
                (ulong)digestSizes[k], (ulong)digestSizes[k]);

            ulong dataFlags = ((ulong)selected[k].PhdrIndex << 20) | 0x2804;
            WriteSegment(span, dataEntry, dataFlags, (ulong)segOffsets[k * 2 + 1],
                (ulong)selected[k].FileSize, (ulong)selected[k].FileSize);
        }

        elf.AsSpan(0, elfHdrLen).CopyTo(span[afterSeg..]);

        ulong authorityId = options.AuthorityId ?? DeveloperAuthorityId;
        BinaryPrimitives.WriteUInt64LittleEndian(span[extInfoStart..], authorityId);
        BinaryPrimitives.WriteUInt64LittleEndian(span[(extInfoStart + 0x08)..], 1); // program type
        BinaryPrimitives.WriteUInt64LittleEndian(span[(extInfoStart + 0x10)..], options.AppVersion);
        BinaryPrimitives.WriteUInt64LittleEndian(span[(extInfoStart + 0x18)..], options.FirmwareVersion);
        // The digest is written once the rest of the container is in place, below.

        BinaryPrimitives.WriteUInt64LittleEndian(span[(extInfoStart + ExtInfoSize)..], 3); // control block type

        // The footer follows the per-segment blocks, so its position moves with the segment count.
        int footerStart = headerSize + segCount * MetaBlockSize;
        BinaryPrimitives.WriteUInt32LittleEndian(span[(footerStart + FooterMarkerOffset)..], 0x00010000);

        for (int k = 0; k < selected.Count; k++)
            elf.AsSpan(selected[k].FileOffset, selected[k].FileSize).CopyTo(span[segOffsets[k * 2 + 1]..]);

        if (versionLength > 0)
            elf.AsSpan(versionOffset, versionLength).CopyTo(span[versionStart..]);

        // The digest covers the image the container carries, not the file it was made from. The two
        // differ: a module holds record-keeping between and after its stored segments - its section
        // table, and the records in its tail - that the container does not carry, so a digest over the
        // file describes something no reader ever sees again. Every module that starts carries the
        // first, and none carries the second.
        //
        // It is taken over the reconstruction rather than over a second assembly of the same bytes, so
        // the value written here and the value a reader arrives at cannot drift apart: they come from
        // one routine. The digest itself sits in the header, outside what is reconstructed, so writing
        // it afterwards does not change what it covers.
        SHA256.HashData(ExtractElf(buffer)).CopyTo(span[(extInfoStart + 0x20)..]);
        return buffer;
    }

    // The version records a module keeps in its unmapped record segment, as an offset and length into
    // the module. A module without that segment reports a zero length and nothing is appended.
    // <paramref name="limit"/> is how far the records are allowed to reach: the module's own length when
    // the whole module is at hand, and the rebuilt extent when only the header region is.
    private static (int Offset, int Length) FindVersionRecords(byte[] elf, int phnum, long limit)
    {
        for (int i = 0; i < phnum; i++)
        {
            int p = ElfHeaderSize + i * ElfPhdrSize;
            if (BinaryPrimitives.ReadUInt32LittleEndian(elf.AsSpan(p)) != PtVersionRecords)
                continue;
            ulong off = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(p + 0x08));
            ulong size = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(p + 0x20));
            if (size == 0 || off > int.MaxValue || size > (ulong)int.MaxValue - off
                || off + size > (ulong)limit)
                return (0, 0);
            return ((int)off, (int)size);
        }
        return (0, 0);
    }

    private readonly record struct SelectedSegment(int PhdrIndex, int FileOffset, int FileSize);

    private static List<SelectedSegment> SelectSegments(byte[] elf, int phoff, int phnum)
    {
        var result = new List<SelectedSegment>();
        for (int i = 0; i < phnum; i++)
        {
            int p = phoff + i * ElfPhdrSize;
            uint pType = BinaryPrimitives.ReadUInt32LittleEndian(elf.AsSpan(p));
            ulong off = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(p + 0x08));
            ulong fsz = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(p + 0x20));
            // A segment storing nothing is carried as a pair of zero-length entries sharing one file
            // offset with whatever follows. Such an entry's digest table covers no blocks, and two
            // entries at one offset leave the table no longer ascending. A module that stores nothing
            // in a mapped segment is malformed at the source, so it is refused here rather than
            // wrapped into that shape.
            if (fsz == 0)
            {
                if (pType == PtLoad && BinaryPrimitives.ReadUInt32LittleEndian(elf.AsSpan(p + 4)) != 0)
                    throw new PrxFormatException(
                        $"Program header {i} is mapped but stores nothing. A container carries such a segment as a " +
                        "zero-length entry, and a module carrying one does not start.");
                continue;
            }
            if (pType != PtLoad && pType != PtModuleData && pType != PtRelro && pType != PtComment)
                continue;
            // A segment the container has to carry, whose bytes are not in the module. Passing over it
            // writes a container missing a segment the loader needs and says nothing, so what comes out
            // reads as a module and is not one.
            if (!RangeInBounds(off, fsz, elf.Length))
                throw new PrxFormatException(
                    $"Program header {i} stores {fsz} bytes at 0x{off:X}, past the end of the {elf.Length}-byte module.");
            result.Add(new SelectedSegment(i, (int)off, (int)fsz));
        }
        return result;
    }

    // Sets the machine to x86-64, a System V or GNU OS/ABI to FreeBSD, and an unset type to
    // executable, without touching segment content.
    private static void NormalizeHeader(byte[] elf)
    {
        BinaryPrimitives.WriteUInt16LittleEndian(elf.AsSpan(OffMachine), MachineX8664);
        if (elf[EiOsAbi] is OsAbiSystemV or OsAbiGnu)
            elf[EiOsAbi] = OsAbiFreeBsd;
        if (BinaryPrimitives.ReadUInt16LittleEndian(elf.AsSpan(OffType)) == 0)
            BinaryPrimitives.WriteUInt16LittleEndian(elf.AsSpan(OffType), 0x02);
    }

    private static byte[] Inflate(byte[] stored, int expected)
    {
        if (expected == 0)
            return [];
        // A container may deflate a segment with or without a zlib wrapper; try the wrapped stream
        // first, then a raw stream. A result of a different length is rejected so wrong bytes never
        // reach the caller.
        if (TryInflate(stored, expected, wrapped: true, out byte[] result) ||
            TryInflate(stored, expected, wrapped: false, out result))
            return result;
        throw new PrxFormatException("A compressed container segment could not be inflated to its recorded size.");
    }

    private static bool TryInflate(byte[] input, int expected, bool wrapped, out byte[] result)
    {
        result = [];
        try
        {
            using var ms = new MemoryStream(input, writable: false);
            using Stream decode = wrapped
                ? new ZLibStream(ms, CompressionMode.Decompress)
                : new DeflateStream(ms, CompressionMode.Decompress);
            var buffer = new byte[expected];
            int total = 0, read;
            while (total < expected && (read = decode.Read(buffer, total, expected - total)) > 0)
                total += read;
            if (total == expected && decode.ReadByte() < 0)
            {
                result = buffer;
                return true;
            }
        }
        catch (InvalidDataException)
        {
        }
        return false;
    }

    private static void WriteSegment(Span<byte> span, int entry, ulong flags, ulong offset, ulong fileSize, ulong memSize)
    {
        BinaryPrimitives.WriteUInt64LittleEndian(span[entry..], flags);
        BinaryPrimitives.WriteUInt64LittleEndian(span[(entry + 0x08)..], offset);
        BinaryPrimitives.WriteUInt64LittleEndian(span[(entry + 0x10)..], fileSize);
        BinaryPrimitives.WriteUInt64LittleEndian(span[(entry + 0x18)..], memSize);
    }

    private static int AlignUp(int value, int alignment) => (value + alignment - 1) & ~(alignment - 1);

    // The size of the digest segment that pairs with a data segment of dataSize bytes: one slot per
    // stored block, rounded up. A segment carrying nothing has no blocks and so no slots.
    private static int DigestSize(int dataSize) =>
        (dataSize + SegmentBlockSize - 1) / SegmentBlockSize * DigestSlotSize;

    // True when [offset, offset + size) lies within [0, length]; false for any value out of range or
    // that would wrap. Every offset and size read from a container or an ELF is bounded through this
    // before it indexes a buffer, so a malformed file is rejected rather than throwing an index error.
    private static bool RangeInBounds(ulong offset, ulong size, long length) =>
        length >= 0 && offset <= (ulong)length && size <= (ulong)length - offset;
}
