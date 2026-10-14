// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.SystemService;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>The system language the user has selected.</summary>
public enum SystemLanguage
{
    Japanese = 0,
    EnglishUS = 1,
    French = 2,
    Spanish = 3,
    German = 4,
    Italian = 5,
    Dutch = 6,
    PortuguesePortugal = 7,
    Russian = 8,
    Korean = 9,
    ChineseTraditional = 10,
    ChineseSimplified = 11,
    Finnish = 12,
    Swedish = 13,
    Danish = 14,
    Norwegian = 15,
    Polish = 16,
    PortugueseBrazil = 17,
    EnglishUK = 18,
    Turkish = 19,
    SpanishLatinAmerica = 20,
    Arabic = 21,
    FrenchCanada = 22,
    Czech = 23,
    Hungarian = 24,
    Greek = 25,
    Romanian = 26,
    Thai = 27,
    Vietnamese = 28,
    Indonesian = 29,
}

/// <summary>The order date fields are shown in.</summary>
public enum DateFormat
{
    YearMonthDay = 0,
    DayMonthYear = 1,
    MonthDayYear = 2,
}

/// <summary>Whether time is shown on a 12- or 24-hour clock.</summary>
public enum TimeFormat
{
    TwelveHour = 0,
    TwentyFourHour = 1,
}

/// <summary>
/// The user's system settings, read from the system service. Use these to match a title's language,
/// date and time presentation to the console.
/// </summary>
public static class SystemParameters
{
    /// <summary>The selected system language.</summary>
    public static SystemLanguage Language => (SystemLanguage)GetInt(SystemService.ParamIdLanguage);

    /// <summary>The date display order.</summary>
    public static DateFormat DateFormat => (DateFormat)GetInt(SystemService.ParamIdDateFormat);

    /// <summary>The 12- or 24-hour time display.</summary>
    public static TimeFormat TimeFormat => (TimeFormat)GetInt(SystemService.ParamIdTimeFormat);

    /// <summary>The time-zone offset from UTC in minutes.</summary>
    public static int TimeZoneMinutes => GetInt(SystemService.ParamIdTimeZone);

    /// <summary>True when summer time is in effect.</summary>
    public static bool IsSummerTime => GetInt(SystemService.ParamIdSummerTime) != 0;

    /// <summary>The name the user gave the console, for showing whose system a title is running on.</summary>
    public static unsafe string SystemName
    {
        get
        {
            const int size = 128;
            byte* buffer = stackalloc byte[size];
            SceResult.ThrowIfFailed(
                SystemService.sceSystemServiceParamGetString(SystemService.ParamIdSystemName, buffer, size),
                nameof(SystemService.sceSystemServiceParamGetString));
            int length = 0;
            while (length < size && buffer[length] != 0)
                length++;
            return Encoding.UTF8.GetString(buffer, length);
        }
    }

    private static unsafe int GetInt(int paramId)
    {
        int value;
        SceResult.ThrowIfFailed(SystemService.sceSystemServiceParamGetInt(paramId, &value),
            nameof(SystemService.sceSystemServiceParamGetInt));
        return value;
    }
}
