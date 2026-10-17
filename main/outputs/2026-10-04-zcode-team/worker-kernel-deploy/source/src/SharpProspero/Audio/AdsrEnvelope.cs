// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Audio;

/// <summary>The phase an <see cref="AdsrEnvelope"/> is currently in.</summary>
public enum EnvelopePhase
{
    /// <summary>Silent and waiting for a note.</summary>
    Idle,
    /// <summary>Rising from zero to full level.</summary>
    Attack,
    /// <summary>Falling from full level to the sustain level.</summary>
    Decay,
    /// <summary>Holding at the sustain level while the note is held.</summary>
    Sustain,
    /// <summary>Falling from the current level back to zero after release.</summary>
    Release,
}

/// <summary>
/// An attack-decay-sustain-release envelope: the volume shape of a note from the moment a key goes down
/// to after it lifts. Multiply a voice by <see cref="Level"/> each frame to give it a natural swell and
/// fade instead of clicking on and off. Times are in seconds; the sustain is a level from 0 to 1.
/// </summary>
public sealed class AdsrEnvelope
{
    /// <summary>Seconds to rise from silence to full level after <see cref="NoteOn"/>.</summary>
    public float Attack { get; set; } = 0.01f;

    /// <summary>Seconds to fall from full level to the sustain level.</summary>
    public float Decay { get; set; } = 0.1f;

    /// <summary>The held level, 0 to 1, while the note stays down.</summary>
    public float Sustain { get; set; } = 0.7f;

    /// <summary>Seconds to fall from the current level to silence after <see cref="NoteOff"/>.</summary>
    public float Release { get; set; } = 0.2f;

    /// <summary>The current output level, 0 to 1.</summary>
    public float Level { get; private set; }

    /// <summary>The phase the envelope is in.</summary>
    public EnvelopePhase Phase { get; private set; } = EnvelopePhase.Idle;

    /// <summary>True while the envelope is producing sound (any phase other than idle).</summary>
    public bool IsActive => Phase != EnvelopePhase.Idle;

    private float _releaseFrom;

    /// <summary>Starts a note: begins the attack from the current level.</summary>
    public void NoteOn() => Phase = EnvelopePhase.Attack;

    /// <summary>Releases the note: begins the release from the current level.</summary>
    public void NoteOff()
    {
        if (Phase is EnvelopePhase.Idle or EnvelopePhase.Release)
            return;
        _releaseFrom = Level;
        Phase = EnvelopePhase.Release;
    }

    /// <summary>Silences the envelope at once, with no release.</summary>
    public void Reset()
    {
        Phase = EnvelopePhase.Idle;
        Level = 0;
    }

    /// <summary>Advances the envelope by <paramref name="deltaSeconds"/> and returns the new level.</summary>
    public float Process(float deltaSeconds)
    {
        if (deltaSeconds < 0)
            deltaSeconds = 0;
        // A phase with a zero time is instantaneous, so cascade through such phases within this step
        // (a note with no attack and no decay jumps straight to sustain). The guard bounds the cascade.
        for (int guard = 0; guard < 4; guard++)
        {
            switch (Phase)
            {
                case EnvelopePhase.Attack:
                    if (Attack <= 0) { Level = 1f; Phase = EnvelopePhase.Decay; continue; }
                    Level += deltaSeconds / Attack;
                    if (Level >= 1f) { Level = 1f; Phase = EnvelopePhase.Decay; }
                    break;
                case EnvelopePhase.Decay:
                    if (Decay <= 0) { Level = Sustain; Phase = EnvelopePhase.Sustain; continue; }
                    Level -= deltaSeconds * (1f - Sustain) / Decay;
                    if (Level <= Sustain) { Level = Sustain; Phase = EnvelopePhase.Sustain; }
                    break;
                case EnvelopePhase.Sustain:
                    Level = Sustain;
                    break;
                case EnvelopePhase.Release:
                    if (Release <= 0) { Level = 0f; Phase = EnvelopePhase.Idle; continue; }
                    Level -= deltaSeconds * _releaseFrom / Release;
                    if (Level <= 0f) { Level = 0f; Phase = EnvelopePhase.Idle; }
                    break;
            }
            break;
        }
        return Level;
    }
}
