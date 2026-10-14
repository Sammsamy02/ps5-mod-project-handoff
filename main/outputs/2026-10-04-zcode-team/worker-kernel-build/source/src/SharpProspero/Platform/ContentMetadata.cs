// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Content;
using System;
using System.Runtime.InteropServices;
using System.Text;
using Native = SharpProspero.Interop.Content.ContentSearch;

namespace SharpProspero.Platform;

/// <summary>
/// Reads the metadata fields of one piece of content, opened from <see cref="ContentLibrary"/>. Read
/// the fields you need, then dispose it. Field names are the constants on
/// <see cref="Interop.Content.ContentSearch"/>, for example <see cref="ContentSearch.FieldTitle"/>.
/// </summary>
public sealed unsafe class ContentMetadata : IDisposable
{
    private readonly int _id;
    private bool _disposed;

    internal ContentMetadata(int id) => _id = id;

    /// <summary>Reads the type and byte size of <paramref name="field"/>.</summary>
    /// <exception cref="ProsperoException">The field could not be read.</exception>
    public (SceContentSearchMetadataType Type, int Size) GetFieldInfo(string field)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(field);

        int count = Encoding.UTF8.GetByteCount(field);
        byte* f = stackalloc byte[count + 1];
        Encoding.UTF8.GetBytes(field, new Span<byte>(f, count));
        f[count] = 0;

        SceContentSearchMetadataType type;
        int size;
        SceResult.ThrowIfFailed(
            Native.sceContentSearchGetMetadataFieldInfo(_id, f, &type, &size),
            nameof(Native.sceContentSearchGetMetadataFieldInfo));
        return (type, size);
    }

    /// <summary>Reads <paramref name="field"/> as a 64-bit integer.</summary>
    /// <exception cref="ProsperoException">The field could not be read.</exception>
    public long GetInt(string field) => ReadValue(field, null).Value;

    /// <summary>Reads <paramref name="field"/> as a real-time-clock tick.</summary>
    /// <exception cref="ProsperoException">The field could not be read.</exception>
    public ulong GetTick(string field) => (ulong)ReadValue(field, null).Value;

    /// <summary>Reads <paramref name="field"/> as a double.</summary>
    /// <exception cref="ProsperoException">The field could not be read.</exception>
    public double GetFloat(string field) => BitConverter.Int64BitsToDouble(ReadValue(field, null).Value);

    /// <summary>Reads <paramref name="field"/> as text, or an empty string when it is not text.</summary>
    /// <exception cref="ProsperoException">The field could not be read.</exception>
    public string GetText(string field)
    {
        (SceContentSearchMetadataType type, int size) = GetFieldInfo(field);
        if (type != SceContentSearchMetadataType.Text || size <= 0)
            return string.Empty;

        // The service writes the text into a buffer the caller supplies through the value's pointer.
        byte* buffer = (byte*)NativeMemory.Alloc((nuint)size);
        try
        {
            _ = ReadValue(field, buffer, size);
            return ReadUtf8(buffer, size);
        }
        finally
        {
            NativeMemory.Free(buffer);
        }
    }

    /// <summary>Closes the metadata handle.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        Native.sceContentSearchCloseMetadata(_id);
    }

    private SceContentSearchMetadataValue ReadValue(string field, byte* textBuffer, int textCapacity = 0)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(field);

        int count = Encoding.UTF8.GetByteCount(field);
        byte* f = stackalloc byte[count + 1];
        Encoding.UTF8.GetBytes(field, new Span<byte>(f, count));
        f[count] = 0;

        var value = default(SceContentSearchMetadataValue);
        if (textBuffer != null)
        {
            // The service is told where to put the text and how much room is there. Handing it the
            // pointer alone leaves the capacity at nought, which is not a buffer it can write into.
            value.Value = (long)(nint)textBuffer;
            value.Size = textCapacity;
        }
        SceResult.ThrowIfFailed(
            Native.sceContentSearchGetMetadataValue(_id, f, &value),
            nameof(Native.sceContentSearchGetMetadataValue));
        return value;
    }

    private static string ReadUtf8(byte* start, int maxLength)
    {
        int length = 0;
        while (length < maxLength && start[length] != 0)
            length++;
        return length == 0 ? string.Empty : Encoding.UTF8.GetString(start, length);
    }
}
