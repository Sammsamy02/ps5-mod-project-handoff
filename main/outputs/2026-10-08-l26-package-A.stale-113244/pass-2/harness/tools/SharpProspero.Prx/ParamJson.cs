// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK
//
// Checking and completing an application's sce_sys/param.json.
//
// The metadata describes the title to the system: what kind of title it is, what it is called, which
// content it is, and what it is allowed to do. Two kinds of mistake are worth separating. A field that
// is absent, holds the wrong type, or carries a value outside the set the system recognises is wrong
// however the title is used, and is reported as a fault. A field that a published title carries and
// this one does not is reported as incomplete instead: the title still describes itself and still
// runs, but the system reads its own default where the title meant to say something.
//
// Both are worth catching before a package is built, because neither shows up as an error message on
// the console - the home screen simply draws the title wrongly, or a service the title expected to
// reach is never offered to it.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace SharpProspero.Prx;

/// <summary>How much a metadata finding matters.</summary>
public enum ParamIssueLevel
{
    /// <summary>The field is absent where every complete title carries one, or disagrees with another field.</summary>
    Incomplete,

    /// <summary>The field is absent, holds the wrong type, or carries a value the system does not recognise.</summary>
    Fault,
}

/// <summary>One finding about an application's metadata.</summary>
/// <param name="Level">How much the finding matters.</param>
/// <param name="Field">The field the finding is about.</param>
/// <param name="Message">What is wrong, written for the person who has to fix it.</param>
/// <param name="CanComplete">Whether <see cref="ParamJson.Complete"/> can settle this one.</param>
public sealed record ParamIssue(ParamIssueLevel Level, string Field, string Message, bool CanComplete);

/// <summary>The result of reading an application's metadata.</summary>
public sealed class ParamReport
{
    internal ParamReport(IReadOnlyList<ParamIssue> issues, ApplicationCategory? category)
    {
        Issues = issues;
        Category = category;
    }

    /// <summary>Everything found, faults first.</summary>
    public IReadOnlyList<ParamIssue> Issues { get; }

    /// <summary>The category the metadata declares, when it declares one the system recognises.</summary>
    public ApplicationCategory? Category { get; }

    /// <summary>The findings that make the metadata wrong rather than merely incomplete.</summary>
    public IEnumerable<ParamIssue> Faults => Issues.Where(i => i.Level == ParamIssueLevel.Fault);

    /// <summary>Whether the metadata describes the title correctly, whatever else is missing from it.</summary>
    public bool IsValid => !Faults.Any();
}

/// <summary>Reading, checking and completing an application's <c>sce_sys/param.json</c>.</summary>
public static class ParamJson
{
    /// <summary>The rights model a title declares in <c>applicationDrmType</c>.</summary>
    public static IReadOnlyList<string> DrmTypes { get; } =
        ["standard", "free", "upgradable", "demo", "freemium"];

    /// <summary>The ways a title may be started, named in <c>gameIntent.permittedIntents</c>.</summary>
    public static IReadOnlyList<string> IntentTypes { get; } =
    [
        "launchActivity", "launchMultiplayerActivity", "launchByCustomParameters", "joinSession",
        "launchTournamentMatch",
    ];

    // A title shares its content with a fixed number of other titles, each named in a field of fixed
    // width. The table is always present and always this shape, blank where nothing is shared.
    private const int SharingSlots = 7;
    private const int SharingSlotWidth = 19;

    /// <summary>Reads <paramref name="document"/> and reports everything wrong or missing.</summary>
    /// <param name="document">The parsed metadata.</param>
    public static ParamReport Check(JsonObject document)
    {
        ArgumentNullException.ThrowIfNull(document);
        var issues = new List<ParamIssue>();
        ApplicationCategory? category = CheckCategory(document, issues);

        CheckIdentity(document, issues);
        CheckDrmType(document, issues);
        CheckTitleName(document, issues);
        CheckBadge(document, category, issues);
        CheckAgeLevel(document, issues);
        CheckIntents(document, category, issues);
        CheckSharing(document, issues);
        CheckExpectedFields(document, issues);

        issues.Sort((a, b) => a.Level == b.Level
            ? string.CompareOrdinal(a.Field, b.Field)
            : a.Level == ParamIssueLevel.Fault ? -1 : 1);
        return new ParamReport(issues, category);
    }

    /// <summary>
    /// Fills in every field <see cref="Check"/> reported as settleable, leaving anything already present
    /// alone.
    /// </summary>
    /// <param name="document">The metadata to complete, changed in place.</param>
    /// <returns>The fields that were written.</returns>
    public static IReadOnlyList<string> Complete(JsonObject document)
    {
        ArgumentNullException.ThrowIfNull(document);
        var written = new List<string>();

        // A category already there is left as it is, even when it names no kind the system knows: that
        // is a fault to report rather than a gap to fill, and overwriting it would hide the mistake.
        ApplicationCategory? category = ReadCategory(document);
        if (document["applicationCategoryType"] is null)
        {
            category = ApplicationCategories.Default;
            document["applicationCategoryType"] = (int)category;
            written.Add("applicationCategoryType");
        }

        // The badge is the author's to choose. Only an absent one is settled here, and only to a
        // sensible starting point; one already chosen is left alone even where it does not follow from
        // the category, because it does not have to. Rewriting it replaced a deliberate choice with a
        // guess, including replacing the one that means no badge at all.
        if (ReadInt(document, "contentBadgeType") is not int existing || existing is < 0 or > 2)
        {
            document["contentBadgeType"] = ApplicationCategories.DefaultBadge(category);
            written.Add("contentBadgeType");
        }

        // Only the missing part is filled in. Replacing the whole block threw away every regional
        // rating an author had set, which is the opposite of settling what is absent.
        if (document["ageLevel"] is not JsonObject age)
        {
            document["ageLevel"] = new JsonObject { ["default"] = 0 };
            written.Add("ageLevel");
        }
        else if (age["default"] is null)
        {
            age["default"] = 0;
            written.Add("ageLevel");
        }

        if (category is ApplicationCategory.Game && !HasIntents(document))
        {
            document["gameIntent"] = new JsonObject
            {
                ["permittedIntents"] = new JsonArray(new JsonObject { ["intentType"] = "launchActivity" }),
            };
            written.Add("gameIntent");
        }

        if (!HasSharingTable(document))
        {
            var slots = new JsonArray();
            for (int i = 0; i < SharingSlots; i++)
                slots.Add(new string(' ', SharingSlotWidth));
            document["addcont"] = new JsonObject { ["serviceIdForSharing"] = slots };
            written.Add("addcont");
        }

        foreach ((string field, JsonNode fallback) in DefaultsForExpectedFields())
            if (document[field] is null)
            {
                document[field] = fallback;
                written.Add(field);
            }

        if (written.Count > 0)
            SortFields(document);
        return written;
    }

    // The metadata is written with its fields in order, so a field added here lands where a reader
    // expects to find it rather than at the end.
    private static void SortFields(JsonObject document)
    {
        var fields = new List<KeyValuePair<string, JsonNode?>>(document);
        fields.Sort((a, b) => string.CompareOrdinal(a.Key, b.Key));

        // Removing a field detaches its value, which a value has to be before it can be added back.
        foreach (KeyValuePair<string, JsonNode?> field in fields)
            document.Remove(field.Key);
        foreach (KeyValuePair<string, JsonNode?> field in fields)
            document.Add(field.Key, field.Value);
    }

    private static ApplicationCategory? CheckCategory(JsonObject document, List<ParamIssue> issues)
    {
        JsonNode? node = document["applicationCategoryType"];
        if (node is null)
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "applicationCategoryType",
                "Absent. Every title states what kind of title it is; " +
                $"an application that ships its own module uses {(int)ApplicationCategories.Default}.", true));
            return null;
        }

        if (!TryReadNumber(node, out long number))
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "applicationCategoryType",
                "Not a number. The category is written as a bare integer, not a string.", false));
            return null;
        }

        if (!ApplicationCategories.IsKnown(number))
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "applicationCategoryType",
                $"{number} is not a kind of title the system recognises. " +
                $"Known values: {string.Join(", ", ApplicationCategories.All.Select(c => $"{(int)c} ({c})"))}.", false));
            return null;
        }

        return (ApplicationCategory)number;
    }

    private static void CheckIdentity(JsonObject document, List<ParamIssue> issues)
    {
        string? titleId = ReadString(document, "titleId");
        if (titleId is null || !IsTitleId(titleId))
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "titleId",
                titleId is null
                    ? "Absent. A title is identified by a nine-character id such as PPSA99099."
                    : $"'{titleId}' is not a title id. Four letters then five digits, for example PPSA99099.", false));

        string? contentId = ReadString(document, "contentId");
        if (contentId is null || contentId.Length != 36)
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "contentId",
                contentId is null
                    ? "Absent. The package is keyed by its content id."
                    : $"'{contentId}' is {contentId.Length} characters; a content id is 36.", false));
        else if (titleId is not null && IsTitleId(titleId) && !contentId.Contains($"-{titleId}_", StringComparison.Ordinal))
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "contentId",
                $"'{contentId}' does not carry the title id '{titleId}'. The two have to agree.", false));

        CheckVersionField(document, "contentVersion", ContentVersionWidths, issues);
        CheckVersionField(document, "masterVersion", MasterVersionWidths, issues);
        // Two more carry a content version and were never checked at all.
        if (document["originContentVersion"] is not null)
            CheckVersionField(document, "originContentVersion", ContentVersionWidths, issues);
        if (document["targetContentVersion"] is not null)
            CheckVersionField(document, "targetContentVersion", ContentVersionWidths, issues);
    }

    // Each group of a version is a fixed width, not merely some digits. A reader takes each group from
    // a fixed position and stops at the end of it, so a group written narrower than its width shifts
    // everything after it and the version reads as a different one - lower, usually - with nothing
    // reporting a problem. Counting the groups alone accepted exactly that.
    private static readonly int[] ContentVersionWidths = [2, 3, 3];
    private static readonly int[] MasterVersionWidths = [2, 2];

    private static void CheckVersionField(JsonObject document, string field, int[] widths, List<ParamIssue> issues)
    {
        string? text = ReadString(document, field);
        if (text is null)
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, field, "Absent.", false));
            return;
        }

        string shape = string.Join(".", widths.Select(w => new string('N', w)));
        string[] groups = text.Split('.');
        if (groups.Length != widths.Length
            || groups.Where((g, i) => g.Length != widths[i] || !g.All(char.IsAsciiDigit)).Any())
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, field,
                $"'{text}' is not a version. Use {shape}.", false));
    }

    private static void CheckDrmType(JsonObject document, List<ParamIssue> issues)
    {
        string? drm = ReadString(document, "applicationDrmType");
        if (drm is null)
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "applicationDrmType",
                $"Absent. One of {string.Join(", ", DrmTypes)}.", false));
        else if (!DrmTypes.Contains(drm, StringComparer.Ordinal))
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "applicationDrmType",
                $"'{drm}' is not a rights model. One of {string.Join(", ", DrmTypes)}.", false));
    }

    private static void CheckTitleName(JsonObject document, List<ParamIssue> issues)
    {
        if (document["localizedParameters"] is not JsonObject localized)
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "localizedParameters",
                "Absent. The name shown on the home screen lives here.", false));
            return;
        }

        string? language = ReadString(localized, "defaultLanguage");
        if (language is null)
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "localizedParameters.defaultLanguage",
                "Absent. It names the language entry to fall back on.", false));
            return;
        }

        if (localized[language] is not JsonObject entry || string.IsNullOrWhiteSpace(ReadString(entry, "titleName")))
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, $"localizedParameters.{language}",
                $"No titleName for the default language '{language}'. " +
                "The home screen has no name to draw without it.", false));
    }

    private static void CheckBadge(JsonObject document, ApplicationCategory? category, List<ParamIssue> issues)
    {
        int? badge = ReadInt(document, "contentBadgeType");
        if (badge is null)
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Incomplete, "contentBadgeType",
                "Absent. It settles the badge drawn on the icon.", true));
            return;
        }

        if (badge is < 0 or > 2)
            issues.Add(new ParamIssue(ParamIssueLevel.Fault, "contentBadgeType",
                $"{badge} is not a badge. It is none, a game, or an application.", false));
    }

    private static void CheckAgeLevel(JsonObject document, List<ParamIssue> issues)
    {
        if (document["ageLevel"] is not JsonObject age)
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Incomplete, "ageLevel",
                "Absent. It carries the age rating, per region, with a default to fall back on.", true));
            return;
        }

        if (age["default"] is null)
            issues.Add(new ParamIssue(ParamIssueLevel.Incomplete, "ageLevel.default",
                "Absent. A region with no entry of its own falls back on it.", true));
    }

    private static void CheckIntents(JsonObject document, ApplicationCategory? category, List<ParamIssue> issues)
    {
        if (category is not null && category != ApplicationCategory.Game)
            return;

        if (document["gameIntent"] is not JsonObject intent)
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Incomplete, "gameIntent",
                "Absent. It names the ways the title may be started.", true));
            return;
        }

        if (intent["permittedIntents"] is not JsonArray permitted || permitted.Count == 0)
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Incomplete, "gameIntent.permittedIntents",
                "Empty. A title names at least the way it is started from the home screen.", true));
            return;
        }

        foreach (JsonNode? item in permitted)
        {
            string? type = item is JsonObject o ? ReadString(o, "intentType") : null;
            if (type is null || !IntentTypes.Contains(type, StringComparer.Ordinal))
                issues.Add(new ParamIssue(ParamIssueLevel.Fault, "gameIntent.permittedIntents",
                    $"'{type ?? "(none)"}' is not a way a title can be started. " +
                    $"One of {string.Join(", ", IntentTypes)}.", false));
        }
    }

    private static void CheckSharing(JsonObject document, List<ParamIssue> issues)
    {
        if (document["addcont"] is not JsonObject addcont)
        {
            issues.Add(new ParamIssue(ParamIssueLevel.Incomplete, "addcont",
                "Absent. It carries the table of titles this one shares content with, blank when it shares none.", true));
            return;
        }

        if (addcont["serviceIdForSharing"] is not JsonArray slots || slots.Count != SharingSlots)
            issues.Add(new ParamIssue(ParamIssueLevel.Incomplete, "addcont.serviceIdForSharing",
                $"The sharing table holds {SharingSlots} entries of {SharingSlotWidth} characters, blank where nothing is shared.", true));
    }

    // Fields a complete title always carries. None of them changes what the title is, so an absence is
    // reported as incomplete rather than a fault - but the system reads its own default for each, which
    // is rarely what the title meant.
    private static void CheckExpectedFields(JsonObject document, List<ParamIssue> issues)
    {
        foreach ((string field, JsonNode _) in DefaultsForExpectedFields())
            if (document[field] is null)
                issues.Add(new ParamIssue(ParamIssueLevel.Incomplete, field, "Absent.", true));

        foreach (string field in (string[])["requiredSystemSoftwareVersion", "sdkVersion"])
        {
            string? text = ReadString(document, field);
            if (text is not null && !IsPackedVersion(text))
                issues.Add(new ParamIssue(ParamIssueLevel.Fault, field,
                    $"'{text}' is not a packed version. Sixteen hex digits after 0x, for example 0x1000000000000000.", false));
        }
    }

    private static IEnumerable<(string Field, JsonNode Value)> DefaultsForExpectedFields()
    {
        yield return ("attribute", JsonValue.Create(0));
        yield return ("attribute2", JsonValue.Create(0));
        yield return ("attribute3", JsonValue.Create(0));
        yield return ("downloadDataSize", JsonValue.Create(0));
        yield return ("versionFileUri", JsonValue.Create(""));
    }

    private static bool HasIntents(JsonObject document) =>
        document["gameIntent"] is JsonObject intent &&
        intent["permittedIntents"] is JsonArray permitted && permitted.Count > 0;

    private static bool HasSharingTable(JsonObject document) =>
        document["addcont"] is JsonObject addcont &&
        addcont["serviceIdForSharing"] is JsonArray slots && slots.Count == SharingSlots;

    private static ApplicationCategory? ReadCategory(JsonObject document) =>
        TryReadNumber(document["applicationCategoryType"], out long number) && ApplicationCategories.IsKnown(number)
            ? (ApplicationCategory)number
            : null;

    // A number reaches this either parsed from the file, where it is held as raw text, or set here,
    // where it is held as whichever type the caller used. Both have to read the same, so each is tried
    // in turn rather than assuming the one the parser produces.
    private static bool TryReadNumber(JsonNode? node, out long value)
    {
        value = 0;
        if (node is not JsonValue number)
            return false;
        if (number.TryGetValue(out long asLong))
        {
            value = asLong;
            return true;
        }
        if (number.TryGetValue(out int asInt))
        {
            value = asInt;
            return true;
        }
        if (number.TryGetValue(out double asDouble) && asDouble == Math.Floor(asDouble) &&
            asDouble is >= long.MinValue and <= long.MaxValue)
        {
            value = (long)asDouble;
            return true;
        }
        return false;
    }

    private static bool IsTitleId(string value) =>
        value.Length == 9 && value[..4].All(char.IsAsciiLetterUpper) && value[4..].All(char.IsAsciiDigit);

    private static bool IsPackedVersion(string value) =>
        value.Length == 18 && value.StartsWith("0x", StringComparison.Ordinal) &&
        value[2..].All(char.IsAsciiHexDigit);

    private static string? ReadString(JsonObject document, string field) =>
        document[field] is JsonValue value && value.TryGetValue(out string? text) ? text : null;

    private static int? ReadInt(JsonObject document, string field) =>
        TryReadNumber(document[field], out long number) && number is >= int.MinValue and <= int.MaxValue
            ? (int)number
            : null;
}
