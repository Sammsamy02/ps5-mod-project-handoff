---
title: Animation
parent: Application host
grand_parent: Application Modules
nav_order: 4
---

# Animation

`SharpProspero.Animation` moves a number from one value to another over a set time along an easing
curve, so a panel slides in, a bar fills, or a colour fades without hand-written per-frame maths. It
also evaluates smooth 2D paths for motion a straight A-to-B move cannot express.

## Tween a value over time

A `Tween` moves a `float` from a start to an end over a duration, shaped by a curve. It holds no
reference to what it drives, so one tween can move a position, an alpha, or a colour channel. Create
it once, then advance it each frame with the time since the last and read the result:

```csharp
using SharpProspero.Animation;

var slideIn = new Tween(from: -200, to: 0, durationSeconds: 0.3f, Ease.OutCubic);

// each frame, in OnFrame:
int x = (int)slideIn.Update((float)frame.DeltaSeconds);
// draw the panel at x this frame
if (slideIn.IsComplete) { /* settled */ }
```

`Update(deltaSeconds)` advances the tween and returns the new `Value`; a zero or negative delta leaves
it where it is. The frame delta comes from `FrameContext.DeltaSeconds` (see [Application](application.md)),
which is a `double`, so cast it to `float` on the way in.

`durationSeconds` must be positive; zero or a negative value throws `ArgumentOutOfRangeException` from
the constructor, not on the first update.

Read the running state through three properties:

| Member | Meaning |
| --- | --- |
| `Value` | The current value, shaped by the easing curve, between `From` and `To`. |
| `Progress` | How far through the current run, 0 to 1, before the curve is applied. |
| `IsComplete` | True once a `Once` tween has reached the end; a looping tween never completes. |

`From`, `To`, `Ease` and `Mode` are readable too, and return what the constructor was given.

`Restart()` moves the tween back to its start so it runs again from the beginning.

### End behaviour with TweenMode

`TweenMode`, passed to the constructor, decides what happens when a run reaches the end:

| Mode | Behaviour |
| --- | --- |
| `Once` | Settles on the end value and reports `IsComplete`. The default. |
| `Loop` | Jumps back to the start and runs again, without end. |
| `PingPong` | Runs to the end, back to the start, then forward again, without end. |

```csharp
var pulse = new Tween(from: 0.4f, to: 1f, durationSeconds: 0.8f, Ease.InOutSine, TweenMode.PingPong);

// each frame:
float alpha = pulse.Update((float)frame.DeltaSeconds);
```

{: .note }
> A `Loop` or `PingPong` tween folds its elapsed time back into one period each update, so it stays
> precise even when left running for a long time.

## Easing curves

`Ease` picks the shape of the motion. `Easing.Apply(ease, t)` maps a progress value `t` (clamped to
0..1) through the chosen curve, where 0 is the start and 1 is the end.

| Curve | Shape |
| --- | --- |
| `Linear` | A straight line: constant speed. |
| `InQuad` / `OutQuad` / `InOutQuad` | Squared ease in, out, or both. |
| `InCubic` / `OutCubic` / `InOutCubic` | Cubed ease — stronger than the squared form. |
| `InSine` / `OutSine` / `InOutSine` | Gentle sine ease in, out, or both. |
| `OutBack` | Overshoots past the end and settles back, for a springy finish. |
| `OutBounce` | Lands and bounces a few times before settling, like a dropped ball. |

For a one-off value without a tween, `Easing.Interpolate` gives the eased position from one value to
another at a fraction `t`:

```csharp
float alpha = Easing.Interpolate(from: 0f, to: 1f, t: 0.5f, Ease.InOutSine);
```

This is the same maths a `Tween` runs internally, so use it when you already track your own progress
and only need the eased reading.

## Splines for curved paths

`Spline` (in `SharpProspero.Animation`) evaluates smooth 2D paths: a camera or enemy that follows a
curve, a pan across a photo, a projectile arc. It works in `Vector2` from
[SharpProspero.Numerics](numerics.md).

Quadratic and cubic Bezier curves run from control points. The quadratic form takes a start, one
control point, and an end; the cubic form takes two control points:

```csharp
using SharpProspero.Animation;
using SharpProspero.Numerics;

Vector2 quad = Spline.Bezier(start, control, end, t);
Vector2 cubic = Spline.Bezier(start, controlA, controlB, end, t);
```

A Catmull-Rom spline passes through a list of waypoints. Give it the points and a `t` from 0 (the
first point) to 1 (the last); the ends are clamped so the curve does not overshoot past the first and
last point. An empty list throws `ArgumentException`; a list of one returns that point:

```csharp
var waypoints = new[]
{
    new Vector2(0, 0),
    new Vector2(120, -40),
    new Vector2(260, 30),
    new Vector2(400, 0),
};

Vector2 pos = Spline.CatmullRom(waypoints, t);
```

`CatmullRomSegment(p0, p1, p2, p3, t)` evaluates a single segment from `p1` to `p2`, with `p0` and
`p3` shaping the tangents at each end. Use it when you drive one span at a time rather than a whole
path.

Drive `t` with a `Tween` to move along a curve over time, combining the two ideas — an eased tween on
`t` gives a path that both bends in space and eases in speed.
