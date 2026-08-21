using System.Windows;
namespace WinxHairPatcher.Gui;
public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        string? initialExePath = e.Args.FirstOrDefault(argument =>
            !string.IsNullOrWhiteSpace(argument));
        var window = new MainWindow(initialExePath);
        MainWindow = window;
        window.Show();
    }
}
