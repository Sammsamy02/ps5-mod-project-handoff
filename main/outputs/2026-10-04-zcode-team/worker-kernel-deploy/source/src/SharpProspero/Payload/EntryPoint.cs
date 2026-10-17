// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Payload;

/// <summary>
/// Retrieves the <see cref="PayloadArgs"/> block the loader passed to the payload's entry point.
/// The start object saves the block pointer before calling constructors or <c>main</c>, and
/// this class reads it back through a getter the start object defines.
/// </summary>
public static unsafe class PayloadEntryPoint
{
    /// <summary>Returns the payload arguments the loader handed to this payload at start-up.</summary>
    public static PayloadArgs* Args => PayloadCrt.GetPayloadArgs();
}
