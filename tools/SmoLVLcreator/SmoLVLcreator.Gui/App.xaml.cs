using System.Windows;
using SmoLVLcreator.Core;

namespace SmoLVLcreator.Gui;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        if (e.Args.Length == 2 && e.Args[0].Equals(
                "--isolated-external-model-batch",
                StringComparison.OrdinalIgnoreCase))
        {
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            int exitCode = SmoExternalModelBatchJob.Run(e.Args[1]);
            Shutdown(exitCode);
            return;
        }
        if (e.Args.Length > 0 && e.Args[0].Equals(
                "--isolated-save-worker",
                StringComparison.OrdinalIgnoreCase))
        {
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            int exitCode = SmoLevelSaveWorkerHost.Run(e.Args);
            Shutdown(exitCode);
            return;
        }
        if (e.Args.Length > 0 && e.Args[0].Equals(
                "--isolated-project-build-worker",
                StringComparison.OrdinalIgnoreCase))
        {
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            int exitCode = SmoProjectBuildWorkerHost.Run(e.Args);
            Shutdown(exitCode);
            return;
        }

        var window = new MainWindow();
        MainWindow = window;
        window.Show();
    }
}
