---
title: Architecture
nav_order: 1
parent: Toolchain
---

# Architecture

SharpProspero turns a C# project into an ELF application module. The design has two halves: a
managed SDK the application links against, and a build pipeline that compiles the application ahead
of time and links it against the device libraries.

## From C# to a package

```
C# application  ->  IL  ->  ahead-of-time compiler (ILC)  ->  x86_64 object
                                                                    |
                        application object + runtime archives
                                                                    |
             the SDK linker  (adds its own start object and stubs)
                                                                    |
                                                                eboot.bin (ELF)
                                                                    |
                                                                   the packager
                                                                    |
                                                              installable *.pkg
```

The compiler emits an x86_64 object that carries the application code, the runtime it needs (garbage
collector, type system, exception handling), and unresolved references to the device services. The
SDK's own linker adds a start object that carries the `_start` entry point and generates a stub for
each imported module; it resolves the references against those stubs, applies the relocations, and
writes a loadable module. Nothing else on disk is involved in the link. The packager takes the module
plus its `sce_sys` metadata and produces the package that installs on the console.

## Layers of the SDK

The library is organized so an application depends only on the layers it needs.

### Interop

`SharpProspero.Interop` holds the bindings. Each service is a `partial` class of `[LibraryImport]`
methods plus the enums, structures and constants that go with it. Types are blittable, so the
generated marshalling is trivial and survives ahead-of-time compilation. The bindings map one to one
onto the underlying service and carry no policy of their own.

- `Interop.Kernel` — direct-memory reserve/map/release, timing, and files and directories.
- `Interop.VideoOut` — open, register buffers, submit flips, wait for the vertical blank.
- `Interop.Pad` — controller init, open, read, vibration and light bar.
- `Interop.Audio` — audio-output init, open, output and volume.
- `Interop.Sysmodule` — load, unload and query the loadable system modules.
- `Interop.Image` — PNG and JPEG decode and encode.
- `Interop.Rtc` — the real-time clock: current wall-clock time and tick.
- `Interop.Random` — random bytes from the system entropy source.
- `Interop.Dialog` — the common dialog subsystem and the message, error, text-input, save-data and
  browser dialogs.
- `Interop.Media` — media playback: start a player, add a source, pull decoded audio.
- `Interop.UserService`, `Interop.SystemService` — startup services and system parameters.

### Memory

`SharpProspero.Memory` wraps the raw allocator in two disposable regions. `DirectMemoryRegion`
reserves, maps and releases a region in one object; `FlexibleMemoryRegion` maps working buffers the
graphics processor does not read. `SystemMemory` reports the flexible memory still available and the
largest free run of direct memory. `HeapMonitor` reads managed heap usage so a loop can stay within
the ceiling set for the module, and `ObjectPool<T>` and `LruCache<TKey, TValue>` hold the allocation
rate flat. Direct memory is the source of GPU-visible buffers; the managed heap is for application
state.

### Graphics

`SharpProspero.Graphics` builds the drawing surface on top of memory and display output.
`DisplayDevice` opens the output, allocates its framebuffers from direct memory, registers them, and
presents frames. `Surface` draws into a framebuffer: clear, fill, lines, outlines, surface copies,
glyphs and text. `PngImage`, `JpegImage`, `BmpImage`, `TgaImage` and `GifImage` decode into
surface-format pixels; `PngEncoder` and `JpegEncoder` write a surface back out as a file.
`BitmapFont` carries the 8x8 glyph table as read-only data, and `TrueTypeFont` draws antialiased
glyphs at any pixel size. `Color` packs a pixel for the display format and blends between colors.
Below that CPU drawing path, `SharpProspero.Graphics.Agc` records register state and draw commands
into buffers the graphics processor runs; `Renderer3D` uses it to draw a `MeshData` mesh with the
built-in shaders. See [Graphics](graphics.md) and [GPU command layer](graphics-gpu.md).

### Input

`SharpProspero.Input` decodes a controller sample into `GamePadState` (button bits, stick axes,
trigger travel, motion as orientation, acceleration and angular velocity, and the touch-pad contacts)
and drives the controller's motors and light bar. The frame loop keeps the previous sample so the
per-frame context can report button edges (pressed and released this frame).

### Audio

`SharpProspero.Audio` opens a stereo output port that paces the caller to the audio clock.

### Timing

`SharpProspero.Timing` holds a monotonic clock (`GameClock`) for frame pacing, a wall-clock reader
(`SystemClock`) for the calendar date and time, and timers driven by the frame delta: `Cooldown`,
`Interval`, `Countdown`, `FixedTimestep` for a fixed simulation step, and `FrameScheduler` for
delayed and repeating callbacks.

### Storage

`SharpProspero.Storage` reads the files bundled with the module (`PackageFile`) and browses and
changes files and directories by path (`FileSystem`).

### Modules

`SharpProspero.Modules` loads a system module by id or a supplied `.prx` at run time.

### Media

`SharpProspero.Media` plays a media file and hands back decoded audio frames.

### Numerics

`SharpProspero.Numerics` holds the arithmetic game code runs on: `Vector2` and `RectF`, the scalar
helpers in `MathUtil`, the overlap tests in `Collision`, a `Quadtree<T>` spatial index, a
`RectPacker` that fits many small rectangles into one atlas, coherent `NoiseField` sampling for
procedural content, a reproducible gameplay generator (`GameRandom`) drawing its seed from the
entropy source (`HardwareEntropy`), a `WeightedTable<T>` for drop and encounter draws, and the 3D
set: `Camera3D`, `Transform`, `Ray`, and the `BoundingBox` and `BoundingSphere` volumes a `Frustum`
culls against.

### Platform

`SharpProspero.Platform` is the system-service layer: console facts, signed-in users and system
parameters, system settings and the events the module receives, the overlay dialogs and
notifications, save data, trophies and the telemetry behind them, the content library and screen
capture, installing and launching titles, the attached storage and disc devices, and networking from
a TCP or UDP socket up to an HTTP client and server. Services that a title does not link against
are loaded at run time and resolved by name through `SystemLibrary`, so no extra library is needed
to reach them. `FirmwareVersion` and `FirmwareSupport` read the running system version and report
whether the SDK supports it, and `FirmwareRegistry` holds, in one place, what each
resolved-by-name service depends on and the version it was confirmed on. See
[Firmware compatibility](firmware.md).

### Threading and diagnostics

`SharpProspero.Threading` moves slow work off the frame loop. `BackgroundOperation` and
`BackgroundOperation<T>` each run one job on a thread of their own; `WorkQueue` runs a stream of jobs
on a small pool of worker threads and hands back a `WorkItem<T>` per result. `Dispatcher` is the way
back: a worker posts a callback and the frame thread runs it when it drains the queue, which is where
the drawing surface and application state are safe to touch. `ProsperoApp` exposes its own
`Dispatcher` and drains it once per frame before `OnFrame`. See [Threading](threading.md).

`SharpProspero.Diagnostics` reports on a running module. `Log` writes leveled messages to the sinks
attached to it — a file (`FileLogSink`) or the console (`ConsoleLogSink`) — and drops anything below
`MinimumLevel`. `FrameStats` tracks frame times over a window and reports the rate, the one-percent
low and any percentile, as a text readout or a graph. See [Diagnostics](diagnostics.md).

### Interface

`SharpProspero.Ui` builds screens out of controls (labels, buttons, lists, checkboxes, progress bars)
so an application does not draw its interface by hand. A `UiScreen` lays a tree of controls out into a
rectangle, moves focus with the controller, and draws it on a `Surface`. The layout, focus navigation
and input handling are plain logic with no device dependency; only the drawing touches the framebuffer.

### Payload

`SharpProspero.Payload` provides the API surface a payload uses instead of the application host.
`PayloadNetwork` wraps the plain socket calls the operating-system library publishes by name (a
payload has no dynamic linker, so the managed network types an application module uses do not
resolve). `PayloadKernel` and `PayloadKernelIo` walk the kernel's process list and modify process
credentials and filesystem jails through the CRT-emitted per-field accessors (initialized once
during CRT startup from the loader's `payload_args` block). `PayloadEntryPoint` holds the loader's
argument pointer. `PayloadFileSystem`, `PayloadProcess`, `PayloadNotification`,
`PayloadHardwareInfo`, `PayloadSysctl`, `PayloadDebug`, `PayloadDlfcn`, `PayloadHttp2`,
`PayloadBrowser`, `PayloadRandom`, and `PayloadUserService` each expose one subsystem of the
device through the syscall gateway the CRT provides. `KernelOffsets1001` records the structure
offsets for firmware 10.01.

### Application

`SharpProspero.Application` ties the layers together. `ProsperoApp` opens the display and controller,
runs a vertical-blank-paced loop, and hands each frame a reused `FrameContext`. An application
overrides `OnLoad`, `OnFrame` and `OnUnload`.

## Entry points

The loader calls `_start`, which the linker's start object defines. It sets the C library up from the
block the loader hands it, registers the teardown routine, runs the module's constructors, then calls
the application's `Main` and passes its return value to `exit`. A module can also expose extra
C-callable entry points by marking a static method `[UnmanagedCallersOnly]`; the compiler exports it
as an unmanaged symbol because `IlcExportUnmanagedEntryPoints` is set in the build props.

```csharp
using System.Runtime.InteropServices;

internal static class Exports
{
    [UnmanagedCallersOnly(EntryPoint = "sharpprospero_frame")]
    public static int Frame(int handle) => 0;
}
```

The exported method takes and returns blittable types only, and does not throw across the boundary.

## Design choices

- **Blittable interop.** Every binding uses primitive and pointer types, so no runtime marshalling
  code is generated and the trimmer keeps nothing extra.
- **No steady-state allocation.** The frame loop reuses one context object and draws into
  pre-allocated framebuffers, so a running application does not grow the heap each frame.
- **Explicit lifetimes.** Direct-memory regions, the display and the controller are disposable and
  released in a defined order at shutdown.
- **A bounded heap.** The application sets a hard ceiling that is baked into the image; the heap
  monitor reads against it.
- **Compatibility by name, not by address.** The SDK reaches the system through exported functions and
  loadable modules, resolved by name, so one build runs across a range of system versions. It pins
  nothing to a version-specific offset.
