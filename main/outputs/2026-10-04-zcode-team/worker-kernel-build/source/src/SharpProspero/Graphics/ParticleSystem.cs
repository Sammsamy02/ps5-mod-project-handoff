// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Numerics;
using System;

namespace SharpProspero.Graphics;

/// <summary>
/// How a burst of particles is thrown: the spread of speed, direction, life, size and colour each
/// particle is given at random within these ranges. Build one for an effect — sparks, smoke, a puff of
/// dust — or start from <see cref="Burst"/>.
/// </summary>
/// <param name="MinSpeed">The slowest a particle may start moving, in pixels a second.</param>
/// <param name="MaxSpeed">The fastest a particle may start moving.</param>
/// <param name="MinAngle">The lowest launch direction, in radians (measured like the drawing angles).</param>
/// <param name="MaxAngle">The highest launch direction, in radians.</param>
/// <param name="MinLife">The shortest a particle may live, in seconds.</param>
/// <param name="MaxLife">The longest a particle may live.</param>
/// <param name="StartSize">The radius a particle starts at, in pixels.</param>
/// <param name="EndSize">The radius a particle ends at (often zero, to shrink away).</param>
/// <param name="StartColor">The colour a particle starts at.</param>
/// <param name="EndColor">The colour a particle ends at (use a zero alpha to fade out).</param>
public readonly record struct EmitParams(
    float MinSpeed, float MaxSpeed,
    float MinAngle, float MaxAngle,
    float MinLife, float MaxLife,
    float StartSize, float EndSize,
    Color StartColor, Color EndColor)
{
    /// <summary>
    /// A round burst that throws particles outward in every direction and fades them out, a sensible
    /// starting point to adjust from.
    /// </summary>
    public static EmitParams Burst(Color color, float speed = 140f, float life = 1f, float size = 4f)
        => new(speed * 0.4f, speed, 0f, MathUtil.TwoPi, life * 0.6f, life, size, 0f, color, color.WithAlpha(0));
}

/// <summary>
/// Throws and animates many small particles for an effect — sparks, smoke, a trail, an explosion. Emit a
/// burst at a point, advance the system each frame, and draw it through a <see cref="Camera2D"/> (or in
/// screen space). It keeps a fixed pool, so it allocates nothing once created and never grows past its
/// capacity.
/// </summary>
/// <example>
/// <code>
/// var sparks = new ParticleSystem(512) { Gravity = new Vector2(0, 400) };
/// // on an event:
/// sparks.Emit(hitPosition, 40, EmitParams.Burst(Color.FromRgb(255, 200, 80)));
/// // each frame:
/// sparks.Update((float)context.DeltaSeconds);
/// sparks.Draw(context.Surface, camera);
/// </code>
/// </example>
public sealed class ParticleSystem
{
    private readonly Particle[] _particles;
    private readonly GameRandom _random;
    private int _active;

    /// <summary>Creates a system holding up to <paramref name="maxParticles"/> particles at once.</summary>
    /// <param name="maxParticles">The pool size; emitting past it is ignored until particles die.</param>
    /// <param name="random">A generator for the spread, or null to seed one from the entropy source.</param>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="maxParticles"/> is not positive.</exception>
    public ParticleSystem(int maxParticles = 512, GameRandom? random = null)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(maxParticles);
        _particles = new Particle[maxParticles];
        _random = random ?? GameRandom.FromEntropy();
    }

    /// <summary>The most particles that can live at once.</summary>
    public int Capacity => _particles.Length;

    /// <summary>How many particles are alive now.</summary>
    public int ActiveCount => _active;

    /// <summary>A constant pull applied to every particle each second, in pixels a second squared (gravity, wind).</summary>
    public Vector2 Gravity { get; set; }

    /// <summary>Throws <paramref name="count"/> particles from <paramref name="position"/> within <paramref name="settings"/>.</summary>
    public void Emit(Vector2 position, int count, in EmitParams settings)
    {
        for (int n = 0; n < count && _active < _particles.Length; n++)
        {
            float life = _random.NextSingle(settings.MinLife, settings.MaxLife);
            if (life <= 0f)
                continue;
            float angle = _random.NextSingle(settings.MinAngle, settings.MaxAngle);
            float speed = _random.NextSingle(settings.MinSpeed, settings.MaxSpeed);

            _particles[_active++] = new Particle
            {
                Position = position,
                Velocity = new Vector2(MathF.Cos(angle), MathF.Sin(angle)) * speed,
                Life = life,
                MaxLife = life,
                StartSize = settings.StartSize,
                EndSize = settings.EndSize,
                StartColor = settings.StartColor,
                EndColor = settings.EndColor,
            };
        }
    }

    /// <summary>Advances every particle by <paramref name="deltaSeconds"/>, retiring the ones that expire.</summary>
    public void Update(float deltaSeconds)
    {
        if (!float.IsFinite(deltaSeconds) || deltaSeconds <= 0f)
            return;

        int i = 0;
        while (i < _active)
        {
            ref Particle particle = ref _particles[i];
            particle.Life -= deltaSeconds;
            if (particle.Life <= 0f)
            {
                // Swap the expired particle with the last live one and shrink the range; reprocess the
                // slot, which now holds a particle not yet advanced this frame.
                _particles[i] = _particles[--_active];
                continue;
            }
            particle.Velocity += Gravity * deltaSeconds;
            particle.Position += particle.Velocity * deltaSeconds;
            i++;
        }
    }

    /// <summary>
    /// Draws every live particle as a soft dot, its size and colour eased from start to end over its life.
    /// With a <paramref name="camera"/> the positions are in world space; without one they are in screen
    /// space.
    /// </summary>
    public void Draw(Surface surface, Camera2D? camera = null)
    {
        for (int i = 0; i < _active; i++)
        {
            ref readonly Particle particle = ref _particles[i];
            float t = 1f - (particle.Life / particle.MaxLife);
            float size = MathUtil.Lerp(particle.StartSize, particle.EndSize, t);
            if (size < 0.5f)
                continue;

            Color color = Color.Lerp(particle.StartColor, particle.EndColor, t);
            Vector2 screen = camera is null ? particle.Position : camera.WorldToScreen(particle.Position);
            surface.FillCircleBlended((int)screen.X, (int)screen.Y, (int)size, color);
        }
    }

    /// <summary>Removes every particle at once.</summary>
    public void Clear() => _active = 0;

    private struct Particle
    {
        public Vector2 Position;
        public Vector2 Velocity;
        public float Life;
        public float MaxLife;
        public float StartSize;
        public float EndSize;
        public Color StartColor;
        public Color EndColor;
    }
}
