// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Text;

namespace SharpProspero.Media;

/// <summary>The descriptive tags of an audio track: what a music list shows for it.</summary>
/// <param name="Title">The track title.</param>
/// <param name="Artist">The performing artist.</param>
/// <param name="Album">The album name.</param>
/// <param name="TrackNumber">The track number (as written, which may be "3/12").</param>
/// <param name="Year">The release year.</param>
/// <param name="Genre">The genre.</param>
public readonly record struct MediaTags(string Title, string Artist, string Album, string TrackNumber, string Year, string Genre)
{
    /// <summary>An all-empty set of tags.</summary>
    public static MediaTags Empty => new("", "", "", "", "", "");

    /// <summary>Whether every field is empty.</summary>
    public bool IsEmpty => Title.Length == 0 && Artist.Length == 0 && Album.Length == 0
        && TrackNumber.Length == 0 && Year.Length == 0 && Genre.Length == 0;
}

/// <summary>
/// Reads the tags a media file carries, with no system module. It understands the tag block at the front
/// of a file (title, artist, album, track, year, genre) and falls back to the short fixed tag some files
/// keep at the end, so a music player can list a folder of tracks by name and artist without decoding the
/// audio.
/// </summary>
public static class MediaMetadata
{
    /// <summary>Reads the tags from a media file's bytes, or <see cref="MediaTags.Empty"/> when it has none.</summary>
    public static MediaTags Read(ReadOnlySpan<byte> file)
    {
        MediaTags tags = ReadFrontTag(file);
        if (!tags.IsEmpty)
            return tags;
        return ReadTrailingTag(file);
    }

    // The tag block at the start of the file: "ID3", a version, flags, a synchsafe size, then frames.
    private static MediaTags ReadFrontTag(ReadOnlySpan<byte> file)
    {
        if (file.Length < 10 || file[0] != (byte)'I' || file[1] != (byte)'D' || file[2] != (byte)'3')
            return MediaTags.Empty;
        int version = file[3];
        if (version is not (3 or 4))
            return MediaTags.Empty; // 2.3 and 2.4 use the ten-byte frame header
        byte flags = file[5];
        int tagSize = ReadSynchsafe(file[6..10]);
        int end = Math.Min(10 + tagSize, file.Length);

        int pos = 10;
        if ((flags & 0x40) != 0 && pos + 4 <= end) // an extended header precedes the frames
        {
            // In 2.4 the size counts the four size bytes; in 2.3 it does not. Compute in a wide type and
            // validate, so a crafted size cannot drive the read offset negative or past the end.
            long extHeader = version == 4
                ? ReadSynchsafe(file.Slice(pos, 4))
                : 4L + BinaryPrimitives.ReadUInt32BigEndian(file[pos..]);
            if (extHeader < 4 || pos + extHeader > end)
                return MediaTags.Empty;
            pos += (int)extHeader;
        }

        string title = "", artist = "", album = "", track = "", year = "", genre = "";
        while (pos + 10 <= end)
        {
            if (file[pos] == 0) // padding
                break;
            string id = Encoding.ASCII.GetString(file.Slice(pos, 4));
            // Wide type, and compare against the bytes remaining, so a crafted size cannot overflow the sum.
            long size = version == 4 ? ReadSynchsafe(file.Slice(pos + 4, 4)) : BinaryPrimitives.ReadUInt32BigEndian(file[(pos + 4)..]);
            if (size <= 0 || size > end - (pos + 10))
                break;
            ReadOnlySpan<byte> body = file.Slice(pos + 10, (int)size);
            switch (id)
            {
                case "TIT2": title = DecodeText(body); break;
                case "TPE1": artist = DecodeText(body); break;
                case "TALB": album = DecodeText(body); break;
                case "TRCK": track = DecodeText(body); break;
                case "TYER" or "TDRC": year = DecodeText(body); break;
                case "TCON": genre = DecodeText(body); break;
            }
            pos += 10 + (int)size;
        }
        return new MediaTags(title, artist, album, track, year, genre);
    }

    // The 128-byte fixed tag at the end: "TAG", then fixed-width Latin-1 fields.
    private static MediaTags ReadTrailingTag(ReadOnlySpan<byte> file)
    {
        if (file.Length < 128)
            return MediaTags.Empty;
        ReadOnlySpan<byte> tag = file[^128..];
        if (tag[0] != (byte)'T' || tag[1] != (byte)'A' || tag[2] != (byte)'G')
            return MediaTags.Empty;
        return new MediaTags(Latin1(tag.Slice(3, 30)), Latin1(tag.Slice(33, 30)), Latin1(tag.Slice(63, 30)), "", Latin1(tag.Slice(93, 4)), "");
    }

    private static string DecodeText(ReadOnlySpan<byte> body)
    {
        if (body.Length < 1)
            return "";
        byte encoding = body[0];
        ReadOnlySpan<byte> text = body[1..];
        string decoded = encoding switch
        {
            1 => DecodeUtf16WithBom(text),
            2 => Encoding.BigEndianUnicode.GetString(text),
            3 => Encoding.UTF8.GetString(text),
            _ => Latin1(text),
        };
        return decoded.TrimEnd('\0').Trim();
    }

    private static string DecodeUtf16WithBom(ReadOnlySpan<byte> text)
    {
        if (text.Length >= 2 && text[0] == 0xFF && text[1] == 0xFE)
            return Encoding.Unicode.GetString(text[2..]);
        if (text.Length >= 2 && text[0] == 0xFE && text[1] == 0xFF)
            return Encoding.BigEndianUnicode.GetString(text[2..]);
        return Encoding.Unicode.GetString(text);
    }

    private static string Latin1(ReadOnlySpan<byte> bytes)
    {
        Span<char> chars = bytes.Length <= 256 ? stackalloc char[bytes.Length] : new char[bytes.Length];
        for (int i = 0; i < bytes.Length; i++)
            chars[i] = (char)bytes[i];
        return new string(chars).TrimEnd('\0', ' ');
    }

    // A synchsafe integer: four bytes, seven bits each, high bit clear.
    private static int ReadSynchsafe(ReadOnlySpan<byte> b)
        => ((b[0] & 0x7F) << 21) | ((b[1] & 0x7F) << 14) | ((b[2] & 0x7F) << 7) | (b[3] & 0x7F);
}
