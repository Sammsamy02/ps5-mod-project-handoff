// A SharpProspero payload that serves a small web page. A loader maps it into a running process and
// starts it; it listens on a port, answers each request with a status page, and keeps serving. A payload
// has no screen and no controller - it runs inside another process - so this is a plain program, not a
// frame loop. Add cases to Route to answer your own paths.
//
// A payload reaches the network through SharpProspero.Payload.PayloadNetwork, the plain socket calls the
// operating-system library publishes by name, because a payload has no dynamic linker to bind the
// wrapped network types an application module uses.

using System;
using System.Text;
using SharpProspero.Payload;
using SharpProspero.Payload.Net;

namespace SampleApp;

internal static unsafe class Program
{
    private const ushort Port = 8080;

    [System.Runtime.InteropServices.UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        int listener;
        try
        {
            listener = PayloadNetwork.Listen(Port);
        }
        catch (Exception)
        {
            // The port could not be opened; a payload ends by returning.
            return -1;
        }

        while (true)
        {
            int client = PayloadNetwork.Accept(listener);
            if (client < 0)
                continue;
            try
            {
                Serve(client);
            }
            catch (Exception)
            {
                // Drop a client that failed and keep serving the next one.
            }
            finally
            {
                PayloadNetwork.Close(client);
            }
        }
    }

    private static void Serve(int client)
    {
        Span<byte> buffer = stackalloc byte[2048];
        long read = PayloadNetwork.Receive(client, buffer);
        if (read <= 0)
            return;
        string path = RequestPath(Encoding.ASCII.GetString(buffer[..(int)read]));
        (int status, string body) = Route(path);
        WriteResponse(client, status, body);
    }

    // The path from the request line, e.g. "/status" from "GET /status HTTP/1.1".
    private static string RequestPath(string request)
    {
        int firstSpace = request.IndexOf(' ');
        if (firstSpace < 0)
            return "/";
        int secondSpace = request.IndexOf(' ', firstSpace + 1);
        return secondSpace < 0 ? "/" : request[(firstSpace + 1)..secondSpace];
    }

    // Answer a path. Add your own cases here.
    private static (int Status, string Body) Route(string path) => path switch
    {
        "/" => (200, "<html><body><h1>SharpProspero payload</h1><p>The web service is running.</p></body></html>"),
        "/status" => (200, "ok"),
        _ => (404, "not found"),
    };

    private static void WriteResponse(int client, int status, string body)
    {
        string reason = status == 200 ? "OK" : "Not Found";
        byte[] bodyBytes = Encoding.UTF8.GetBytes(body);
        string header = $"HTTP/1.1 {status} {reason}\r\n"
            + "Content-Type: text/html; charset=utf-8\r\n"
            + $"Content-Length: {bodyBytes.Length}\r\n"
            + "Connection: close\r\n\r\n";
        PayloadNetwork.SendAll(client, Encoding.ASCII.GetBytes(header));
        PayloadNetwork.SendAll(client, bodyBytes);
    }
}
