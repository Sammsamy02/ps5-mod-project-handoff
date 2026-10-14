// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK
//
// Which of the modules an application imports from have to travel with it.
//
// The system publishes most modules itself: an application names one and the loader finds it. A small
// set is different - the application is expected to carry its own copy in its sce_module folder, and
// the system publishes nothing for the loader to bind against. An application that names one of these
// and does not ship it is accepted by every structural check, installs cleanly, and then hangs the
// console when it is launched: the loader is resolving the missing module before any of the
// application's own code runs, so there is no fault and nothing is written to the log.
//
// That failure is unrecoverable without a power cycle, so the build refuses to produce a package
// rather than let one reach a console.

using System;
using System.Collections.Generic;
using System.Linq;

namespace SharpProspero.Prx;

/// <summary>
/// The modules an application has to carry in its own <c>sce_module</c> folder rather than rely on the
/// system to publish.
/// </summary>
public static class BundledModules
{
    // Held as file names because that is what an application names in its needed-module list and what
    // the sce_module folder is keyed by.
    private static readonly string[] Names =
    [
        "libc.prx",
        "libSceFace.prx",
        "libSceFaceTracker.prx",
        "libSceJobManager.prx",
        "libSceJobManager_nosubmission.prx",
        "libSceNpCppWebApi.prx",
        "libScePfs.prx",
    ];

    /// <summary>Every module that has to travel with the application that names it.</summary>
    public static IReadOnlyList<string> All => Names;

    /// <summary>Whether <paramref name="moduleFileName"/> has to travel with the application.</summary>
    public static bool IsBundled(string moduleFileName) =>
        !string.IsNullOrEmpty(moduleFileName) &&
        Names.Contains(moduleFileName, StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// The subset of <paramref name="neededModules"/> that has to travel with the application, in the
    /// order given.
    /// </summary>
    /// <param name="neededModules">The modules an application names, as file names.</param>
    public static IReadOnlyList<string> Required(IEnumerable<string> neededModules)
    {
        ArgumentNullException.ThrowIfNull(neededModules);
        var result = new List<string>();
        foreach (string name in neededModules)
            if (IsBundled(name) && !result.Contains(name, StringComparer.OrdinalIgnoreCase))
                result.Add(name);
        return result;
    }
}
