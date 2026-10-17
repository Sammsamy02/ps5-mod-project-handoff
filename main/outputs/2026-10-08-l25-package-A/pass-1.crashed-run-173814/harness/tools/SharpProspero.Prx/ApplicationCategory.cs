// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK
//
// The kind of title an application declares in its metadata.
//
// The category tells the system what it is starting, and it selects real handling rather than only a
// label: whether the picture is protected, whether it is auto-scaled, how power saving applies, and
// whether the title is given a media application's shared storage allowance.
//
// The number is read in fields - the high byte is the broad kind and the lower bytes narrow it - so
// the values are not a plain sequence. Only the ones below are recognised; a title carrying anything
// else is not a kind the system knows. The memory a title is given is settled by its own fields, not
// by this one.

using System;
using System.Collections.Generic;
using System.Globalization;

namespace SharpProspero.Prx;

/// <summary>
/// The value of the <c>applicationCategoryType</c> field in an application's <c>sce_sys/param.json</c>.
/// </summary>
public enum ApplicationCategory
{
    /// <summary>A title that ships and runs its own module. Value 0.</summary>
    Game = 0,

    /// <summary>A media application that ships and runs its own module. Value 65536.</summary>
    MediaApp = 65536,

    /// <summary>A media application driven by the streaming service framework. Value 65792.</summary>
    RnpsMediaApp = 65792,

    /// <summary>A media application whose front end is a web document. Value 66048.</summary>
    WebMediaApp = 66048,

    /// <summary>An application built into the system software. Value 131328.</summary>
    SystemBuiltInApp = 131328,

    /// <summary>A background service that runs with an application's resources. Value 131584.</summary>
    BigDaemon = 131584,

    /// <summary>The home screen and the menus drawn over a running title. Value 16777216.</summary>
    ShellUi = 16777216,

    /// <summary>A background service. Value 33554432.</summary>
    Daemon = 33554432,

    /// <summary>A dialog the system draws on a title's behalf. Value 50331648.</summary>
    CommonDialog = 50331648,

    /// <summary>An application that runs as part of the home screen. Value 67108864.</summary>
    ShellApp = 67108864,
}

/// <summary>Reading and naming <see cref="ApplicationCategory"/> values.</summary>
public static class ApplicationCategories
{
    // Ordered by value so a listing reads the way the field's own numbering runs.
    private static readonly ApplicationCategory[] Known =
    [
        ApplicationCategory.Game,
        ApplicationCategory.MediaApp,
        ApplicationCategory.RnpsMediaApp,
        ApplicationCategory.WebMediaApp,
        ApplicationCategory.SystemBuiltInApp,
        ApplicationCategory.BigDaemon,
        ApplicationCategory.ShellUi,
        ApplicationCategory.Daemon,
        ApplicationCategory.CommonDialog,
        ApplicationCategory.ShellApp,
    ];

    /// <summary>Every category the system recognises, lowest value first.</summary>
    public static IReadOnlyList<ApplicationCategory> All => Known;

    /// <summary>The category an application carrying its own module declares.</summary>
    /// <remarks>
    /// Every title that ships a module of its own uses this, whatever the module goes on to do. The
    /// media categories belong to applications the system software itself publishes, and the handling
    /// they select assumes the service frameworks those applications are built around. Declaring one
    /// does not gain a title the services, only the handling that expects them.
    /// </remarks>
    public static ApplicationCategory Default => ApplicationCategory.Game;

    /// <summary>Whether <paramref name="value"/> is a category the system recognises.</summary>
    public static bool IsKnown(long value) =>
        value is >= int.MinValue and <= int.MaxValue && Array.IndexOf(Known, (ApplicationCategory)value) >= 0;

    /// <summary>
    /// The badge a title of this category draws on its icon: the value its <c>contentBadgeType</c> field
    /// carries.
    /// </summary>
    /// <remarks>
    /// This is only the starting point for a title that names none. The badge does not follow from the
    /// category and is the author's to choose: a title of any category may carry any of the three, and
    /// deriving one from the other overwrote a deliberate choice, including the one meaning no badge.
    /// </remarks>
    public static int DefaultBadge(ApplicationCategory? category) =>
        category == ApplicationCategory.Game ? 1 : 2;

    /// <summary>
    /// Reads a category from <paramref name="text"/>, which may name one or give its number.
    /// </summary>
    /// <param name="text">A category name such as <c>Game</c>, or a number such as <c>65536</c>.</param>
    /// <param name="category">The category read, when this returns <see langword="true"/>.</param>
    public static bool TryParse(string? text, out ApplicationCategory category)
    {
        category = default;
        if (string.IsNullOrWhiteSpace(text))
            return false;

        string trimmed = text.Trim();
        if (long.TryParse(trimmed, NumberStyles.Integer, CultureInfo.InvariantCulture, out long number))
        {
            if (!IsKnown(number))
                return false;
            category = (ApplicationCategory)number;
            return true;
        }

        // Names are matched without regard to case so a value typed at a command line reads naturally.
        foreach (ApplicationCategory known in Known)
            if (string.Equals(known.ToString(), trimmed, StringComparison.OrdinalIgnoreCase))
            {
                category = known;
                return true;
            }
        return false;
    }

    /// <summary>The name and number of <paramref name="value"/>, or just the number when unrecognised.</summary>
    public static string Describe(long value) =>
        IsKnown(value)
            ? $"{(ApplicationCategory)value} ({value})"
            : value.ToString(CultureInfo.InvariantCulture);
}
