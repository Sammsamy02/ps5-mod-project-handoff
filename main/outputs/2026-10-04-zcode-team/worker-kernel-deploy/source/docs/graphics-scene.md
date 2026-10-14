---
title: 2D scenes
parent: Graphics
grand_parent: Application Modules
nav_order: 1
---

# 2D scenes

A scrolling game or map is built from three pieces in `SharpProspero.Graphics` that work together: a
camera that turns world positions into screen positions, a tile grid that draws the level and answers
collision questions, and a particle system for effects. This page wires them into one per-frame loop, then
adds pathfinding across the grid.

<details open markdown="block">
  <summary>On this page</summary>
  {: .text-delta }
- TOC
{:toc}
</details>

## Camera2D: world and screen

`Camera2D` is a movable, zoomable view onto a 2D world. Game logic stays in world coordinates — where
things actually are — and the camera converts them to screen coordinates for drawing, so a map larger
than the display can scroll and zoom while the rest of your code never thinks about the screen.

```csharp
using SharpProspero.Graphics;
using SharpProspero.Numerics;

var camera = new Camera2D(surface.Width, surface.Height);

camera.MoveTo(player.Position);           // centre on the player, in world units
camera.ClampToBounds(level.WorldBounds);  // do not scroll past the edges of the map
```

The camera exposes `WorldToScreen` and `ScreenToWorld` for converting single points (turn a touch or
cursor position back into a world location with `ScreenToWorld`), `Position` and `Zoom` for the view,
`Move` and `MoveTo` to reposition it, and `ClampToBounds` to hold it inside a rectangle. `VisibleWorldBounds`
returns the world rectangle currently on screen, which is what the tile map uses to cull tiles it does not
need to draw.

`Zoom` above one magnifies the view; it is kept finite and above zero, so a bad value never breaks the
transform. When the world is smaller than the view along an axis, `ClampToBounds` centres on that axis
instead of clamping.

## TileMap: a grid of tiles

`TileMap` is a grid where each cell holds a frame index into a `SpriteSheet`, or `TileMap.Empty` (-1) for
nothing. Build one in code with `SetTile`, or load a level exported as CSV. It draws through the camera and
answers whether a rectangle hits a solid tile.

```csharp
// tile numbers separated by commas, one map row per line:
var level = TileMap.FromCsv(PackageFile.ReadAllText("/app0/level.csv"), tileWidth: 32, tileHeight: 32);

var sheet = new SpriteSheet(tileSurface, frameWidth: 32, frameHeight: 32);
level.Draw(surface, sheet, camera);
```

`Draw` reads `VisibleWorldBounds` from the camera and only touches the cells inside it, so the cost follows
the size of the screen, not the size of the map. It scales each tile to the camera's `Zoom` as it draws:
zooming in makes tiles larger and shows less of the map, zooming out shows more of it at a smaller size.
Empty cells, and any cell whose value falls outside the sheet, are skipped.

What counts as solid is your call, passed to `Collides` as a test on the tile number. Empty cells never
reach the test — they are always open — so the test only has to answer for tiles that are actually there.
The same map drives both the look and the collision:

```csharp
// tiles numbered 16 and up are walls:
bool blocked = level.Collides(player.NextBounds, tile => tile >= 16);
if (!blocked)
    player.Position = player.NextPosition;
```

Other members fill in the rest: `GetTile` and `SetTile` read and write a cell, `TileBounds` gives the
world rectangle of a cell, `WorldToTile` maps a world point to a column and row, `Fill` sets every cell,
and `Columns`, `Rows`, `WidthInPixels`, `HeightInPixels` and `WorldBounds` describe the grid.

{: .note }
> `FromCsv` throws `ArgumentException` when the text holds no cells. A blank cell, or one that is not a
> whole number, is loaded as empty rather than failing the whole map.

## ParticleSystem: emit, update, draw

`ParticleSystem` throws and animates many small particles for an effect — sparks, smoke, a trail, an
explosion. It keeps a fixed pool sized when you create it, so it allocates nothing afterwards and never
grows past its capacity; emitting past the pool is ignored until live particles die.

```csharp
var sparks = new ParticleSystem(512) { Gravity = new Vector2(0, 500) };

// on a hit event:
sparks.Emit(hit.Position, 40, EmitParams.Burst(Color.FromRgb(255, 200, 80)));

// each frame:
sparks.Update((float)context.DeltaSeconds);
sparks.Draw(context.Surface, camera);
```

`Gravity` is a constant pull applied every second (use it for real gravity, or wind). `Update` advances
every particle and retires the ones that expire; `Draw` renders each as a soft dot whose size and colour
ease from start to end over its life. Pass the camera to draw in world space, or omit it to draw in screen
space for a heads-up effect. `Emit` throws a batch from a point; `Clear` removes every particle;
`ActiveCount` and `Capacity` report the pool.

`EmitParams` describes the spread of a burst — ranges for speed, launch angle, life, size and colour that
each particle draws from at random. Start from `EmitParams.Burst`, which throws particles outward in every
direction and fades them out, then adjust the record's fields for a tighter or slower effect:

```csharp
// a narrow upward jet of longer-lived, larger particles:
var jet = EmitParams.Burst(Color.FromRgb(120, 200, 255)) with
{
    MinAngle = MathUtil.Pi * 1.4f,
    MaxAngle = MathUtil.Pi * 1.6f,
    MinLife = 1.2f,
    MaxLife = 2.0f,
    StartSize = 6f,
};
```

## Putting a scene together

The three pieces slot into one frame: point the camera, clamp it to the map, then clear, draw tiles, step
particles and draw them. Collision and effects run off the same tile numbers.

```csharp
// setup, once:
var camera = new Camera2D(surface.Width, surface.Height);
var level = TileMap.FromCsv(PackageFile.ReadAllText("/app0/level.csv"), tileWidth: 32, tileHeight: 32);
var sheet = new SpriteSheet(tileSurface, 32, 32);
var sparks = new ParticleSystem(512) { Gravity = new Vector2(0, 500) };

// each frame:
camera.MoveTo(player.Position);
camera.ClampToBounds(level.WorldBounds);

surface.Clear(sky);
level.Draw(surface, sheet, camera);
sparks.Update(dt);
sparks.Draw(surface, camera);

// collision against the level, and an effect on a hit:
bool blocked = level.Collides(player.NextBounds, tile => tile >= 16);
if (landed)
    sparks.Emit(landing, 40, EmitParams.Burst(Color.FromRgb(255, 200, 80)));
```

Because the particle pool never allocates after creation and the tile map only draws what the camera sees,
this loop holds a steady per-frame cost regardless of how large the level grows. See [Memory](memory.md)
for keeping the rest of your frame allocation-free.

## Pathfinding on a tile grid

`GridPathfinder` — in `SharpProspero.Ai` — finds the shortest path across a grid with A*, for an enemy
that routes around walls or a cursor that steps toward a target. It works with any grid through a walkable
test, so it reads a `TileMap` directly. Reuse one instance: it keeps its working buffers, so repeated
searches allocate only the path they return.

```csharp
using SharpProspero.Ai;

var finder = new GridPathfinder(level.Columns, level.Rows) { AllowDiagonal = true };

var path = finder.FindPath((enemyCol, enemyRow), (playerCol, playerRow),
    (col, row) => level.GetTile(col, row) < 16);   // tiles below 16 are floor

foreach ((int col, int row) in path)
    StepEnemyTo(col, row);
```

The list runs from the start cell to the goal cell inclusive, or is empty when there is no way through (or
an end is off the grid or blocked). With `AllowDiagonal`, a diagonal step is only taken when both cells
beside it are open, so a path never cuts through the corner of a wall.

For terrain that costs more to cross than plain ground — mud slower than a road — use the overload that
returns a cost per cell instead of a yes/no test. A cost of one is normal ground, higher is slower, and
zero or less blocks the cell:

```csharp
var route = finder.FindPath(start, goal, (col, row) => level.GetTile(col, row) switch
{
    < 16 => 1f,   // floor
    16 or 17 => 3f, // slow terrain
    _ => 0f,      // wall — impassable
});
```

{: .tip }
> Keep costs at one or above. Values below one break the A* heuristic's assumptions and the search may no
> longer return the cheapest path.

The heuristic switches automatically with `AllowDiagonal`: Manhattan distance for four-way movement,
octile distance for eight-way. `Columns` and `Rows` report the grid the finder was built for.

## Related

- [GPU command layer](graphics-gpu.md) — the lower layer beneath the `Surface` these types draw onto.
- [Numerics and vectors](numerics.md) — `Vector2`, `RectF` and `MathUtil`, used throughout this page.
