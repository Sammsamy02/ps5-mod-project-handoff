---
title: Content and capture
parent: System services
grand_parent: Application Modules
nav_order: 5
---

# Content and capture

The console keeps a library of captures and imported media, and it can grab the finished screen as a
screenshot or a video clip. Both live in `SharpProspero.Platform`. The library and capture services are
permission-gated: a service refuses with a `ProsperoException` when the running module is not allowed to
reach it, so treat every open as something that can fail.

## The content library

`ContentLibrary` lists the photos and videos on the console, counts them, and totals their size. Open
it, query it, dispose it. The content type comes from `SharpProspero.Interop.Content`.

```csharp
using SharpProspero.Platform;
using SharpProspero.Interop.Content;

using var library = ContentLibrary.Open();
long photos = library.Count(SceContentSearchContentType.Photo);
foreach (ContentItem item in library.List(SceContentSearchContentType.Photo))
    Show(item.Title, item.Path, item.Size);
```

A `ContentItem` carries the `ContentId`, `Type`, `MimeType`, `Title`, `Path`, `IconPath`, `Size`, and an
`Available` flag. `Count` returns how many items of a type exist. `List` reads one page of them, sorted
by title: it returns at most `ContentSearch.MaxLimit` rows (92) per call, so walk a larger library by
raising its `offset` argument until a call returns fewer rows than it asked for. `Open` takes an optional
working-heap size; the default, `ContentLibrary.DefaultMemorySize`, is 3 MB and suits ordinary listing.

{: .note }
> Listing needs the content-search permission. Without it, the first query raises a `ProsperoException`
> whose code is `ContentSearch.PermissionRequired`.

## Reading one item's metadata

`ContentMetadata` reads the individual fields of one item, opened by path or by content id from the
library. Each field is typed.

```csharp
using ContentMetadata meta = library.OpenMetadata(item.Path);
string title = meta.GetText(ContentSearch.FieldTitle);
long width = meta.GetInt(ContentSearch.FieldWidth);
double duration = meta.GetFloat(ContentSearch.FieldDuration);
```

`GetText`, `GetInt`, `GetFloat`, and `GetTick` read a named field in the type it stores, and
`GetFieldInfo` reports that type and the field's byte size when you need to choose. `GetText` returns an
empty string for a field that is not text. The names are constants on
`SharpProspero.Interop.Content.ContentSearch`: `FieldTitle`, `FieldMimeType`, `FieldCreatedTime`,
`FieldSize`, `FieldWidth`, `FieldHeight`, and `FieldDuration`.

## Exporting a file into the library

`ContentExporter` copies a file a module produced — a rendered image, a recording — into the console's
content library so it shows up alongside the user's own captures.

```csharp
using var exporter = ContentExporter.Open();
string savedPath = exporter.Export("/data/render.png", "My Render", ContentExport.FormatImagePng);
```

`Export` takes the source path, a display title, and the media type, and returns the path the file
was written to inside the library. `ContentExport` names the media types: `FormatImageJpeg`,
`FormatImagePng`, `FormatImageGif`, `FormatVideoMp4`, and `FormatVideoWebm`.

## Deleting content

`ContentDeleter` removes a user-owned item — a screenshot, a clip, a download — by path or by id.

```csharp
using var deleter = ContentDeleter.Open();
deleter.DeleteByPath("/user/photo/old.png");
deleter.DeleteById(contentId);
```

{: .warning }
> Deletion is permanent. Only user-owned content can be removed, and the delete does not prompt, so
> confirm with the user before calling it.

## Capturing the finished screen

`ShareCapture` captures the whole finished screen — the application together with the system overlays —
and saves it to the console's capture gallery. It is the capture the share button drives: a 2K or 4K
screenshot, or the last several seconds of output as a clip. This differs from encoding a drawing
surface, which captures only what the application itself drew (see [Graphics](graphics.md)).

```csharp
using SharpProspero.Platform;
using SharpProspero.Interop.Share;

using var share = ShareCapture.Start();
share.CaptureScreenshot(ScreenshotFormat.Png4K);   // saved to the gallery in the background
share.CaptureRecentClip(secondsBack: 30);          // save the last 30 seconds as a clip
```

Captures are asynchronous: each call returns a request id and the image or clip is written in the
background. `RecordingStatus` reports whether the 2K and 4K clip recorders are stopped, paused or
running. `Block(ShareFeature.Screenshot)` prevents capture while a sensitive screen is shown, and
`Allow` re-enables it. `SetScreenshotOverlay(filePath, marginX, marginY, origin)` lays an image over
captured screenshots, anchored by a `ScreenshotOrigin` corner, edge or center point with the given
margins. `Start` takes the service's heap size and helper-thread priority when the defaults do not suit.

## Live capture of the composited screen

`SystemAvCapture` reads the live system-composited video — the finished screen the whole system draws —
for a recorder or a stream, rather than saving a gallery clip. It opens a video channel only; no audio
reaches the caller through this type.

{: .important }
> This is an advanced, privileged surface. The capture service runs behind a system channel, and the
> process must hold the authority to reach it, which a plain application sandbox does not. Opening it
> without that authority fails with a permission error. Treat it as best-effort.

```csharp
using SharpProspero.Platform;
using SharpProspero.Interop.AvCapture;

using var capture = SystemAvCapture.Open();
capture.OpenVideo(Avcap2VideoConfig.Create());
capture.Start();
while (recording)
{
    if (capture.TryReadVideo(out Avcap2VideoFrameInfo frame) && frame.IsValid)
        Encode(frame);          // a privileged consumer reads the frame planes
}
capture.Stop();
```

`Avcap2VideoConfig.Create()` returns a cleared configuration. `Kind` selects the channel — a non-zero
value opens the primary one — and `Mode` selects a sub-mode. The default mode reads no further fields.
Modes one and two need `FrameAreaAddress` and `FrameAreaLength` to name direct memory that is already
mapped and at least that large. Mode one also needs `AreaAddress` and `AreaLength`; mode two accepts that
pair only when both are set or both are clear. The open call is refused when any of these is missing or
short.

`GetFramePitch` reports the row pitch of the captured frames, `IsVideoOpen` and `IsStarted` report state,
and `CloseVideo` closes the channel while leaving the service running. The frame descriptor's leading
words are a service-internal layout; `IsValid` is what a caller checks.

For a recording path that needs no elevated privilege, save a gallery clip with `ShareCapture`, or
encode the application's own frames with the image encoders on the [Graphics](graphics.md) page.

## Related pages

- [Packages and devices](packages-devices.md) — installing titles and reading connected devices.
- [System services](system-services.md) — the permission model behind these services.
- [Media](media.md) — decoding and playing back the video files the library holds.
