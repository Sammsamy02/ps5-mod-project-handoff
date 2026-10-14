// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Content;
using SharpProspero.Interop.Sysmodule;
using SharpProspero.Modules;
using System;
using System.Collections.Generic;
using System.Text;
using Native = SharpProspero.Interop.Content.ContentSearch;

namespace SharpProspero.Platform;

/// <summary>One row from the content library.</summary>
/// <param name="ContentId">The content's id.</param>
/// <param name="Type">Whether it is a photo or a video.</param>
/// <param name="MimeType">The media type.</param>
/// <param name="Title">The title shown for it.</param>
/// <param name="Path">The path to the content file.</param>
/// <param name="IconPath">The path to its thumbnail, when it has one.</param>
/// <param name="Size">The file size in bytes.</param>
/// <param name="Available">Whether the content is usable rather than broken.</param>
public readonly record struct ContentItem(
    long ContentId,
    SceContentSearchContentType Type,
    SceContentSearchMimeType MimeType,
    string Title,
    string Path,
    string IconPath,
    long Size,
    bool Available);

/// <summary>
/// Reads the library of captures and imported media on the console: list photos and videos, count
/// them, and total their size. Open it, query it, dispose it.
/// </summary>
/// <remarks>
/// The service is permission-gated. A process that does not hold the content-search permission gets a
/// <see cref="ProsperoException"/> whose code is <see cref="ContentSearch.PermissionRequired"/> from a
/// query; whether a process holds it depends on the console.
/// </remarks>
/// <example>
/// <code>
/// using var library = ContentLibrary.Open();
/// foreach (ContentItem photo in library.List(SceContentSearchContentType.Photo))
///     Console.WriteLine(photo.Title);
/// </code>
/// </example>
public sealed unsafe class ContentLibrary : IDisposable
{
    /// <summary>The default working heap the service is started with, in bytes.</summary>
    public const int DefaultMemorySize = 3 * 1024 * 1024;

    // The loadable module the service lives in, owned from the moment it is loaded so that every way
    // out of the open sequence gives it back rather than leaving it mapped for the life of the process.
    private readonly SystemModule _module;
    private bool _disposed;
    private bool _initialized;

    private ContentLibrary(SystemModule module) => _module = module;

    /// <summary>
    /// Loads and starts the content-search service. <paramref name="memorySize"/> is the working heap;
    /// the default suits ordinary listing.
    /// </summary>
    /// <exception cref="ProsperoException">The service could not be loaded or started.</exception>
    public static ContentLibrary Open(int memorySize = DefaultMemorySize)
    {
        var library = new ContentLibrary(SystemModule.Load(SystemModuleId.ContentSearch));
        try
        {
            var param = new SceContentSearchInitParam { MemorySize = (nuint)memorySize };
            SceResult.ThrowIfFailed(Native.sceContentSearchInit(&param), nameof(Native.sceContentSearchInit));
            library._initialized = true;
            return library;
        }
        catch
        {
            library.Dispose();
            throw;
        }
    }

    /// <summary>Counts the items of <paramref name="type"/> in the library.</summary>
    /// <exception cref="ProsperoException">The query was rejected.</exception>
    public long Count(SceContentSearchContentType type)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var value = new SceContentSearchColumnValue { Size = sizeof(long), Value = (long)type };
        var column = new SceContentSearchContentColumn
        {
            ColumnType = SceContentSearchContentColumnType.ContentType,
            ColumnOperator = SceContentSearchColumnValueOperator.Equal,
            Value = &value,
            LogicalConnector = SceContentSearchLogicalConnector.None,
        };
        var columnSet = new SceContentSearchContentColumnSet
        {
            Columns = &column,
            ColumnsLength = 1,
            LogicalConnector = SceContentSearchLogicalConnector.None,
        };

        long count = 0, lastUpdateId = 0;
        SceResult.ThrowIfFailed(
            Native.sceContentSearchGetNumOfContent(&columnSet, 1, &count, &lastUpdateId),
            nameof(Native.sceContentSearchGetNumOfContent));
        return count;
    }

    /// <summary>
    /// Reads a page of items of <paramref name="type"/>, sorted by title. <paramref name="limit"/> is
    /// clamped to the service maximum of <see cref="ContentSearch.MaxLimit"/> rows per call; page
    /// through larger sets by raising <paramref name="offset"/>.
    /// </summary>
    /// <exception cref="ProsperoException">The query was rejected.</exception>
    public IReadOnlyList<ContentItem> List(
        SceContentSearchContentType type, int offset = 0, int limit = ContentSearch.MaxLimit)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentOutOfRangeException.ThrowIfNegative(offset);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(limit);
        if (limit > ContentSearch.MaxLimit)
            limit = ContentSearch.MaxLimit;

        var value = new SceContentSearchColumnValue { Size = sizeof(long), Value = (long)type };
        var column = new SceContentSearchContentColumn
        {
            ColumnType = SceContentSearchContentColumnType.ContentType,
            ColumnOperator = SceContentSearchColumnValueOperator.Equal,
            Value = &value,
            LogicalConnector = SceContentSearchLogicalConnector.None,
        };
        var columnSet = new SceContentSearchContentColumnSet
        {
            Columns = &column,
            ColumnsLength = 1,
            LogicalConnector = SceContentSearchLogicalConnector.None,
        };
        var orderBy = new SceContentSearchContentOrderByCondition
        {
            ColumnType = SceContentSearchContentSortColumnType.Title,
            SortType = SceContentSearchSortType.Ascendant,
        };

        var infos = new SceContentSearchContentInfo[limit];
        long numOfContent = 0, lastUpdateId = 0;
        int rc;
        fixed (SceContentSearchContentInfo* rows = infos)
            rc = Native.sceContentSearchSearchContent(
                &columnSet, 1, &orderBy, 1, (uint)offset, (uint)limit, &numOfContent, rows, &lastUpdateId);
        SceResult.ThrowIfFailed(rc, nameof(Native.sceContentSearchSearchContent));

        int rows2 = (int)Math.Min(numOfContent, limit);
        var items = new List<ContentItem>(rows2);
        for (int i = 0; i < rows2; i++)
        {
            ref SceContentSearchContentInfo row = ref infos[i];
            fixed (SceContentSearchContentInfo* p = &row)
            {
                items.Add(new ContentItem(
                    row.ContentId,
                    row.ContentType,
                    row.MimeType,
                    ReadUtf8(p->Title, SceContentSearchContentInfo.TitleLength + 1),
                    ReadUtf8(p->ContentPath, SceContentSearchContentInfo.PathLength + 1),
                    ReadUtf8(p->IconPath, SceContentSearchContentInfo.PathLength + 1),
                    row.Size,
                    row.Status == SceContentSearchContentStatus.Available));
            }
        }
        return items;
    }

    /// <summary>
    /// Opens the metadata of the content file at <paramref name="filePath"/> (for example the
    /// <see cref="ContentItem.Path"/> of a listed item), so its fields can be read.
    /// </summary>
    /// <exception cref="ProsperoException">The metadata could not be opened.</exception>
    public ContentMetadata OpenMetadata(string filePath)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(filePath);

        int count = Encoding.UTF8.GetByteCount(filePath);
        byte* path = stackalloc byte[count + 1];
        Encoding.UTF8.GetBytes(filePath, new Span<byte>(path, count));
        path[count] = 0;

        int metadataId;
        SceResult.ThrowIfFailed(
            Native.sceContentSearchOpenMetadata(path, &metadataId), nameof(Native.sceContentSearchOpenMetadata));
        return new ContentMetadata(metadataId);
    }

    /// <summary>Opens the metadata of the content with <paramref name="contentId"/>.</summary>
    /// <exception cref="ProsperoException">The metadata could not be opened.</exception>
    public ContentMetadata OpenMetadata(long contentId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        int metadataId;
        SceResult.ThrowIfFailed(
            Native.sceContentSearchOpenMetadataByContentId(contentId, &metadataId),
            nameof(Native.sceContentSearchOpenMetadataByContentId));
        return new ContentMetadata(metadataId);
    }

    /// <summary>Stops the service and unloads the module.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        if (_initialized)
            Native.sceContentSearchTerm();
        _module.Dispose();
    }

    private static string ReadUtf8(byte* start, int maxLength)
    {
        int length = 0;
        while (length < maxLength && start[length] != 0)
            length++;
        return length == 0 ? string.Empty : Encoding.UTF8.GetString(start, length);
    }
}
