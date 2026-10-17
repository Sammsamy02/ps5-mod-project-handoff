// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Content;

/// <summary>The kind of a piece of content in the library.</summary>
public enum SceContentSearchContentType
{
    /// <summary>Not a recognized kind.</summary>
    Unknown = 0,

    /// <summary>A photo.</summary>
    Photo = 1,

    /// <summary>A video.</summary>
    Video = 2,
}

/// <summary>The media type of a piece of content.</summary>
public enum SceContentSearchMimeType
{
    Unknown = 0,
    ImageJpeg = 1,
    VideoMp4 = 2,
    ImagePng = 3,
    ImageGif = 4,
    VideoWebm = 5,
    AudioMpeg = 0x1000,
    AudioMp4 = 0x1001,
    AudioMp3 = 0x1002,
    AudioAac = 0x1003,
    AudioWav = 0x1004,
    AudioVorbis = 0x1005,
    AudioOpus = 0x1006,
}

/// <summary>A column a search filters or reads.</summary>
public enum SceContentSearchContentColumnType
{
    ContentId = 1,
    MimeType = 2,
    ContentType = 3,
    Title = 4,
    CreatedTime = 5,
    Size = 6,
    UserAccount = 7,
    Duration = 9,
    ContentPath = 10,
    GeneratorType = 11,
    Status = 12,
    UploadStatus = 14,
}

/// <summary>A column a search orders results by.</summary>
public enum SceContentSearchContentSortColumnType
{
    ContentId = 1,
    Title = 4,
    CreatedTime = 5,
    Size = 6,
    Duration = 9,
    ContentPath = 10,
}

/// <summary>How a column value is compared in a filter.</summary>
public enum SceContentSearchColumnValueOperator
{
    Equal = 0,
    NotEqual = 1,
    Greater = 2,
    GreaterEqual = 3,
    Less = 4,
    LessEqual = 5,
}

/// <summary>How a filter joins to the next one.</summary>
public enum SceContentSearchLogicalConnector
{
    And = 0,
    Or = 1,
    None = 2,
}

/// <summary>The order results are sorted in.</summary>
public enum SceContentSearchSortType
{
    Ascendant = 0,
    Descendant = 1,
}

/// <summary>Whether a piece of content is usable.</summary>
public enum SceContentSearchContentStatus
{
    Broken = 0,
    Available = 1,
}

/// <summary>What produced a piece of content.</summary>
public enum SceContentSearchContentGeneratorType
{
    Unknown = 0,
    Share = 1,
    Export = 2,
    SystemEdit = 3,
    ShareLibrary = 4,
}

/// <summary>Whether a piece of content has been uploaded.</summary>
public enum SceContentSearchUploadStatus
{
    NotUploaded = 0,
    Uploaded = 1,
}

/// <summary>The parameters the content-search service is initialized with.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceContentSearchInitParam
{
    /// <summary>The size of the heap the service works with, in bytes.</summary>
    public nuint MemorySize;
}

/// <summary>A value a filter compares a column against.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceContentSearchColumnValue
{
    /// <summary>The size of the value in <see cref="Value"/>, in bytes.</summary>
    public uint Size;

    /// <summary>The value itself, read per the column's type (an integer, a tick, or a text pointer).</summary>
    public long Value;
}

/// <summary>One filter of a search: a column, an operator, a value and how it joins the next.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceContentSearchContentColumn
{
    public SceContentSearchContentColumnType ColumnType;
    public SceContentSearchColumnValueOperator ColumnOperator;
    public SceContentSearchColumnValue* Value;
    public SceContentSearchLogicalConnector LogicalConnector;
}

/// <summary>A set of filters applied together.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceContentSearchContentColumnSet
{
    public SceContentSearchContentColumn* Columns;
    public int ColumnsLength;
    public SceContentSearchLogicalConnector LogicalConnector;
}

/// <summary>An order-by clause: the column to sort on and the direction.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceContentSearchContentOrderByCondition
{
    public SceContentSearchContentSortColumnType ColumnType;
    public SceContentSearchSortType SortType;
}

/// <summary>
/// One result row. The fixed-size fields hold null-terminated UTF-8 text; the reserved fields are the
/// header's alignment padding and are not read.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 2400)]
public unsafe struct SceContentSearchContentInfo
{
    /// <summary>Path length limit, including the terminator.</summary>
    public const int PathLength = 1024;

    /// <summary>Title length limit, including the terminator.</summary>
    public const int TitleLength = 256;

    public long ContentId;                          // 0x000
    public int Duration;                            // 0x008
    public SceContentSearchMimeType MimeType;       // 0x00C
    public SceContentSearchContentType ContentType; // 0x010
    public SceContentSearchContentGeneratorType GeneratorId; // 0x014
    public fixed byte ContentPath[PathLength + 1];  // 0x018
    private fixed byte _reserved1[11];              // 0x419
    public fixed byte Title[TitleLength + 1];       // 0x424
    private fixed byte _reserved2[6];               // 0x525
    public fixed byte IconPath[PathLength + 1];     // 0x52B
    private fixed byte _reserved3[8];               // 0x92C
    public SceContentSearchUploadStatus UploadStatus; // 0x934
    public ulong CreatedTime;                       // 0x938 (a tick)
    public long Size;                               // 0x940
    public SceContentSearchContentStatus Status;    // 0x948
    public fixed int Accounts[4];                   // 0x94C
    private int _reserved4;                          // 0x95C -> 0x960 (2400)
}

/// <summary>The type of a metadata field value.</summary>
public enum SceContentSearchMetadataType
{
    /// <summary>A 64-bit integer.</summary>
    Int = 0,

    /// <summary>A double-precision float.</summary>
    Float = 1,

    /// <summary>Text (a caller-provided buffer receives it).</summary>
    Text = 2,

    /// <summary>A real-time-clock tick.</summary>
    Tick = 4,
}

/// <summary>A metadata field value, read per its <see cref="Type"/>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceContentSearchMetadataValue
{
    /// <summary>The size of the value in bytes; for text, the size to allocate for <see cref="Value"/>.</summary>
    public int Size;

    /// <summary>The type of the value.</summary>
    public SceContentSearchMetadataType Type;

    /// <summary>An integer or tick value, a reinterpreted float, or a pointer to a text buffer.</summary>
    public long Value;
}

/// <summary>
/// Content-search bindings: read the library of captures and imported media on the console. The
/// service is permission-gated: a process that does not hold the content-search permission gets
/// <see cref="PermissionRequired"/> from a query.
/// </summary>
public static unsafe partial class ContentSearch
{
    private const string Lib = "libSceContentSearch";

    /// <summary>Metadata field name for the title.</summary>
    public const string FieldTitle = "title";

    /// <summary>Metadata field name for the media type.</summary>
    public const string FieldMimeType = "mime_type";

    /// <summary>Metadata field name for the created time.</summary>
    public const string FieldCreatedTime = "created_time";

    /// <summary>Metadata field name for the size in bytes.</summary>
    public const string FieldSize = "size";

    /// <summary>Metadata field name for the pixel width.</summary>
    public const string FieldWidth = "width";

    /// <summary>Metadata field name for the pixel height.</summary>
    public const string FieldHeight = "height";

    /// <summary>Metadata field name for the duration.</summary>
    public const string FieldDuration = "duration";

    /// <summary>The process does not hold the permission a query needs.</summary>
    public const int PermissionRequired = unchecked((int)0x809D1013);

    /// <summary>The service was not initialized.</summary>
    public const int NotInitialized = unchecked((int)0x809D1001);

    /// <summary>A limit above the maximum the service accepts.</summary>
    public const int LimitTooBig = unchecked((int)0x809D100A);

    /// <summary>The largest number of rows a single search returns.</summary>
    public const int MaxLimit = 92;

    /// <summary>Starts the content-search service.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchInit(SceContentSearchInitParam* initParam);

    /// <summary>Stops the content-search service.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchTerm();

    /// <summary>Reads the id the content database last changed at, to detect updates.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchGetContentLastUpdateId(long* lastUpdateId);

    /// <summary>Counts the rows a filter set matches.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchGetNumOfContent(
        SceContentSearchContentColumnSet* columnSet, uint columnSetLength, long* numOfContent, long* lastUpdateId);

    /// <summary>Sums the size of the rows a filter set matches, in bytes.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchGetTotalContentSize(
        SceContentSearchContentColumnSet* columnSet, uint columnSetLength, long* size, long* lastUpdateId);

    /// <summary>Reads a page of rows a filter set matches, sorted by the order-by clauses.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchSearchContent(
        SceContentSearchContentColumnSet* columnSet, uint columnSetLength,
        SceContentSearchContentOrderByCondition* orderByConditions, uint orderByConditionsLength,
        uint offset, uint limit, long* numOfContent, SceContentSearchContentInfo* infos, long* lastUpdateId);

    /// <summary>Opens a metadata handle for the content file at <paramref name="filePath"/> (UTF-8).</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchOpenMetadata(byte* filePath, int* metadataId);

    /// <summary>Opens a metadata handle for the content with <paramref name="contentId"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchOpenMetadataByContentId(long contentId, int* metadataId);

    /// <summary>Closes a metadata handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchCloseMetadata(int metadataId);

    /// <summary>Reads the type and size of the <paramref name="field"/> (UTF-8) of a metadata handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchGetMetadataFieldInfo(
        int metadataId, byte* field, SceContentSearchMetadataType* metadataType, int* size);

    /// <summary>Reads the value of the <paramref name="field"/> (UTF-8) of a metadata handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentSearchGetMetadataValue(
        int metadataId, byte* field, SceContentSearchMetadataValue* value);
}
