using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Windows;
using System.Windows.Navigation;

namespace SmoViewer;

using MessageBox = System.Windows.MessageBox;

public partial class AboutWindow : Window
{
    public AboutWindow(
        string? exporterExecutable,
        string? importerExecutable,
        string? hairPatcherExecutable)
    {
        InitializeComponent();

        Assembly assembly = typeof(AboutWindow).Assembly;
        ViewerVersionText.Text = GetAssemblyVersion(assembly);
        ExporterVersionText.Text = GetExecutableVersion(exporterExecutable);
        ImporterVersionText.Text = GetExecutableVersion(importerExecutable);
        HairPatcherVersionText.Text = GetExecutableVersion(hairPatcherExecutable);
        BuildDateText.Text = GetBuildDate(assembly);
    }

    private static string GetAssemblyVersion(Assembly assembly)
    {
        string? informationalVersion = assembly
            .GetCustomAttribute<AssemblyInformationalVersionAttribute>()?
            .InformationalVersion;
        return NormalizeVersion(informationalVersion)
            ?? assembly.GetName().Version?.ToString(3)
            ?? "не определена";
    }

    private static string GetExecutableVersion(string? executablePath)
    {
        if (string.IsNullOrWhiteSpace(executablePath) || !File.Exists(executablePath))
            return "не входит в сборку";

        try
        {
            FileVersionInfo info = FileVersionInfo.GetVersionInfo(executablePath);
            return NormalizeVersion(info.ProductVersion)
                ?? NormalizeVersion(info.FileVersion)
                ?? "не определена";
        }
        catch (Exception)
        {
            return "не определена";
        }
    }

    private static string? NormalizeVersion(string? version)
    {
        if (string.IsNullOrWhiteSpace(version))
            return null;

        string normalized = version.Split('+', 2)[0].Trim();
        return normalized.Length == 0 ? null : normalized;
    }

    private static string GetBuildDate(Assembly assembly)
    {
        string? value = assembly.GetCustomAttributes<AssemblyMetadataAttribute>()
            .FirstOrDefault(attribute => attribute.Key == "BuildDateUtc")?
            .Value;

        if (!DateTimeOffset.TryParse(
                value,
                CultureInfo.InvariantCulture,
                DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal,
                out DateTimeOffset buildDate))
        {
            return "не определена";
        }

        return buildDate.ToLocalTime().ToString(
            "d MMMM yyyy, HH:mm",
            CultureInfo.GetCultureInfo("ru-RU"));
    }

    private void CommunityLink_RequestNavigate(object sender, RequestNavigateEventArgs e)
    {
        try
        {
            Process.Start(new ProcessStartInfo(e.Uri.AbsoluteUri)
            {
                UseShellExecute = true
            });
        }
        catch (Exception exception)
        {
            MessageBox.Show(
                this,
                $"Не удалось открыть ссылку.\n\n{exception.Message}",
                "SmoViewer",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
        }

        e.Handled = true;
    }
}
