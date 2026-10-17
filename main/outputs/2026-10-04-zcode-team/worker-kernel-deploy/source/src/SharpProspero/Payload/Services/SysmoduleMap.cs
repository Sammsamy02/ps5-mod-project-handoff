// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Payload.Services;

/// <summary>
/// Maps library sonames to their internal sysmodule identifiers for
/// <c>sceSysmoduleLoadModuleInternal</c>. The identifier is the library's registration
/// number with the <c>0x80000000</c> mask applied.
/// </summary>
public static class PayloadSysmoduleMap
{
    /// <summary>
    /// Returns the internal sysmodule identifier for the given library soname string,
    /// or zero if the library is not in the map.
    /// </summary>
    public static uint GetInternalId(string soname)
    {
        for (int i = 0; i < Table.Length; i++)
        {
            if (Table[i].Name == soname)
                return Table[i].Id;
        }
        return 0;
    }

    /// <summary>One entry in the sysmodule map.</summary>
    /// <param name="Name">The library soname.</param>
    /// <param name="Id">The internal sysmodule identifier with the 0x80000000 mask.</param>
    public readonly record struct Entry(string Name, uint Id);

    /// <summary>The complete sysmodule map (139 entries).</summary>
    public static readonly Entry[] Table =
    [
        new("libSceNpManager", 0x80000002),
        new("libSceInvitationDialog", 0x80000003),
        new("libSceNet", 0x80000009),
        new("libSceNetCtl", 0x80000014),
        new("libSceSsl", 0x80000018),
        new("libSceHttp", 0x8000001B),
        new("libSceHttp2", 0x8000001C),
        new("libSceNpCommon", 0x80000023),
        new("libSceSysUtil", 0x80000026),
        new("libSceRtc", 0x80000028),
        new("libSceJson", 0x80000029),
        new("libSceJson2", 0x8000002A),
        new("libSceWebBrowserDialog", 0x80000031),
        new("libSceNpTrophy", 0x80000032),
        new("libSceNpSnsTrophy", 0x80000033),
        new("libSceIme", 0x80000034),
        new("libSceImeDialog", 0x80000035),
        new("libSceMsgDialog", 0x80000038),
        new("libSceErrorDialog", 0x80000039),
        new("libSceRandom", 0x80000040),
        new("libSceCompanionHttpd", 0x80000041),
        new("libSceCompanionUtil", 0x80000042),
        new("libSceSigninDialog", 0x80000044),
        new("libSceNpProfileDialog", 0x80000045),
        new("libSceSaveData", 0x80000046),
        new("libSceSaveDataDialog", 0x80000047),
        new("libSceFiber", 0x8000004A),
        new("libSceAppContent", 0x8000004D),
        new("libSceNpAuth", 0x8000004E),
        new("libSceDiscMap", 0x80000050),
        new("libScePlayGo", 0x80000051),
        new("libSceNpParty", 0x80000052),
        new("libSceFontFt", 0x80000054),
        new("libSceFont", 0x80000055),
        new("libSceVideodec", 0x80000057),
        new("libScePngDec", 0x80000058),
        new("libScePngEnc", 0x80000059),
        new("libSceJpegDec", 0x8000005A),
        new("libSceJpegEnc", 0x8000005B),
        new("libSceMove", 0x8000005E),
        new("libSceVoice", 0x80000060),
        new("libSceVoiceQos", 0x80000061),
        new("libSceContentDelete", 0x80000063),
        new("libSceContentExport", 0x80000064),
        new("libSceContentSearch", 0x80000065),
        new("libSceFsInternalForVsh", 0x80000066),
        new("libSceAppInstUtil", 0x80000068),
        new("libSceBgft", 0x8000006A),
        new("libSceAvSetting", 0x8000006D),
        new("libSceMbus", 0x8000006E),
        new("libSceNpGameIntent", 0x80000070),
        new("libSceGameUpdate", 0x80000071),
        new("libSceAudioIn", 0x80000074),
        new("libSceAudioOut", 0x80000075),
        new("libSceM4aacEnc", 0x80000076),
        new("libSceAudiodec", 0x80000077),
        new("libSceAudioDecCpu", 0x80000078),
        new("libSceAt9Enc", 0x80000079),
        new("libSceConvertKeycode", 0x8000007E),
        new("libSceSharePlay", 0x80000080),
        new("libSceCompositeExt", 0x80000083),
        new("libSceScreenShot", 0x80000085),
        new("libSceAppMessaging", 0x80000087),
        new("libSceNgs2", 0x80000088),
        new("libSceShareFactoryUtil", 0x80000089),
        new("libSceRemoteplay", 0x8000008A),
        new("libSceUsbStorage", 0x8000008E),
        new("libSceAvPlayer", 0x8000008F),
        new("libSceMediaFrameworkUtil", 0x80000090),
        new("libSceNpPartyVsh", 0x80000092),
        new("libSceZlib", 0x80000093),
        new("libSceCdlgUtilServer", 0x80000095),
        new("libSceGameCustomDataDialog", 0x80000096),
        new("libSceNpScore", 0x80000097),
        new("libSceNpMatching2", 0x80000098),
        new("libSceNpSignaling", 0x80000099),
        new("libSceLoginDialog", 0x8000009B),
        new("libSceLoginService", 0x8000009C),
        new("libSceNpWebApi", 0x800000A2),
        new("libSceRegMgr", 0x800000A4),
        new("libSceUserService", 0x800000A5),
        new("libSceAudio3d", 0x800000A6),
        new("libSceAjm", 0x800000A7),
        new("libSceNpCommerce", 0x800000A8),
        new("libSceCamera", 0x800000A9),
        new("libSceMouse", 0x800000AA),
        new("libSceSystemService", 0x800000AB),
        new("libSceCompanion", 0x800000AE),
        new("libSceKeyboard", 0x800000AF),
        new("libScePad", 0x800000B0),
        new("libSceDepth2", 0x800000B1),
        new("libSceVideodec2", 0x800000B2),
        new("libSceVideoRecording", 0x800000B3),
        new("libSceContentSearchSrv", 0x800000B4),
        new("libSceJsc", 0x800000B5),
        new("libSceCes", 0x800000B6),
        new("libSceS3DConversion", 0x800000B7),
        new("libSceShareInternal", 0x800000B8),
        new("libSceCoredump", 0x800000BA),
        new("libSceVenc", 0x800000BD),
        new("libSceVdecwrap", 0x800000BE),
        new("libSceNotification", 0x800000C0),
        new("libSceNpTus", 0x800000C2),
        new("libSceGnmDriver", 0x800000CB),
        new("libSceAgcDriver", 0x800000CC),
        new("libSceGameLiveStreaming", 0x800000CD),
        new("libSceAutoMounterClient", 0x800000CE),
        new("libSceNpToolkit2", 0x800000CF),
        new("libSceNpUniversalDataSystem", 0x800000D0),
        new("libSceBluetoothHid", 0x800000D1),
        new("libSceCffMgr", 0x800000D2),
        new("libSceVdecsw", 0x800000D3),
        new("libSceLibcInternal", 0x800000D4),
        new("libSceDebugger", 0x800000D5),
        new("libSceNpTrophy2", 0x800000D6),
        new("libSceWebApi2", 0x800000D7),
        new("libSceDbg", 0x800000D8),
        new("libSceNpAuth2", 0x800000D9),
        new("libSceKernelDebug", 0x800000DA),
        new("libSceShellCoreUtil", 0x800000DB),
        new("libSceDeci5Ttyp", 0x800000DC),
        new("libSceAvcap2", 0x800000DD),
        new("libSceNpPartyUds", 0x800000DF),
        new("libSceVrTracker", 0x800000E0),
        new("libSceVrServiceDialog", 0x800000E1),
        new("libSceSystemStateMgr", 0x800000E2),
    ];

}
