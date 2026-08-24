using System.Windows;
using System.Windows.Threading;

namespace SmoImporter.Gui;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        SessionLog.Initialize();
        DispatcherUnhandledException += OnDispatcherUnhandledException;
        AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
        TaskScheduler.UnobservedTaskException += OnUnobservedTaskException;
        base.OnStartup(e);
    }

    protected override void OnExit(ExitEventArgs e)
    {
        SessionLog.Shutdown($"Application exit code {e.ApplicationExitCode}.");
        base.OnExit(e);
    }

    private static void OnDispatcherUnhandledException(
        object sender,
        DispatcherUnhandledExceptionEventArgs e) =>
        SessionLog.Error("DISPATCHER_UNHANDLED_EXCEPTION", e.Exception);

    private static void OnUnhandledException(object? sender, UnhandledExceptionEventArgs e)
    {
        if (e.ExceptionObject is Exception exception)
            SessionLog.Error("APPDOMAIN_UNHANDLED_EXCEPTION", exception);
        else
            SessionLog.Error(
                "APPDOMAIN_UNHANDLED_EXCEPTION",
                e.ExceptionObject?.ToString() ?? "Unknown non-Exception failure.");
        SessionLog.Flush();
    }

    private static void OnUnobservedTaskException(
        object? sender,
        UnobservedTaskExceptionEventArgs e) =>
        SessionLog.Error("UNOBSERVED_TASK_EXCEPTION", e.Exception);
}
