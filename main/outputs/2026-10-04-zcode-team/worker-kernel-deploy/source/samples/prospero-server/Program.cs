// A SharpProspero network service: a small HTTP control panel served from the module. It serves a page
// at "/" and a JSON status at "/status", handling requests in the frame loop without blocking it, and
// shows the address and the request count on screen. Press Options to stop the server and exit. Add your
// own routes in Handle.

using System;
using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Interop.Pad;
using SharpProspero.Platform;
using SharpProspero.Storage;

namespace SampleApp;

internal sealed class Server : ProsperoApp
{
    private const int Port = 8080;

    private HttpServer? _server;
    private string _address = "starting...";
    private long _requests;
    private string _lastRequest = "-";

    protected override void OnLoad()
    {
        // Opening the socket and status services needs the module to be permitted to use the network;
        // both are tolerant, so the panel shows why when one is not available rather than failing.
        try
        {
            _server = HttpServer.Start(Port);
            using var net = NetworkInfo.Open();
            _address = (net.IsConnected ? net.IpAddress : "0.0.0.0") + ":" + Port;
        }
        catch (Exception e)
        {
            _address = "unavailable (" + e.Message + ")";
        }
    }

    protected override void OnFrame(FrameContext context)
    {
        // Service the requests that are waiting, without blocking the frame. A cap keeps a burst of
        // requests from stalling the loop, so the screen keeps drawing and Options still exits; any left
        // over are handled next frame.
        if (_server is not null)
        {
            for (int i = 0; i < 64 && _server.PollOnce(Handle, timeoutMicroseconds: 0); i++)
            {
            }
        }

        Draw(context.Surface);

        if (context.Pressed(ScePadButton.Options))
            context.RequestExit();
    }

    private HttpServerResponse Handle(HttpServerRequest request)
    {
        _requests++;
        _lastRequest = request.Method + " " + request.Path;

        if (request.Path == "/status")
        {
            var status = JsonValue.NewObject();
            status["ok"] = true;
            status["title"] = "APP_TITLE";
            status["requests"] = (double)_requests;
            return HttpServerResponse.Json(status.Write());
        }
        if (request.Path == "/")
            return HttpServerResponse.Html(Page());

        return HttpServerResponse.NotFound();
    }

    private string Page() =>
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>APP_TITLE</title></head>"
        + "<body style=\"font-family:sans-serif;background:#10141a;color:#eceef3;padding:2rem\">"
        + "<h1>APP_TITLE</h1><p>Served from a SharpProspero module.</p>"
        + "<p>Requests handled: " + _requests + "</p>"
        + "<p><a style=\"color:#5aa0ff\" href=\"/status\">/status</a> returns JSON.</p>"
        + "</body></html>";

    protected override void OnUnload() => _server?.Dispose();

    private void Draw(Surface surface)
    {
        surface.Clear(Color.FromRgb(0x0E, 0x12, 0x18));
        surface.DrawText("APP_TITLE", 80, 80, 4, Color.White);

        Color body = Color.FromRgb(0xC8, 0xD0, 0xDC);
        surface.DrawText("Listening on " + _address, 80, 190, 2, body);
        surface.DrawText("Requests handled: " + _requests, 80, 230, 2, body);
        surface.DrawText("Last request: " + _lastRequest, 80, 270, 2, body);

        Color hint = Color.FromRgb(0x8A, 0x94, 0xA0);
        surface.DrawText("Open http://" + _address + "/ from a device on the same network.", 80, 330, 2, hint);
        surface.DrawText("Press Options to stop and exit", 80, 370, 2, hint);
    }
}

internal static class Program
{
    private static void Main()
    {
        using (var app = new Server())
            app.Run();

        // Returning from here is reported to the platform as a fault and the user is shown the
        // box that says the application closed unexpectedly, even when everything went as
        // intended. The process is ended through the C library instead.
        ProcessExit.Exit();
    }
}
