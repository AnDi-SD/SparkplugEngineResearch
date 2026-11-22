using System.Windows;
using SmoLVLcreator.Core;

namespace SmoLVLcreator.Gui;

public partial class App : Application
{
    private ResourceDictionary? _languageResources;

    public static string VersionText => typeof(App).Assembly.GetName().Version?.ToString(3) ?? "dev";

    public static string WorkspaceTitle => LevelText.Get("Language.WorkspaceTitle", VersionText);

    internal void ApplyLanguage(string preference)
    {
        LevelText.Initialize(preference);
        var strings = new ResourceDictionary();
        foreach (string key in LevelText.Keys)
            strings["Loc." + key] = LevelText.Get(key);

        if (_languageResources is null)
            Resources.MergedDictionaries.Add(strings);
        else
            Resources.MergedDictionaries[Resources.MergedDictionaries.IndexOf(_languageResources)] = strings;
        _languageResources = strings;
    }

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        LevelText.Initialize(Environment.GetEnvironmentVariable(LevelText.WorkerLanguageVariable));
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

        ApplyLanguage(SmoLVLcreatorSettingsStore.Load().Language);
        var window = new MainWindow();
        MainWindow = window;
        window.InitializeLanguageSelector();
        window.Show();
    }
}
