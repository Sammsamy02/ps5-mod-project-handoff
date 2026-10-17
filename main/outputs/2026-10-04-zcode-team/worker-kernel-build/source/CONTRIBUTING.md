# Contributing to SharpProspero

## Build and test

The build needs the .NET 10 SDK. On Windows it also needs WSL, because the ahead-of-time compile runs
on Linux; the build starts it for you. Check the machine first:

```bash
pwsh SharpProspero/doctor.ps1
```

```bash
dotnet build SharpProspero/SharpProspero.slnx
```

```bash
dotnet test SharpProspero/tests/SharpProspero.Tests/SharpProspero.Tests.csproj
```

Build a module end to end:

```bash
pwsh SharpProspero/build/build-app.ps1 -ProjectPath SharpProspero/src/SharpProspero.Sample/SharpProspero.Sample.csproj -Output Folder -OutputFolder out
```

That writes a `module/` folder holding `eboot.bin`, `sce_sys/` and `sce_module/`. Ship the whole
folder to the console, not `eboot.bin` on its own.

## Reporting an issue

Search the existing issues and add to a matching thread rather than opening a second one.

State what you expected, what happened instead, and the shortest steps that show it. An issue about a
module running on the console also needs the details below. The same symptom has many causes, and the
logs are what tell them apart.

| Detail | When |
|---|---|
| The firmware the console runs, exactly, for example `10.01` | Always |
| The firmware the module targets | Always |
| Every module in `sce_module/` besides `libc.prx`, and any module the code loads by name at run time | Always |
| A core dump | The application crashes |
| A kernel log covering only the moment the module ran | The application crashes, or runs but does not do what it should |

This reports the firmware a module targets:

```bash
dotnet run --project SharpProspero/tools/SharpProspero.Bindings.Generator -- sysver --folder <module-folder>
```

Attach the build as a zip of the whole `module/` folder, and name the release, or the branch and
commit, it came from.

### Core dump

Take the folder carrying your title id from:

```
/devlog/system/sce_coredumps.0/
```

Attach all of it. Compress and link it when it is too large for an issue.

### Kernel log

A whole session runs to tens of thousands of lines and the moment that matters is a handful of them.
Capture only that moment:

1. Attach to the log.
2. Clear it with `cls` or `clear`.
3. Detach, then attach again, so the log starts from here.
4. Start the application.
5. Do the exact thing that crashes it, or that fails to do what it should.
6. Detach and copy the output.

Paste it in a fenced code block, or attach it as a `.txt` file when it runs long. Do not trim it: the
lines either side of the failure usually identify it.

### Template

```markdown
### What I expected

### What happened instead

### Steps to reproduce
1.
2.
3.

### Console
- Firmware the console runs:
- Firmware the module targets:
- Modules in sce_module besides libc.prx:
- Modules loaded by name at run time:

### Build
- Built from: (release, or branch and commit)
- Title id:

### Attached
- [ ] Core dump (if it crashed)
- [ ] Kernel log covering only the moment the module ran
- [ ] The module folder as a zip
```

For a feature, describe the problem before the solution: what you want to do, what the SDK makes you
do instead, and how the answer looks from a caller's side.

## Pull requests

Open an issue first for anything beyond a small fix, so you and the maintainer settle the approach
before you write it.

Branch off `main` and name the branch after the work: `sharpprospero-flip-wait`. Write one commit per
self-contained change. The subject says what it does; the body says why the old behaviour was wrong
and what you measured:

```
SharpProspero: wait for the flip that was asked for
```

Give the pull request:

- What changed and why, so a reviewer does not read the diff to find the point.
- How you verified it. Name the test, the output you compared, or the module you built and ran.
- A test. A behaviour change needs one that fails before and passes after; a fix needs one that pins
  the bug so it cannot return.
- A green suite.
- The documentation pages your change contradicts, corrected in the same pull request, and comments on
  any public API you added.
- Nothing unrelated: no reformatting of untouched files, no version bumps, no drive-by renames.

When a test starts failing because you corrected the code it asserts, read it before you touch it: it
may have been pinning the defect. Rewrite it to assert the correct behaviour and say so in the commit.

Establish a claim about how the system behaves from the headers, from a module you can read, or from a
build you ran, never from what a similar system does. Say what you checked it against.

Before you mark it ready:

- [ ] `dotnet build SharpProspero/SharpProspero.slnx` succeeds and the suite passes.
- [ ] New behaviour has a test; a fixed bug has a test that pins it.
- [ ] Public API carries documentation comments.
- [ ] The documentation matches the change.
- [ ] No debug scaffolding, commented-out code or leftover diagnostic output.
- [ ] The diff holds nothing the description leaves out.

## Style

C# 14 and .NET 10.

Comments say why, not what the line already says. Documentation stays technical, accurate and readable
by someone doing the task.

## License

GPL-3.0-or-later. Contributions ship under the same terms.
