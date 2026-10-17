// Full HTTP/2 lifecycle using the SDK's PayloadHttp2 API. Initialises the stack, creates a
// template and request, sends a GET to a configurable URL, reads the response body, and
// outputs it via klog.

using System;
using System.Runtime.InteropServices;
using SharpProspero.Payload;
using SharpProspero.Payload.Net;

namespace SampleApp;

internal static unsafe class Program
{
    private static int _netMemId = -1, _sslCtxId = -1, _httpCtxId = -1;
    private static int _tmplId = -1, _reqId = -1;

    [UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        int error = 0;
        fixed (byte* agent = "http2_get/1.0\0"u8)
        fixed (byte* url = "http://ps5-host-not-configured.invalid\0"u8)
        {
            error = Http2InitStack(agent, url);
        }

        if (error == 0)
            error = Http2Get();

        Http2Fini();
        return error;
    }

    private static int Http2InitStack(byte* agent, byte* url)
    {
        int result = PayloadHttp2.InitStack(
            "http2_get\0"u8, 32 * 1024, 256 * 1024, 256 * 1024,
            out _netMemId, out _sslCtxId, out _httpCtxId);
        if (result != 0) return -1;

        _tmplId = PayloadHttp2.sceHttp2CreateTemplate(_httpCtxId, agent, 3, 1);
        if (_tmplId < 0) return -1;

        fixed (byte* method = "GET\0"u8)
        {
            _reqId = PayloadHttp2.sceHttp2CreateRequestWithURL(_tmplId, method, url, 0);
            if (_reqId < 0) return -1;
        }
        return 0;
    }

    private static int Http2Get()
    {
        if (PayloadHttp2.sceHttp2SendRequest(_reqId, null, 0) != 0) return -1;
        int status;
        if (PayloadHttp2.sceHttp2GetStatusCode(_reqId, &status) != 0) return -1;
        if (status == 200)
        {
            byte* buf = stackalloc byte[0x1000];
            int length;
            while ((length = PayloadHttp2.sceHttp2ReadData(_reqId, buf, 0x0FFF)) > 0)
            {
                buf[length] = 0;
                PayloadCrt.Klog(buf);
            }
        }
        return status;
    }

    private static void Http2Fini()
    {
        PayloadHttp2.Cleanup(_reqId, _tmplId, _httpCtxId, _sslCtxId, _netMemId);
    }
}
