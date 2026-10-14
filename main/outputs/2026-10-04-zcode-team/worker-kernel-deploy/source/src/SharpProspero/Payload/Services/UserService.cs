// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.UserService;

namespace SharpProspero.Payload.Services;

/// <summary>
/// Initialises and tears down the user service in a payload context. The user service must
/// be initialised as a prerequisite before launching the web browser, because the system
/// service requires an active user-service session to identify which user's context to
/// launch into.
/// </summary>
/// <remarks>
/// Unlike the application-module user service wrapper in <c>Platform</c>, this class makes no
/// assumptions about a pre-initialised session or launcher-owned context. It calls the SPRX
/// functions directly through the interop bindings. Requires <c>libSceUserService</c> in the
/// payload's DT_NEEDED list.
/// </remarks>
public static unsafe class PayloadUserService
{
    /// <summary>
    /// Starts the user service with default parameters.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int Initialize()
    {
        return UserService.sceUserServiceInitialize(null);
    }

    /// <summary>
    /// Stops the user service. Call this in the payload's cleanup path.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int Terminate()
    {
        return UserService.sceUserServiceTerminate();
    }

    /// <summary>
    /// Reads the initial (first signed-in) user id. The user id is needed by system services
    /// that operate in the context of a specific user.
    /// </summary>
    /// <param name="userId">On success, the initial user id.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int GetInitialUser(out int userId)
    {
        int uid;
        int result = UserService.sceUserServiceGetInitialUser(&uid);
        userId = result == 0 ? uid : 0;
        return result;
    }
}
