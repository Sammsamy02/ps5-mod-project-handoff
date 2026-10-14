// A SharpProspero 3D application. Renders a spinning, lit cube with the graphics processor; press
// Options to exit. Swap the mesh (MeshData.Cube/Sphere/Plane), move the camera, or change the rotation.

using System.Numerics;
using SharpProspero.Graphics;
using SharpProspero.Graphics.Agc;
using SharpProspero.Input;
using SharpProspero.Interop.Pad;
using SharpProspero.Interop.SystemService;
using SharpProspero.Interop.UserService;
using SharpProspero.Numerics;

namespace SampleApp;

internal static class Program
{
    private static unsafe void Main()
    {
        // This template drives the renderer directly rather than through the application base, so the
        // two things that base does before its first frame are done here. Without the first, nothing
        // drawn is ever seen: the picture the system shows while an application starts stays on top
        // until it is told the application is ready.
        int priority = 700;
        UserService.sceUserServiceInitialize(&priority);

        using var display = DisplayDevice.Open(1920, 1080);
        using var renderer = new Renderer3D(display);
        using var cube = MeshBuffer.Upload(MeshData.Cube(1.5f, Color.FromRgb(0x4A, 0x9E, 0xFF)));
        using var pad = GamePad.Open();

        var camera = new Camera3D
        {
            Position = new Vector3(0f, 1.5f, 4.5f),
            Target = Vector3.Zero,
            AspectRatio = 1920f / 1080f,
        };

        SystemService.sceSystemServiceHideSplashScreen();

        float angle = 0f;
        while (true)
        {
            GamePadState state = pad.Read();
            if (state.IsPressed(ScePadButton.Options))
                break;

            angle += 0.02f;
            Matrix4x4 model = Matrix4x4.CreateRotationX(0.4f) * Matrix4x4.CreateRotationY(angle);
            Matrix4x4 mvp = model * camera.ViewProjection;

            // Draws the mesh and presents the frame; the present paces the loop to the display.
            renderer.DrawMesh(cube, mvp, model);
        }
    }
}
