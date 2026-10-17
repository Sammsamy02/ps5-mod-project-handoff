// Sends an elaborate JSON notification toast through
// sceNotificationSend with InteractiveToastTemplateB, Downloads channel, and a DeepLink
// action. Uses the SCE_NOTIFICATION_LOCAL_USER_ID_SYSTEM (0xFE) user id.

using System;
using System.Runtime.InteropServices;
using SharpProspero.Payload.Services;

namespace SampleApp;

internal static unsafe class Program
{
    // The toast template is built as a NUL-terminated byte sequence to avoid managed string
    // allocation. This follows the static const char toast_tmpl[].
    private static ReadOnlySpan<byte> ToastTemplate => "{\n  \"rawData\": {\n    \"viewTemplateType\": \"InteractiveToastTemplateB\",\n    \"channelType\": \"Downloads\",\n    \"useCaseId\": \"IDC\",\n    \"toastOverwriteType\": \"No\",\n    \"isImmediate\": true,\n    \"priority\": 100,\n    \"viewData\": {\n      \"icon\": {\n        \"type\": \"Url\",\n        \"parameters\": {\n          \"url\": \"/path/to/icon.png\"\n        }\n      },\n      \"message\": {\n        \"body\": \"Hello World!\"\n      },\n      \"subMessage\": {\n        \"body\": \"SharpProspero notify sample\"\n      },\n      \"actions\": [\n        {\n          \"actionName\": \"Go to debug settings\",\n          \"actionType\": \"DeepLink\",\n          \"defaultFocus\": true,\n          \"parameters\": {\n            \"actionUrl\": \"pssettings:play?function=debug_settings\"\n          }\n        }\n      ]\n    },\n    \"platformViews\": {\n      \"previewDisabled\": {\n        \"viewData\": {\n          \"icon\": {\n            \"type\": \"Predefined\",\n            \"parameters\": {\n              \"icon\": \"download\"\n            }\n          },\n          \"message\": {\n            \"body\": \"SharpProspero notify sample is running\"\n          }\n        }\n      }\n    }\n  },\n  \"createdDateTime\": \"2025-12-14T03:14:51.473Z\",\n  \"localNotificationId\": \"588193127\"\n}\0"u8;

    [UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        return PayloadNotification.SendNotification(0xFE, true, ToastTemplate);
    }
}
