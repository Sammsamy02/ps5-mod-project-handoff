| # | test | result | detail | time | log line |
|---|---|---|---|---|---|
| 1 | print | PASS | logged | <=16 ms | 2901 |
| 2 | LoadExport | PASS | missing-name-err(expected) | <=16 ms | 2903 |
| 3 | Key.F12 | PASS | F12=123 | <=16 ms | 2905 |
| 4 | ModifierKey.SHIFT | PASS | v=16 | <=16 ms | 2907 |
| 5 | EObjectFlags.RF_Public | PASS | v=1 | <=16 ms | 2909 |
| 6 | EInternalObjectFlags.PendingKill | PASS | v=536870912 | <=16 ms | 2911 |
| 7 | EFindName.FNAME_Find | PASS | v=0 | <=16 ms | 2913 |
| 8 | PropertyTypes.ObjectProperty | PASS | table Size=8 | <=16 ms | 2915 |
| 9 | UE4SS.GetVersion | PASS | 3.0.1 | <=16 ms | 2916 |
| 10 | UnrealVersion.GetMajor | PASS | 5 | <=16 ms | 2917 |
| 11 | UnrealVersion.GetMinor | PASS | 6 | <=16 ms | 2918 |
| 12 | UnrealVersion.IsEqual | PASS | true | <=16 ms | 2919 |
| 13 | UnrealVersion.IsAtLeast | PASS | true | <=16 ms | 2920 |
| 14 | UnrealVersion.IsAtMost | PASS | true | <=16 ms | 2921 |
| 15 | UnrealVersion.IsBelow | PASS | false | <=16 ms | 2922 |
| 16 | UnrealVersion.IsAbove | PASS | true | <=16 ms | 2923 |
| 17 | EGameThreadMethod | PASS | fm=2 | <=16 ms | 2924 |
| 18 | EngineTickAvailable | PASS | true | <=16 ms | 2925 |
| 19 | ProcessEventAvailable | PASS | true | <=16 ms | 2926 |
| 20 | __OriginalReturnValue | PASS | nil-at-rest(upstream-parity) | <=16 ms | 2927 |
| 21 | MainThread | PASS | thread | <=16 ms | 2928 |
| 22 | AsyncThread | PASS | thread | <=16 ms | 2929 |
| 23 | ue4ss_loaded_modules | PASS | n=0 | <=16 ms | 2930 |
| 24 | NAME_None | PASS | None | <=16 ms | 2931 |
| 25 | FName-ctor | PASS | made | <=16 ms | 2932 |
| 26 | FName.ToString | PASS | st2name | <=16 ms | 2933 |
| 27 | FName-tostring | PASS | st2name | <=16 ms | 2934 |
| 28 | FName.Equals | PASS | callable; return-value-undefined(upstrea | <=16 ms | 2935 |
| 29 | FName-eq | PASS | eq=true | <=16 ms | 2936 |
| 30 | FName.GetComparisonIndex | PASS | 5407291 | <=16 ms | 2937 |
| 31 | FName.type | PASS | FName | <=16 ms | 2938 |
| 32 | FName-ctor-FNAME_Find | PASS | find-ctor | <=16 ms | 2939 |
| 33 | FText-ctor | PASS | made | <=16 ms | 2940 |
| 34 | FText.ToString | PASS | st2text | <=16 ms | 2942 |
| 35 | FText-tostring | PASS | st2text | <=16 ms | 2944 |
| 36 | FText.type | PASS | FText | <=16 ms | 2945 |
| 37 | FText-eq | PASS | eq=true | <=16 ms | 2946 |
| 38 | FString-ctor | PASS | made | <=16 ms | 2947 |
| 39 | FString.Len | PASS | len=6 | <=16 ms | 2948 |
| 40 | FString.IsEmpty | PASS | false | <=16 ms | 2949 |
| 41 | FString.ToString | PASS | st2str | <=16 ms | 2950 |
| 42 | FString-tostring | PASS | st2str | <=16 ms | 2951 |
| 43 | FString.type | PASS | FString | <=16 ms | 2952 |
| 44 | FString.Append | PASS | len=7 | <=16 ms | 2953 |
| 45 | FString.EndsWith | PASS | true | <=16 ms | 2954 |
| 46 | FString.StartsWith | PASS | true | <=16 ms | 2955 |
| 47 | FString.Find | PASS | r=3 | <=16 ms | 2956 |
| 48 | FString.ToLower | PASS | r=userdata:st2strx | <=16 ms | 2957 |
| 49 | FString.ToUpper | PASS | r=userdata:ST2STRX | <=16 ms | 2958 |
| 50 | FString.Clear | PASS | empty=true | <=16 ms | 2959 |
| 51 | FString.Empty | PASS | empty=true | <=16 ms | 2960 |
| 52 | FUtf8String-ctor | PASS | made | <=16 ms | 2961 |
| 53 | FUtf8String.Len | PASS | len=5 | <=16 ms | 2962 |
| 54 | FUtf8String.ToString | PASS | st2u8 | <=16 ms | 2963 |
| 55 | FAnsiString-ctor | PASS | made | <=16 ms | 2964 |
| 56 | FAnsiString.Len | PASS | len=5 | <=16 ms | 2965 |
| 57 | FAnsiString.ToString | PASS | st2an | <=16 ms | 2966 |
| 58 | GetCurrentThreadId | PASS | userdata tostring-ok | <=16 ms | 2967 |
| 59 | GetMainModThreadId | PASS | userdata tostring-ok | <=16 ms | 2968 |
| 60 | GetAsyncThreadId | PASS | userdata tostring-ok | <=16 ms | 2969 |
| 61 | GetGameThreadId | PASS | userdata tostring-ok | <=16 ms | 2970 |
| 62 | IsInMainModThread | PASS | false | <=16 ms | 2971 |
| 63 | IsInAsyncThread | PASS | false | <=16 ms | 2972 |
| 64 | IsInGameThread | PASS | true | <=16 ms | 2973 |
| 65 | os.clock | PASS | t=192.359375 | <=16 ms | 2974 |
| 66 | os.date | PASS | ok | <=16 ms | 2975 |
| 67 | debug.getinfo | PASS | src | <=16 ms | 2976 |
| 68 | FindFirstOf | PASS + SLOW | valid=true | **39 ms (SLOW)** | 2977 |
| 69 | FindAllOf | PASS + SLOW | n=1 | **39 ms (SLOW)** | 2978 |
| 70 | StaticFindObject-long | PASS | valid=true | <=16 ms | 2979 |
| 71 | StaticFindObject-short | PASS + SLOW | valid=false | **55 ms (SLOW)** | 2981 |
| 72 | CreateInvalidObject | PASS | invalid-ok | <=16 ms | 2982 |
| 73 | uobj.IsValid | PASS + SLOW | valid | **39 ms (SLOW)** | 2983 |
| 74 | uobj.GetAddress | PASS | 0x18000d2a80 | <=16 ms | 2984 |
| 75 | uobj.GetFullName | PASS | AITestBedPlayerControlle | <=16 ms | 2985 |
| 76 | uobj.GetFName | PASS | AITestBedPlayerControlle | <=16 ms | 2986 |
| 77 | uobj.GetClass | PASS | valid | <=16 ms | 2987 |
| 78 | uobj.GetOuter | PASS | valid=true | <=16 ms | 2988 |
| 79 | uobj.GetWorld | PASS | valid=true | <=16 ms | 2989 |
| 80 | uobj.IsA-class | PASS | true | <=16 ms | 2990 |
| 81 | uobj.IsA-string | PASS | true | <=16 ms | 2991 |
| 82 | uobj.IsAnyClass | PASS | pc=false cls=true | <=16 ms | 2992 |
| 83 | uobj.IsClass | PASS | pc=false cls=true | <=16 ms | 2993 |
| 84 | uobj.HasAllFlags | PASS | r=false | <=16 ms | 2994 |
| 85 | uobj.HasAnyFlags | PASS | r=false | <=16 ms | 2995 |
| 86 | uobj.HasAnyInternalFlags | PASS | false | <=16 ms | 2996 |
| 87 | uobj.GetName-reflected | PASS | AITestBedPlayerControlle | <=16 ms | 2997 |
| 88 | uobj.K2_GetActorLocation | PASS | type=table X=-23759.404296875 | <=16 ms | 2998 |
| 89 | uobj.CallFunction | FAIL | in function <ue4ss/Mods/ueselftest/Scrip | <=16 ms | 2999 |
| 90 | uobj.ProcessConsoleExec | FAIL | Virtual UObject::ProcessConsoleExec is u | <=16 ms | 3000 |
| 91 | prop-read.Pawn | PASS | valid=true | <=16 ms | 3001 |
| 92 | prop-read.PlayerCameraManager | PASS | valid=true | <=16 ms | 3002 |
| 93 | prop-read.Player | PASS | valid=true | <=16 ms | 3003 |
| 94 | prop-read.bool-bShowMouseCursor | PASS | v=false | <=16 ms | 3004 |
| 95 | uobj.GetPropertyValue | PASS | type=boolean same-as-direct=true | <=16 ms | 3005 |
| 96 | uobj.Reflection.GetProperty | PASS | valid | <=16 ms | 3006 |
| 97 | prop.ContainerPtrToValuePtr | PASS | type=userdata | <=16 ms | 3007 |
| 98 | UStruct.GetSuperStruct | PASS | depth=7 | <=16 ms | 3008 |
| 99 | UStruct.ForEachProperty | PASS | n=34 | <=16 ms | 3009 |
| 100 | UEnum.ForEachName | PASS | n=0 v0=nil | <=16 ms | 3011 |
| 101 | TArray-acquire-ConsoleKeys | PASS | got-array | <=16 ms | 3012 |
| 102 | TArray-len | PASS | n=2 | <=16 ms | 3013 |
| 103 | TArray-index-read | PASS | got-elem | <=16 ms | 3014 |
| 104 | TArray.GetArrayNum | PASS | n=2 | <=16 ms | 3015 |
| 105 | TArray.GetArrayMax | PASS | n=18 | <=16 ms | 3016 |
| 106 | TArray.GetArrayAddress | PASS | number | <=16 ms | 3017 |
| 107 | TArray.GetArrayDataAddress | PASS | number | <=16 ms | 3018 |
| 108 | TArray.IsValid | PASS | true | <=16 ms | 3019 |
| 109 | TArray.ForEach | PASS | n=2 | <=16 ms | 3020 |
| 110 | NameProp-userdata-read | PASS | Tilde | <=16 ms | 3021 |
| 111 | loadfile-native | PASS | fn=function | <=16 ms | 3022 |
| 112 | RegisterHook | PASS | 71,72 | <=16 ms | 3024 |
| 113 | UnregisterHook | PASS | ret=nil | <=16 ms | 3025 |
| 114 | NotifyOnNewObject | PASS | ret=nil | <=16 ms | 3027 |
| 115 | RegisterCustomEvent | PASS | ret=nil | <=16 ms | 3028 |
| 116 | UnregisterCustomEvent | PASS | ret=nil | <=16 ms | 3029 |
| 117 | RegisterLoadMapPreHook | PASS | ret=nil | <=16 ms | 3030 |
| 118 | RegisterLoadMapPostHook | PASS | ret=nil | <=16 ms | 3031 |
| 119 | RegisterInitGameStatePreHook | PASS | registered | <=16 ms | 3032 |
| 120 | RegisterInitGameStatePostHook | PASS | registered | <=16 ms | 3033 |
| 121 | RegisterBeginPlayPreHook | PASS | registered | <=16 ms | 3034 |
| 122 | RegisterBeginPlayPostHook | PASS | registered | <=16 ms | 3035 |
| 123 | RegisterEndPlayPreHook | PASS | registered | <=16 ms | 3036 |
| 124 | RegisterEndPlayPostHook | PASS | registered | <=16 ms | 3037 |
| 125 | RegisterProcessConsoleExecPreHook | PASS | registered | <=16 ms | 3038 |
| 126 | RegisterProcessConsoleExecPostHook | PASS | registered | <=16 ms | 3039 |
| 127 | RegisterCallFunctionByNameWithArgumentsPreHook | PASS | registered | <=16 ms | 3040 |
| 128 | RegisterCallFunctionByNameWithArgumentsPostHook | PASS | registered | <=16 ms | 3041 |
| 129 | RegisterULocalPlayerExecPreHook | PASS | registered | <=16 ms | 3042 |
| 130 | RegisterULocalPlayerExecPostHook | PASS | registered | <=16 ms | 3043 |
| 131 | RegisterConsoleCommandHandler | PASS | ret=nil | <=16 ms | 3044 |
| 132 | RegisterConsoleCommandGlobalHandler | PASS | ret=nil | <=16 ms | 3045 |
