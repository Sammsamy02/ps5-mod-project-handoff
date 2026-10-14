---
title: Compression and archives
parent: Data and utilities
grand_parent: Application Modules
nav_order: 7
---

# Compression and archives

`SharpProspero.Compression` compresses and decompresses the formats that assets, downloads and network
payloads arrive in, and reads and writes ZIP archives. There is no service to open and no size cap, so the
same call works on the console, in a command-line tool, and in a test. Point it at bytes you already hold
in memory and it hands back the result.

## Decompress DEFLATE, zlib and gzip

`Inflate` reads the DEFLATE format and its two common wrappers. DEFLATE is the compression behind gzip, the
zlib format, ZIP entries, and PNG image data, so one decompressor covers a lot of ground.

```csharp
using SharpProspero.Compression;

byte[] plain = Inflate.Gzip(FileSystem.ReadAllBytes("/app0/assets/level.json.gz"));
byte[] fromZlib = Inflate.Zlib(zlibBytes);   // a 2-byte header and an Adler-32 trailer
byte[] fromRaw = Inflate.Raw(deflateBytes);  // a bare stream, as found inside a ZIP entry
```

`Raw` reads a bare DEFLATE stream; pass the uncompressed size as `sizeHint` when you know it to size the
output buffer once. `Zlib` reads an RFC 1950 stream and checks its Adler-32 trailer; `Gzip` reads an
RFC 1952 member and checks its CRC-32 and length trailer. A malformed or truncated stream, or a failed
checksum, throws `CompressionException`. `Adler32` is exposed for computing the checksum yourself.

{: .note }
> This runs in managed code and works anywhere, with no size limit. The service-backed
> `Storage.ZlibDecompressor` is a separate path that offloads to the system and caps each call at 64 KiB;
> reach for `Inflate` unless you specifically want the service.

## Compress to DEFLATE, zlib or gzip

`Deflate` is the mirror of `Inflate`: it shrinks a save file, a network payload, or a bundle of assets, and
what it writes any DEFLATE reader accepts. It matches repeats with a hash-chain search and emits fixed
Huffman codes, favouring a small, predictable footprint over the last few percent of ratio.

```csharp
using SharpProspero.Compression;

byte[] gz = Deflate.Gzip(json);              // a gzip member, ready to write to disk or send
byte[] payload = Deflate.Zlib(saveBytes);    // a zlib stream with an Adler-32 trailer
byte[] raw = Deflate.Raw(bytes);             // a bare stream, e.g. for a ZIP entry
```

`Raw`, `Zlib` and `Gzip` each round-trip through the matching `Inflate` call and through any standard
decompressor.

## Read a ZIP archive

`ZipArchive` opens a ZIP held in memory - a bundle of assets, a downloaded pack, a save export. It parses
the directory up front so you can list what is inside, then decompresses one member at a time, checking
each against its recorded CRC-32.

```csharp
using SharpProspero.Compression;

ZipArchive zip = ZipArchive.Open(FileSystem.ReadAllBytes("/data/pack.zip"));
foreach (ZipEntry entry in zip.Entries)
    if (!entry.IsDirectory)
        FileSystem.WriteAllBytes("/data/unpacked/" + entry.Name, zip.Extract(entry));
```

`Entries` lists every member with its name, sizes, method, CRC-32 and modification time. `TryGetEntry` finds
one by name, `Extract` decompresses a member (stored or DEFLATE) and verifies it, and a directory entry
extracts to an empty array. Members compressed with other methods, and ZIP64 archives, are reported with a
`CompressionException` rather than a wrong result. `Extract(string name)` does the same for a member looked
up by name, raising `KeyNotFoundException` when there is none. A name is read as UTF-8 only when the entry
says it is; otherwise it is read in the format's older single-byte encoding, so a name an outside tool
wrote comes back with the characters it wrote.

{: .tip }
> A ZIP is the easy way to ship many assets as one file and pull them out at run time. Read the archive
> once into memory, keep the `ZipArchive`, and `Extract` each asset as the application needs it.

## Write a ZIP archive

`ZipBuilder` gathers files into one archive - a save export, a set of screenshots, a pack to send over the
network. Add each member, then take the finished bytes; any ZIP reader, including `ZipArchive`, reads them
back.

```csharp
using SharpProspero.Compression;

byte[] zip = new ZipBuilder()
    .AddText("manifest.json", manifest)   // compressed with DEFLATE
    .Add("save/slot0.bin", saveBytes)     // compressed when that is smaller, stored otherwise
    .Add("save/raw.bin", rawBytes, compress: false)
    .AddDirectory("save")
    .ToArray();

FileSystem.WriteAllBytes("/data/export.zip", zip);
```

`Add` and `AddText` compress by default and fall back to storing when a member does not shrink; pass
`compress: false` to store it as-is. `AddDirectory` adds an explicit folder entry. Entries carry a fixed,
reproducible timestamp, so the same inputs produce the same archive.

`ZipBuilder` writes names as UTF-8 and turns `\` into `/`. It marks an entry whose name holds a character
above ASCII as UTF-8, so any reader decodes it the same way; an ASCII name goes unmarked and reads back
unchanged in readers that predate the flag.
