---
title: Buffers and encodings
parent: Data and utilities
grand_parent: Application Modules
nav_order: 3
---

# Buffers and encodings

Everything in `SharpProspero.Buffers` works on plain byte buffers with no stream and, where it matters, no
allocation: read and write binary layouts, keep a rolling window of recent values, move bytes between a
producer and a consumer, and turn bytes into text and back.

## Reading and writing bytes

`SpanReader` and `ByteWriter` read and write numbers, text and raw bytes in a byte buffer, choosing the byte
order per value — for a file header, a save file, or a message on a socket. `SpanReader` keeps a cursor into
a buffer and throws rather than reading past the end; `ByteWriter` grows its buffer as it goes.

```csharp
using SharpProspero.Buffers;

var writer = new ByteWriter();
writer.WriteUInt32BigEndian(0x53415645);   // a "SAVE" tag
writer.WriteInt32LittleEndian(level);
writer.WriteUtf8(playerName);
byte[] bytes = writer.ToArray();

var reader = new SpanReader(bytes);
uint tag = reader.ReadUInt32BigEndian();
int savedLevel = reader.ReadInt32LittleEndian();
```

Both cover the 8-, 16-, 32- and 64-bit integers, the 32- and 64-bit floats (little- and big-endian), raw
byte spans and UTF-8 text. `SpanReader` exposes `Position`, `Length`, `Remaining` and `End`, and
`Skip(count)` steps over reserved fields without reading them; a read past the end throws
`EndOfStreamException`. Its
`ReadBytes(count)` returns a view into the buffer without copying, and `ReadUtf8(byteCount)` decodes text.
`ByteWriter` tracks `Count`, hands back `WrittenSpan` or a fresh `ToArray()`, and `Clear()` resets it while
keeping the buffer it has grown to, so a per-frame message writer allocates once and refills.

{: .note }
> `SpanReader` is a `ref struct`: it holds a span, so it lives on the stack, reads in place with no
> allocation, and cannot be stored in a field or captured by a lambda. Read from it within the method that
> owns the buffer. `WriteUtf8` returns the number of bytes it wrote, which is handy when a length prefix has
> to be back-patched.

## Ring buffers

`RingBuffer<T>` is a fixed-capacity queue over a single array. When it is full, adding another item
overwrites the oldest — exactly what a rolling history wants: the last N frame times, recent log lines, an
input trail. The oldest item is at index 0, and it enumerates oldest to newest.

```csharp
using SharpProspero.Buffers;

var recent = new RingBuffer<float>(120);          // last 120 frame times
recent.Add((float)context.DeltaSeconds);
float newest = recent[recent.Count - 1];
```

Alongside the indexer and enumeration it offers `Add`, `Peek`, `Remove`, `TryRemove(out T)` and `Clear`,
plus `Capacity`, `Count`, `IsEmpty` and `IsFull` — check `IsFull` before an `Add` when you need to know the
oldest item is about to fall off the end. It implements `IReadOnlyCollection<T>`, so a `foreach` or a LINQ
query walks the current items oldest to newest.

`ByteRing` is the byte equivalent for buffering a stream — audio samples between a producer and the audio
output, bytes off a socket before a whole message has arrived, a decoder's output waiting to be consumed.
Unlike `RingBuffer<T>` it never overwrites: `Write` stores only what fits and returns how much it took, and
`Read` copies the oldest bytes into your span and removes them. That return value is the back-pressure
signal, so a producer and consumer running at different rates stay in step. Both wrap around the end of the
buffer without an extra copy.

```csharp
using SharpProspero.Buffers;

var ring = new ByteRing(4096);
int stored = ring.Write(incoming);                // less than incoming.Length once the ring fills

Span<byte> chunk = stackalloc byte[512];
int taken = ring.Read(chunk);                     // oldest bytes first; less than 512 once it runs dry
```

`FreeSpace` says how much a `Write` can still accept, `Count` how many bytes are waiting to be read — the
check a consumer makes before it decides a whole message has arrived — with `Capacity` and `IsEmpty`
alongside them. `Skip(count)` drops the oldest bytes without copying
them out, and `Clear` empties the ring.

## Bit fields

`BitWriter` and `BitReader` pack and unpack values that are not a whole number of bytes wide - the flag
bits, small counters and variable-length fields custom binary formats and image codecs are built from.
Both work most-significant bit first, so what one writes the other reads straight back.

```csharp
using SharpProspero.Buffers;

var writer = new BitWriter();
writer.WriteBits(0b101, 3);      // a 3-bit field
writer.WriteBit(true);           // a single flag
writer.WriteBits(sampleRate, 20);
byte[] packed = writer.ToArray(); // a part-filled final byte is padded with zero bits

var reader = new BitReader(packed);
uint mode = reader.ReadBits(3);
bool flag = reader.ReadBit();
uint rate = reader.ReadBits(20);
```

`WriteBits`/`ReadBits` handle 0 to 32 bits at a time, `WriteBit`/`ReadBit` do one, and `AlignToByte` moves
to the next whole-byte boundary (padding on write, skipping on read). Both expose `BitLength` — the total
on a reader, the count written so far on a writer, which is how you learn the exact bit count before
`ToArray` pads the last byte. `BitReader` also exposes `BitPosition`,
`BitsRemaining` and `TryReadBits`, which returns false at the end of the buffer instead of throwing.

## Hex, Base32 and Base64

`BaseN` turns bytes into text and back — hexadecimal, Base32 (RFC 4648), and Base64 (standard or the
URL-safe alphabet, with or without padding) — for an HTTP header, a token or a `data:` URL, or a stored
blob. Decoding ignores whitespace, and the base codecs also ignore padding, so a round trip survives text
that has been wrapped or trimmed.

```csharp
using SharpProspero.Buffers;

string token = BaseN.ToBase64(bytes, urlSafe: true, padding: false);
byte[] back = BaseN.FromBase64(token);
string hex = BaseN.ToHex(digest);                 // lower-case; upperCase: true for upper
```

| Encoding | To text | From text |
| --- | --- | --- |
| Hexadecimal | `ToHex(data, upperCase)` | `FromHex(text)` |
| Base32 | `ToBase32(data, padding)` | `FromBase32(text)` |
| Base64 | `ToBase64(data, urlSafe, padding)` | `FromBase64(text)` |

`ToHex` pairs well with the digests from [Hashing and checksums](hashing.md) when you need a hex fingerprint
to log or compare. For reading and writing whole files rather than in-memory buffers, see
[Files and storage](storage.md).
