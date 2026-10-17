---
title: Signed and unsigned
parent: References
nav_order: 2
---

# Signed and unsigned modules

A program and a library each have an unsigned form and a signed form, so four file kinds appear:

| Extension | What it is | Signed |
|---|---|---|
| `.elf` | A program (an executable). | No |
| `.self` | A program wrapped in a signed container. | Yes |
| `.prx` | A library or module. | No |
| `.sprx` | A library or module wrapped in a signed container. | Yes |

`.prx` and `.sprx` are libraries: you load one at run time or link against it. `.elf` and `.self` are
programs: the application's `eboot.bin` is one. The leading `s` marks the signed form of each.

## Unsigned and signed

An **unsigned** file is a plain ELF. The linker produces this form: an `eboot.bin` is an ELF program,
and a library the linker builds is a `.prx`, also an ELF.

A **signed** file wraps that ELF in a container: a header, a segment table, the ELF header and
program headers, extended information, and the program's segment data. The two signed forms differ in
how their segment data is stored, which each segment records in its own flags word:

- **Developer-accepted (readable).** The container's digest and signature slots are zero-filled and
  its segment data is plaintext. A development console accepts it. The toolchain produces and reads
  this form.
- **Retail (sealed).** The container is signed with a certificate and its segment data is encrypted.
  A retail console requires this form. Its contents cannot be read without its key, so the tools
  report the form but cannot open it.

### The header magic and the region it sizes

A container header carries the magic `0xEEF51454`, and the metadata region that follows it ends with a
`0x200`-byte signature area. The two go together. A second magic, `0x1D3D154F`, goes with a signature
area `0x100` bytes smaller. The tools read a container carrying either one and write the first.

Modules carrying either pairing run, so neither magic is better than the other; what does not appear on
any module measured is one magic with the other's region length. The tools therefore write the two
together and never separately, and the build leaves a module already wrapped in either shape alone rather than
re-wrapping it.

### Records the container keeps outside its segments

A module's **version records** sit in a segment nothing maps. No container segment carries them, so
they follow the last stored segment instead, past the size the header declares. Reading a container
back puts them where the program header places them.

### How segment data is stored

Inside the container, the program's content is not one run of bytes per segment. Each content segment
is stored in fixed **0x4000-byte blocks**, and every content segment is paired with a preceding
segment that holds **one 0x20-byte digest slot per block** — so a 0xB192C-byte code segment is 45
blocks and its digest segment is 0x5A0 bytes, not 0x20.

This matters because the loader derives the block count from the content size and then reads exactly
that many slots. A digest segment sized for a single block is correct only while the content fits in
one block; anything larger declares less digest data than the loader reads, and loading the module's
blocks fails. The toolchain sizes every pair from its content, so you never set this by hand, but it
is the reason a container cannot be assembled with a fixed-size digest segment.

## Why a module has to be wrapped

**A plain ELF does not launch.** The loader reads and authenticates a module's container header
before any of its code runs, so an `eboot.bin` left as an ELF is turned away at that point — the
application installs and then never starts, with nothing of yours having executed.

The build wraps the module for you. `build-app.ps1` runs a wrapping step after it settles the system
version and before it produces either output, so both a package and a plain folder ship a module the
loader accepts. The step leaves an already-wrapped module alone, so re-running a build is safe and a
folder that mixes a built module with ones you supply is handled correctly.

Sign a file by hand only to produce a loose `.self` or `.sprx` outside a build:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- self --sign --in module.elf --out module.self
```

## Reading any readable form

The inspector and the module reader take either a plain ELF or a developer-accepted signed container.
A signed container is unwrapped to its ELF first, so a `.sprx` reads the same way as a `.prx`:

```
sharpprospero-bindgen elf --file mylib.sprx
sharpprospero-bindgen prx --module mylib.sprx --inspect
```

The `elf` command prints the container line so the form is clear:

```
File:      mylib.sprx
Container: signed (.self / .sprx)
Class:     ELF64
OS/ABI:    9
Type:      Dynamic module
```

`self --inspect` reports which of the forms a file is, including a retail encrypted container it
cannot open:

```
sharpprospero-bindgen self --inspect --file mylib.sprx
```

## Inspecting and shrinking a module

The `elf` command has a few reading modes beyond the header, each reading the file directly (a signed
container is unwrapped first):

```
sharpprospero-bindgen elf --file eboot.bin --sizes            # loadable size by kind
sharpprospero-bindgen elf --file eboot.bin --symbols          # the dynamic symbol table
sharpprospero-bindgen elf --file eboot.bin --strings --min 6  # printable strings, at least 6 long
sharpprospero-bindgen elf --file eboot.bin --strip --out slim.bin
```

`--sizes` splits the loadable footprint into code, read-only, data and zero-filled, which is what the heap
ceiling and memory maps are measured against. `--strip` writes a smaller copy without the section-header
table a dynamic loader does not read; it applies to a module with a dynamic segment (an `eboot.bin` or a
`.prx`), not to a payload, whose section headers the loader reads.

## Converting between the forms

```
sharpprospero-bindgen self --sign    --in app.elf  --out app.self
sharpprospero-bindgen self --extract --in app.self --out app.elf
```

`self --sign` wraps an unsigned ELF in a developer-accepted signed container. `--app-version`,
`--fw-version` and `--authority` set the extended-info fields (hexadecimal); `--no-normalize` keeps
the ELF header exactly as supplied instead of adjusting the machine, OS/ABI and type for the module
loader.

`self --extract` recovers the ELF from a developer-accepted signed container. A signed container
stores only the loadable program segments, so the recovered ELF carries the same headers and loadable
content, which is what the inspector reads; it is not a byte copy of the original file, because
section headers and the padding between segments are not stored.

A retail signed, encrypted container cannot be signed or extracted here: signing it needs a
certificate, and extracting it needs its key.
