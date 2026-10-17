---
title: Text utilities
parent: Data and utilities
grand_parent: Application Modules
nav_order: 4
---

# Text utilities

Two things applications do to text that the runtime does not do for them: turn raw values into strings a
person can read, and match a typed query against a list. Both live in `SharpProspero.Text` and are plain
string work — they run anywhere and allocate only the result.

## Human-readable formatting

`TextFormat` is a static class that produces the strings a UI shows: a file size, a playback duration, a
filename-friendly sort, aligned columns, and a byte dump.

```csharp
using SharpProspero.Text;

string size = TextFormat.ByteSize(1_572_864);     // "1.5 MiB"  (binary: false for KB/MB steps)
string time = TextFormat.Duration(track.Seconds);  // "3:45", or "1:02:03" once past an hour
string dump = TextFormat.HexDump(header);           // offset, hex, and printable-ASCII columns
```

`ByteSize` steps in binary units (1024, KiB/MiB/…) by default; pass `binary: false` for decimal units
(1000, KB/MB/…). It handles negative counts and rounds to one decimal, promoting a unit when rounding would
otherwise read "1024 KiB" instead of "1 MiB".

`Duration` takes a `double` number of seconds and returns `m:ss` under an hour and `h:mm:ss` at or above
one. A negative or non-finite value is treated as zero, and an enormous value is capped rather than
overflowing.

### Sorting with embedded numbers

A plain string sort puts `file10` before `file2` because `1` precedes `2` character by character.
`NaturalComparer` sorts runs of digits by value instead, and compares letters without regard to case.

```csharp
using SharpProspero.Text;

var files = new List<string> { "file10", "file2", "file1" };
files.Sort(TextFormat.NaturalComparer);   // file1, file2, file10
```

`CompareNatural(left, right)` is the comparison behind the comparer, for when you need the raw result
rather than an `IComparer<string>`. It throws `ArgumentNullException` for a null argument, where the
comparer treats a null as an empty string. Only ASCII digits take the numeric path; digits from other scripts fall
through to the plain character comparison so the ordering stays consistent.

### Aligned columns

`Columns` lays ragged rows out as left-aligned columns for a control panel or a diagnostic view. Each
column widens to its longest cell plus a spacing gap; rows may have different lengths, and missing cells are
treated as empty. It returns the rows joined by newlines with no trailing spaces.

```csharp
using SharpProspero.Text;

string table = TextFormat.Columns(new string[][]
{
    ["Name", "Size", "Modified"],
    ["level1.map", TextFormat.ByteSize(20480), "2 days ago"],
    ["textures.pak", TextFormat.ByteSize(6_291_456), "1 hour ago"],
});
```

`HexDump` renders a byte span as an eight-digit offset, the bytes in hex, then the printable characters,
16 bytes per row by default — the standard view for inspecting a header or a decoder's output. It pairs
well with the hex and Base-N encoders in [Buffers and encodings](buffers.md).

## Fuzzy matching

`FuzzyMatcher` matches a short pattern against text the way an incremental "type to find" box does: the
pattern characters must appear in order but need not be adjacent, matching is case-insensitive, and a match
is scored so that adjacent runs and word starts rank higher. Use it to filter and rank a list — files,
titles, commands — and to highlight the characters that landed.

The quickest question is whether a pattern matches at all:

```csharp
using SharpProspero.Text;

bool hit = FuzzyMatcher.IsMatch("sl", "Select level");   // true: s…l in order
```

`TryMatch` also hands back a `FuzzyMatch`, a readonly record struct carrying the `Score` and the
`MatchedIndices` — the positions in the text that the pattern's characters fell on, ready to underline or
embolden.

```csharp
using SharpProspero.Text;

if (FuzzyMatcher.TryMatch(query, candidate, out FuzzyMatch match))
    Highlight(candidate, match.MatchedIndices);   // match.Score orders it against other candidates
```

### Ranking a list

`Rank` runs `TryMatch` over a sequence, drops the items that do not match, and returns the rest ordered by
descending score. A `selector` pulls the text to search from each item, so you can rank the objects
themselves rather than bare strings. Equal scores keep their original input order.

```csharp
using SharpProspero.Text;

string[] titles = ["Settings", "Save data", "Select level", "Audio setup"];
List<(string Item, FuzzyMatch Match)> hits = FuzzyMatcher.Rank("sl", titles, t => t);

foreach ((string title, FuzzyMatch match) in hits)
    Draw(title, match.MatchedIndices);   // hits[0] is the best match
```

### How the score is built

Each matched character earns a base amount, and three adjustments shape the ranking: a bonus when a
character sits right after the previous one (a run reads as a word), a larger bonus when a character begins
a word — the first character, one that follows a separator, or a camelCase boundary — and a penalty that
grows with the gap skipped to reach a character, including the distance from the start. That penalty stops
growing past a gap of ten characters, so it never cancels the base amount a matched character earns. An
empty pattern matches anything with a zero score and no indices.

{: .note }
> A score is only meaningful against other candidates matched with the same pattern. Do not compare a score
> from one query against a score from another — the numbers are relative, not absolute.

`FuzzyMatch` compares and hashes by value: two matches are equal when their score and their matched indices
are the same, which makes them safe to store in a set or use as a dictionary key.
