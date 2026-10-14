---
title: System information
parent: System services
grand_parent: Application Modules
nav_order: 1
---

# System information

`SystemInfo`, `SystemParameters` and `Users` read what the console is and who is signed in; `SystemControl` and `SystemSettings` act on the running system; `Sysctl` and `SystemTelemetry` read the values the kernel publishes, including how hot the machine is. All of these live in `SharpProspero.Platform`, and a system or diagnostics utility usually touches every one of them.

<details open markdown="block">
  <summary>On this page</summary>
  {: .text-delta }
- TOC
{:toc}
</details>

## Console facts

`SystemInfo` reports facts about the machine the module runs on. A diagnostics or settings tool shows the system software version the way the console displays it:

```csharp
using SharpProspero.Platform;

string firmware = SystemInfo.SystemSoftwareVersion;   // for example "11.020.000"
```

`SystemSoftwareVersionValue` returns the same version packed into a word — major byte then minor byte, as it reads — which is the form a package's requirement is compared against. `ConsoleId` returns the console's open identifier as a 32-character hex string, and `ProcessorCount` returns the number of cores available to the application.

```csharp
uint versionValue = SystemInfo.SystemSoftwareVersionValue;  // 11.20 sits as 0x1120 in the high half
string id = SystemInfo.ConsoleId;                           // stable per-console value
int cores = SystemInfo.ProcessorCount;
```

{: .note }
> Any of these can throw `ProsperoException` if the underlying value cannot be read. For matching a build against a firmware window, see [Firmware compatibility](firmware.md).

## Language, date and time

`SystemParameters` reads the user's console settings so a title can match its own presentation to them.

```csharp
if (SystemParameters.Language == SystemLanguage.French)
    LoadStrings("fr");

int minutesFromUtc = SystemParameters.TimeZoneMinutes;
bool dst = SystemParameters.IsSummerTime;
```

`Language` is a `SystemLanguage` (the full set of console languages, from `Japanese` and `EnglishUS` through `ChineseSimplified`, `PortugueseBrazil` and the rest). `DateFormat` describes the order date fields are shown in, and `TimeFormat` whether the clock is 12- or 24-hour:

```csharp
string pattern = SystemParameters.DateFormat switch
{
    DateFormat.YearMonthDay => "yyyy/MM/dd",
    DateFormat.DayMonthYear => "dd/MM/yyyy",
    DateFormat.MonthDayYear => "MM/dd/yyyy",
    _ => "yyyy/MM/dd",
};

bool use24Hour = SystemParameters.TimeFormat == TimeFormat.TwentyFourHour;
```

`TimeZoneMinutes` is the offset from UTC in minutes, `IsSummerTime` reports daylight saving, and `SystemName` is the name the user gave the console — useful for showing whose system a title is running on.

## Signed-in users

`Users` lists the signed-in profiles, so an application can greet the player by name or offer a choice of accounts. A `UserProfile` pairs the numeric id the system tracks a user by with the display name.

```csharp
int me = Users.InitialUserId;                 // the user who started this application
string myName = Users.InitialUserName;

foreach (UserProfile profile in Users.LoggedInUsers)
    DrawProfileRow(profile.Id, profile.Name);

string name = Users.GetUserName(otherId);
```

`InitialUserId` is the id you pass to the controller, save data and dialog services to act on behalf of the launching user. `LoggedInUserIds` is the raw id list for the console's slots, and `LoggedInUsers` pairs each id with its name in one call.

{: .note }
> The user service must be running first. `ProsperoApp` starts it at startup, so a normal application need do nothing; a module that runs without the app host initializes the user service once before reading these.

## Keeping the console awake and reacting to events

`SystemControl` is the app-loop plumbing a real application and a system tool both need: hold off the idle shutdown, read system events such as resuming from sleep, learn whether the application is backgrounded, and take the audio output for the module alone.

During a long operation with no controller activity — a download or an install — call `KeepAwake` periodically so the console does not shut down on its idle timer:

```csharp
while (installing)
{
    SystemControl.KeepAwake();
    // do a slice of work
}
```

Poll `TryReceiveEvent` each frame to react to system events. It returns `false` once the queue is empty, so a `while` loop drains a burst in one pass. `SystemEventType` names `Resume` (the application came back from a suspended state) and `AppLaunched` (another application was launched over this one); an unrecognized event keeps its raw number so nothing is silently dropped. After a `Resume`, the clock and inputs may have moved on, so anything time-based should resynchronize:

```csharp
while (SystemControl.TryReceiveEvent(out SystemEventType type))
{
    if (type == SystemEventType.Resume)
        ResyncClock();
}
```

`GetStatus` returns a `SystemStatus` snapshot — the number of `PendingEvents`, whether a system dialog is drawn over the application (`IsSystemUiOverlaid`), and whether the module is `IsInBackground`. The `IsInBackground` property is a shortcut for the same flag.

```csharp
SystemStatus status = SystemControl.GetStatus();
if (status.IsSystemUiOverlaid)
    PauseInput();
```

The remaining members cover the rest of the loop:

| Member | What it does |
|---|---|
| `DisplaySafeAreaRatio` | Fraction of the screen (0 to 1) safe for important content; multiply screen dimensions by it and centre. |
| `SilenceBackgroundMedia` / `RestoreBackgroundMedia` | Stop and later restore the background media player so the module owns the audio. |
| `LoadExecutable(path)` | Replace the running module with another executable for chain-loading; on success it does not return. |

## Stored system settings

`SystemSettings` reads and writes the values the system itself keeps, which no other service exposes. Each entry is addressed by a numeric identifier the system defines, so a tool supplies the identifier it is interested in; the class does not name them, because an identifier's meaning belongs to the system version in use.

```csharp
using SharpProspero.Platform;
using SharpProspero.Diagnostics;

if (SystemSettings.TryOpen(out SystemSettings? settings))
{
    using (settings)
    {
        if (settings!.TryGetInt32(id, out int value))
            Log.Information($"setting {id} = {value}");

        settings.SetInt32(id, value + 1);
        string text = settings.GetString(otherId);
    }
}
```

| Call | What it does |
|---|---|
| `TryOpen(out settings)` | Load the service, reporting whether it could be reached. |
| `Open()` | Load it, raising when it could not be reached. |
| `GetInt32(id)` / `TryGetInt32(id, out value)` | Read a whole-number setting. |
| `SetInt32(id, value)` | Write a whole-number setting. |
| `GetString(id, maxLength)` / `TryGetString(id, out value, maxLength)` | Read a text setting. |
| `SetString(id, value)` | Write a text setting. |

The service is loaded at run time and unloaded when you dispose the object, so open it once, use it, and let the `using` block close it.

{: .important }
> Reaching this service depends on what the running build is permitted to do. `TryOpen` and the `Try` forms report a refusal rather than throwing, so a tool can offer the feature only where it works and carry on where it does not. `Log` lives in `SharpProspero.Diagnostics` — see [Diagnostics](diagnostics.md).

## Named system values

The kernel publishes a tree of values under dotted names — `hw.ncpu`, `kern.ostype`, `hw.acpi.thermal.tz0.temperature` and many more. `Sysctl` reads them. Every value is a block of bytes with a name and a size, so there is a reader for each shape that block takes: a fixed-width integer, a NUL-terminated string, and an opaque run of bytes.

```csharp
using SharpProspero.Platform;
using SharpProspero.Diagnostics;

int cores = Sysctl.ReadInt32("hw.ncpu");

if (Sysctl.TryReadString("kern.ostype", out string os))
    Log.Information(os);

byte[] block = Sysctl.ReadRaw("hw.acpi.thermal.tz0._ACx");
```

| Call | What it does |
|---|---|
| `Exists(name)` | Report whether the machine publishes anything under that name. |
| `GetSize(name)` / `TryGetSize(name, out size)` | Ask how many bytes the value takes. |
| `ReadInt32` / `ReadUInt32` / `ReadInt64` / `ReadUInt64` | Read a fixed-width number, each with a `Try` form. |
| `ReadString(name)` / `TryReadString(name, out value)` | Read text, without the terminating NUL. |
| `ReadRaw(name)` / `TryReadRaw(name, out value)` | Read the whole block, sized from the system. |
| `TryReadRaw(name, destination, out written)` | Read into a buffer you already have. |
| `LastErrorNumber` | Why the last call failed, read straight afterwards. |

Which names exist depends on what the running kernel configured, so a name that answers on one machine can be missing on another. A `Try` form returning false means "this machine does not publish that"; `LastErrorNumber` tells an absent name (`Sysctl.NotPresentError`) from a refused one (`Sysctl.NotPermittedError`).

## Temperature and cooling

`SystemTelemetry` reads the platform's thermal zones through those named values. Ask which zones the machine has first — an empty list means it declares none, and nothing else will answer.

```csharp
using SharpProspero.Platform;
using SharpProspero.Diagnostics;

foreach (int zone in SystemTelemetry.EnumerateThermalZones())
{
    if (!SystemTelemetry.TryReadZone(zone, out ThermalZoneReading reading))
        continue;

    Log.Information($"zone {zone}: {reading.Temperature}");   // for example "48.9 C"

    if (reading.Alarms.HasFlag(ThermalAlarms.PassiveThresholdReached))
        Log.Warning("the system is slowing the processor down to cool off");
}
```

`Temperature` holds the reading the way the platform reports it — a whole number of tenths of a kelvin, in `DeciKelvin` — and converts to `Celsius`, `Kelvin` and `Fahrenheit` from there. `ToString()` gives degrees Celsius to one decimal place.

| Reading | What it is |
|---|---|
| `TryReadTemperature(zone, out t)` | The temperature the zone recorded at its last poll. |
| `TryReadAlarms(zone, out alarms)` | Which of the zone's thresholds that poll found crossed. |
| `TryReadActiveCoolingLevel(zone, out level)` | The active-cooling step engaged; negative when none is. |
| `TryReadPassiveCoolingEnabled(zone, out on)` | Whether the zone may cool by slowing the processor down. |
| `TryReadPassiveTripPoint` / `TryReadHotTripPoint` / `TryReadCriticalTripPoint` | The temperature at which the system slows down, suspends, or shuts down. |
| `TryReadActiveTripPoints(zone, out points)` | The temperature at which each active-cooling step engages. |
| `TryReadPollingIntervalSeconds(out seconds)` | How often the driver samples the zones. |
| `TryReadMinimumCoolingRuntimeSeconds(out seconds)` | How long a cooling step stays engaged before it drops back. |

Each of these has a throwing `Read…` twin. A trip point comes back as `Temperature?`, with null meaning the platform firmware defines no such threshold — that is different from the machine refusing to answer, which is what a false return means.

{: .important }
> Whether any thermal zone exists is decided by the platform firmware of the machine the module runs on, and a retail machine may declare none. Write the reading as optional: show it where it answers and leave it out where it does not.
>
> Fan duty, fan speed and electrical measurements are not available. Nothing publishes a named value for them and no library an application links exports one, so an application cannot ask. `TryReadActiveCoolingLevel` is the nearest reachable substitute: it names the cooling step the zone has engaged, not a duty cycle or a speed.

## Related pages

Installing and inspecting titles, and reading the parameters a title was packaged with, are covered in [Packages and devices](packages-devices.md). For the overview of console services and the permission model behind them, see the [System services](system-services.md) landing page.
