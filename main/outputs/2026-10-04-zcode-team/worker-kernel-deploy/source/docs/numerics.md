---
title: Numerics and vectors
parent: Data and utilities
grand_parent: Application Modules
nav_order: 2
---

# Numerics and vectors

Small value types and helpers for the arithmetic game and drawing code runs every frame: vectors and
rectangles, scalar math, overlap tests, seedable randomness, a spatial index, and the 3D camera, ray and
bounding-volume set. Everything here lives in `SharpProspero.Numerics` and works in single precision.

<details open markdown="block">
  <summary>On this page</summary>
  {: .text-delta }
- TOC
{:toc}
</details>

## Vectors

`Vector2` is a small value type for a position, a velocity or a direction, so movement and steering read as
arithmetic rather than a pair of loose floats.

```csharp
var position = new Vector2(100f, 60f);
var velocity = Vector2.UnitX * 240f;                 // 240 px/sec to the right
position += velocity * (float)context.DeltaSeconds;  // advance by the frame time
float away = Vector2.Distance(position, target);
Vector2 toward = (target - position).Normalized();
```

It has the usual operators, `Length`/`LengthSquared`, `Normalized`, `Dot`, `Distance`/`DistanceSquared`,
`Lerp` and `Rotate`, and the `Zero`, `One`, `UnitX` and `UnitY` constants. For steering there is
`MoveTowards` (advance toward a target by a capped step), `ClampLength` (cap the magnitude),
`Perpendicular` (a quarter turn), `Cross` (the perpendicular dot product, whose sign gives turn direction),
and the angle pair `FromAngle`/`ToAngle`. `WithX`/`WithY` return a copy with one component changed.

## Rectangles

`RectF` is an axis-aligned rectangle for a bounding box or a hit area. It answers whether it holds a point
or another rectangle, where two rectangles overlap, and pulls a point to the nearest spot inside.

```csharp
var button = new RectF(x, y, w, h);
if (button.Contains(cursor)) Press();                  // point in rectangle

Vector2 snapped = playfield.Clamp(ball);               // nearest spot inside
RectF overlap = a.Intersection(b);                     // Empty when they miss
```

`RectF` has `Contains`, `Intersects`, `Intersection`, `Union`, `Inflate`, `Offset` and `Clamp`, plus the
`FromEdges` and `FromCenter` constructors, the `Empty` value, and the read-only
`Left`/`Top`/`Right`/`Bottom`, `Position`, `Center`, `Size` and `IsEmpty` members. Edges follow the
half-open rule — top and left are inside, bottom and right
are not — so rectangles that share an edge do not both claim it.

## Scalar math

`MathUtil` holds the floating-point helpers that go with the vectors: blending between values, mapping a
value from one range to another, easing an edge, and stepping toward a target.

```csharp
float health = MathUtil.Clamp01(hp / maxHp);
float eased  = MathUtil.SmoothStep(0f, 1f, t);
float volume = MathUtil.Remap(distance, 0f, 400f, 1f, 0f); // near = loud, far = silent
float facing = MathUtil.WrapAngle(heading + turn);         // back into -pi..pi
```

It provides `Lerp` and `LerpClamped`, `InverseLerp` and `Remap`, `SmoothStep`, `MoveTowards`,
`Clamp`/`Clamp01`, `Approximately`, `Repeat` and `PingPong`, the angle helpers `DegreesToRadians`,
`RadiansToDegrees` and `WrapAngle`, and the `Pi`, `TwoPi`, `DegreesPerRadian` and `RadiansPerDegree`
constants.

For a value that should ease toward a target and settle without overshooting - a camera that follows the
player, a slider that glides to its new spot - `SmoothDamp` is the one to reach for. It carries a velocity
between calls, so keep that in a field and pass it by reference each frame; `smoothTime` is roughly how
long the move takes. `SmoothDampAngle` does the same for a heading, and `LerpAngle` blends two angles the
short way round. `Vector2.SmoothDamp` smooths a position with the same feel.

```csharp
Vector2 cameraVelocity; // kept between frames
cameraTarget = player.Position;
cameraPos = Vector2.SmoothDamp(cameraPos, cameraTarget, ref cameraVelocity, smoothTime: 0.25f, deltaTime);
```

## Collision tests

`Collision` is a static class of the overlap tests game code reaches for, over `Vector2` and `RectF`:
circles, a circle against a rectangle, a point in a circle, and where two line segments cross.
Rectangle-against-rectangle and point-in-rectangle live on `RectF` itself.

```csharp
if (Collision.CircleOverlapsRect(ball, radius, paddle)) Bounce();
if (Collision.CirclesOverlap(a, ra, b, rb)) Hit();

if (Collision.SegmentIntersection(eye, target, wallA, wallB, out Vector2 where))
    DrawSpark(where);                                  // line of sight blocked here
```

The full set is `PointInCircle`, `CirclesOverlap`, `CircleOverlapsRect`, `SegmentsIntersect`,
`SegmentIntersection` (with the crossing point as an `out`), and `SegmentIntersectsRect` for a segment that
touches or enters a rectangle. A segment test reports parallel or collinear lines as no single crossing.

## Randomness

There are two random sources. `GameRandom` is a fast, reproducible generator for gameplay; the same seed
always yields the same sequence, so a replay or a procedural level comes out identical. `HardwareEntropy`
draws unpredictable bytes from the system for seeds.

```csharp
var rng = new GameRandom(seed: 1234);   // same seed, same sequence
int roll = rng.Next(1, 7);              // 1..6 (max is exclusive)
double t = rng.NextDouble();            // 0..1
bool crit = rng.NextBool(0.1);          // true 10% of the time

var unpredictable = GameRandom.FromEntropy();       // seeded from the system
ulong token = HardwareEntropy.NextUInt64();         // straight from the entropy source
```

`GameRandom` gives `NextUInt64`, `NextUInt32`, `NextDouble` (0 to 1), `Next(max)` and `Next(min, max)`
(max exclusive),
`NextSingle` (with an optional range), `NextBool`, `Pick` (one item from a set) and `Shuffle` (reorders a
span in place). `Pick` and `Shuffle` take spans, so they work over an array or a slice without allocating.

```csharp
ReadOnlySpan<string> loot = ["sword", "shield", "potion"];
string drop = rng.Pick(loot);           // one of the three, evenly

rng.Shuffle(deck);                      // deck is a Span<Card>, reordered in place
```

`HardwareEntropy` fills a span with `Fill` or hands back a value with `NextUInt64`; seed a `GameRandom`
from it when you want an unpredictable start.

For a draw that is not even - a loot table, a drop chart, a random-encounter list - `WeightedTable<T>` gives
each entry a weight and returns one with a chance proportional to it. It draws through a `GameRandom`, so a
seeded run repeats exactly. Weights are any non-negative numbers; an entry at weight 3 comes up three times
as often as one at weight 1.

```csharp
var loot = new WeightedTable<string>()
    .Add("common", 70)
    .Add("uncommon", 25)
    .Add("rare", 5);

string drop = loot.Pick(rng);          // "common" about 70% of the time
loot.TryPick(rng, out string safe);    // false instead of throwing on an empty table
```

`Add` chains, `Count` and `TotalWeight` report the contents, `Clear` empties it, and an entry at weight 0
is kept but never drawn.

## Coherent noise

Where `GameRandom` gives independent values, `NoiseField` gives *smooth* ones: sampling nearby coordinates
returns nearby values, so it draws terrain heights, cloud and marble textures, and organic motion rather
than static. The same seed and coordinate always return the same value (-1 to 1), so a world is
reproducible.

```csharp
var noise = new NoiseField(seed: 2024);
float height = noise.Noise2D(x * 0.05f, z * 0.05f);        // one smooth layer
float terrain = noise.FractalNoise2D(x * 0.02f, z * 0.02f, octaves: 5); // layered detail
float density = noise.Noise3D(x, y, z);                     // caves, clouds
```

`Noise2D`/`Noise3D` are one layer; `FractalNoise2D` sums octaves at rising frequency and falling amplitude
for detail, with `persistence` and `lacunarity` to shape it. Scale the input coordinates to set the
feature size - smaller multipliers give broader shapes.

{: .important }
> `GameRandom` is for gameplay, not for keys, tokens or anything that must resist guessing. Take those
> bytes from `HardwareEntropy`. For hashing and checksums see [Hashing and checksums](hashing.md).

## Spatial queries

When a scene holds many things, testing every pair is wasteful. `Quadtree<T>` indexes items by rectangle
and answers "what is in this area" by visiting only the parts of the world near the query — collision
broad-phase, off-screen culling, or picking under the cursor.

```csharp
var tree = new Quadtree<Entity>(worldBounds);
foreach (Entity e in entities)
    tree.Insert(e, e.Bounds);                          // rebuild each frame for moving items

foreach (Entity near in tree.Query(visibleArea))
    near.Draw(surface);
```

`Query(RectF)` returns a fresh `List<T>`; the `Query(RectF, List<T>)` overload appends into a list you
own, so a per-frame query reuses one buffer instead of allocating — the kind of discipline the
[Memory](memory.md) page argues for. `Insert` grows the tree, `Count`
reports the item total, and `Clear` empties it while keeping the same bounds and limits. The constructor
takes optional `maxItemsPerNode` and `maxDepth` limits that bound how finely it subdivides. An item that
straddles a split line is kept at that level rather than duplicated, so a query never returns the same item
twice.

{: .note }
> `Quadtree<T>` groups by rectangle, not by exact shape. Treat a hit as a candidate and follow it with the
> precise test — `Collision.CirclesOverlap` or `RectF.Intersects` — on the pairs it returns.

For curved motion, `Spline` evaluates Bezier and Catmull-Rom curves for a camera path or a projectile arc.
It lives in `SharpProspero.Animation` and is covered on the [Animation](animation.md) page.

## 3D math

The same namespace carries the 3D set the renderer runs on. `Camera3D` holds a position, a target and the
projection settings and hands back `View`, `Projection` and `ViewProjection` matrices, with `Perspective`
and `Orthographic` builders, `WorldToScreen` for placing a marker over an object, and `ScreenToRay` for
picking under the cursor. `Transform` is a position, rotation and scale with a `Matrix`, the
`Forward`/`Right`/`Up` axes, `LookAt`, `Rotate` and `TransformPoint`/`TransformDirection`.

```csharp
var camera = new Camera3D { Position = new Vector3(0, 1.5f, 4.5f), AspectRatio = 1920f / 1080f };
Ray pick = camera.ScreenToRay(cursorX, cursorY, 1920, 1080);
if (pick.IntersectSphere(target.Bounds) >= 0f)     // negative means the ray misses
    Select(target);
```

`Ray` intersects a plane, a sphere, a box or a triangle and returns the distance along itself to the hit,
negative for a miss; a ray that starts inside a sphere reports 0. `BoundingBox` builds from a point cloud
with `FromPoints`, grows with `Encapsulate`, re-fits around a matrix with `Transform(Matrix4x4)`, and
answers `Contains` and `Intersects`. `BoundingSphere` shares `FromPoints`, `Contains` and `Intersects`, and
adds `FromBox` for the sphere around a box. `Frustum` builds from a view-projection matrix and tests a
point, a sphere or a box against the six planes, so only what the camera can see is drawn. See
[GPU command layer](graphics-gpu.md) for the renderer that consumes them.

## Packing rectangles

`RectPacker` fits many small rectangles into one larger area without overlap - the job behind building a
sprite sheet or a glyph atlas out of separate images. It works in whole pixels and fills bottom-left along a
running skyline, which keeps the result tight. Give it a size and an id per piece, and it reports where each
one landed so you can copy the image in and record the region.

```csharp
using var atlas = new PixelBuffer(1024, 1024);
Surface sheet = atlas.AsSurface();
var packer = new RectPacker(1024, 1024);

foreach ((int id, Surface image) in sprites)
{
    PackedRect? slot = packer.Insert(image.Width, image.Height, id);
    if (slot is { } r)
        sheet.Blit(image, r.X, r.Y);                   // and remember (r.X, r.Y, r.Width, r.Height)
}
```

`Insert` returns null when a piece will not fit in the space left. `Pack` takes a whole batch, sorts it
largest-first for a tighter fit, and returns the pieces that fit - compare that count with the number you
gave to see whether any were too big. `Occupancy` is the fraction of the area filled, and `Reset` clears
it to pack again.
