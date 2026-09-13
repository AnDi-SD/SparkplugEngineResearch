using OpenTK.Windowing.Desktop;
using OpenTK.Mathematics;

internal class Program
{
    static void Main(string[] args)
    {
        var gameSettings = GameWindowSettings.Default;

        var nativeSettings = new NativeWindowSettings()
        {
            ClientSize = new Vector2i(1280, 720),
            Title = "SMO Viewer"
        };

        using var window = new ViewerWindow(gameSettings, nativeSettings);
        window.Run();
    }
}