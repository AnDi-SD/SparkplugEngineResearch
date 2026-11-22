using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Globalization;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("Sparkplug .NET Runtime Bootstrapper")]
[assembly: System.Reflection.AssemblyDescription("Installs the official Microsoft .NET Desktop Runtime when required.")]
[assembly: System.Reflection.AssemblyCompany("Sparkplug Engine Research")]

internal static class RuntimeBootstrap
{
    private const string RequiredFramework = "Microsoft.WindowsDesktop.App";
    private const int RequiredMajor = 8;
    private const long MaximumInstallerBytes = 150L * 1024L * 1024L;
    private const string InstallerUrl = "https://aka.ms/dotnet/8.0/windowsdesktop-runtime-win-x64.exe";
    private const string ConfigName = "release.json";
    private static readonly Lazy<Dictionary<string, string>> Messages =
        new Lazy<Dictionary<string, string>>(LoadMessages);

    private static Dictionary<string, string> LoadMessages()
    {
        Assembly assembly = typeof(RuntimeBootstrap).Assembly;
        var product = Attribute.GetCustomAttribute(assembly, typeof(AssemblyProductAttribute)) as AssemblyProductAttribute;
        string language = "ru";
        if (product != null && product.Product == "SmoLVLcreator")
        {
            language = System.Threading.Thread.CurrentThread.CurrentUICulture.TwoLetterISOLanguageName == "ru" ? "ru" : "en";
            try
            {
                string settingsPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "SparkplugEngineResearch", "SmoLVLcreator", "settings.json");
                if (File.Exists(settingsPath))
                {
                    var settings = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(
                        File.ReadAllText(settingsPath, Encoding.UTF8));
                    object preference;
                    if (settings != null && settings.TryGetValue("Language", out preference))
                    {
                        if (Equals(preference, "Russian")) language = "ru";
                        if (Equals(preference, "English")) language = "en";
                    }
                }
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
            catch (ArgumentException) { }
            catch (InvalidOperationException) { }
        }

        using (Stream stream = assembly.GetManifestResourceStream("Bootstrapper.Strings." + language + ".json"))
        using (var reader = new StreamReader(stream, Encoding.UTF8))
            return new JavaScriptSerializer().Deserialize<Dictionary<string, string>>(reader.ReadToEnd());
    }

    private static string Message(string key, params object[] arguments)
    {
        return string.Format(CultureInfo.CurrentCulture, Messages.Value[key], arguments);
    }

    [STAThread]
    private static int Main(string[] args)
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        try
        {
            string baseDirectory = AppDomain.CurrentDomain.BaseDirectory;
            string entryPoint = ReadEntryPoint(Path.Combine(baseDirectory, ConfigName), baseDirectory);

            if (!HasRequiredRuntime())
            {
                DialogResult answer = MessageBox.Show(
                    Message("InstallPrompt"),
                    Message("InstallTitle"),
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Information,
                    MessageBoxDefaultButton.Button1);

                if (answer != DialogResult.Yes)
                    return 1223;

                InstallRuntime();
                if (!HasRequiredRuntime())
                    throw new InvalidOperationException(
                        Message("RuntimeNotFound"));
            }

            StartApplication(entryPoint, args);
            return 0;
        }
        catch (Win32Exception ex)
        {
            if (ex.NativeErrorCode == 1223)
                return 1223;

            ShowError(ex.Message);
            return ex.NativeErrorCode != 0 ? ex.NativeErrorCode : 1;
        }
        catch (Exception ex)
        {
            ShowError(ex.Message);
            return 1;
        }
    }

    private static string ReadEntryPoint(string configPath, string baseDirectory)
    {
        if (!File.Exists(configPath))
            throw new FileNotFoundException(Message("ConfigNotFound"), configPath);

        JavaScriptSerializer serializer = new JavaScriptSerializer();
        Dictionary<string, object> config = serializer.Deserialize<Dictionary<string, object>>(
            File.ReadAllText(configPath, Encoding.UTF8));
        object value;
        if (config == null || !config.TryGetValue("entryPoint", out value) || value == null)
            throw new InvalidDataException(Message("EntryMissing"));

        string basePath = Path.GetFullPath(baseDirectory).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        string target = Path.GetFullPath(Path.Combine(baseDirectory, Convert.ToString(value)));
        if (!target.StartsWith(basePath, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException(Message("EntryOutsidePackage"));
        if (!File.Exists(target))
            throw new FileNotFoundException(Message("ApplicationNotFound"), target);
        return target;
    }

    private static bool HasRequiredRuntime()
    {
        foreach (string root in GetDotNetRoots())
        {
            string sharedFramework = Path.Combine(root, "shared", RequiredFramework);
            if (!Directory.Exists(sharedFramework))
                continue;

            foreach (string directory in Directory.GetDirectories(sharedFramework))
            {
                Version version;
                if (Version.TryParse(Path.GetFileName(directory), out version) && version.Major == RequiredMajor)
                    return true;
            }
        }
        return false;
    }

    private static IEnumerable<string> GetDotNetRoots()
    {
        HashSet<string> roots = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        AddRoot(roots, GetRegisteredInstallLocation());
        AddRoot(roots, Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "dotnet"));
        return roots;
    }

    private static void AddRoot(HashSet<string> roots, string path)
    {
        if (!String.IsNullOrWhiteSpace(path))
            roots.Add(Path.GetFullPath(Environment.ExpandEnvironmentVariables(path)));
    }

    private static string GetRegisteredInstallLocation()
    {
        try
        {
            using (RegistryKey machine = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
            using (RegistryKey key = machine.OpenSubKey(@"SOFTWARE\dotnet\Setup\InstalledVersions\x64"))
                return key == null ? null : key.GetValue("InstallLocation") as string;
        }
        catch
        {
            return null;
        }
    }

    private static void InstallRuntime()
    {
        string temporaryDirectory = Path.Combine(Path.GetTempPath(), "SparkplugDotNetBootstrap", Guid.NewGuid().ToString("N"));
        string installerPath = Path.Combine(temporaryDirectory, "windowsdesktop-runtime-8-win-x64.exe");
        Directory.CreateDirectory(temporaryDirectory);

        try
        {
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072;
            using (DownloadDialog dialog = new DownloadDialog(InstallerUrl, installerPath))
                dialog.Download();

            VerifyMicrosoftSignature(installerPath);

            ProcessStartInfo startInfo = new ProcessStartInfo(installerPath, "/install /quiet /norestart");
            startInfo.UseShellExecute = true;
            startInfo.Verb = "runas";
            using (Process installer = Process.Start(startInfo))
            {
                if (installer == null)
                    throw new InvalidOperationException(Message("InstallerStartFailed"));
                installer.WaitForExit();
                if (installer.ExitCode != 0 && installer.ExitCode != 3010)
                    throw new InvalidOperationException(
                        Message("InstallerExitCode", installer.ExitCode));
            }
        }
        finally
        {
            try { Directory.Delete(temporaryDirectory, true); }
            catch { }
        }
    }

    private static void VerifyMicrosoftSignature(string path)
    {
        if (!Authenticode.IsTrusted(path))
            throw new InvalidDataException(Message("InvalidSignature"));

        X509Certificate2 certificate = new X509Certificate2(X509Certificate.CreateFromSignedFile(path));
        try
        {
            if (certificate.Subject.IndexOf("O=Microsoft Corporation", StringComparison.OrdinalIgnoreCase) < 0)
                throw new InvalidDataException(Message("IncorrectSigner"));
        }
        finally
        {
            certificate.Dispose();
        }
    }

    private static void StartApplication(string entryPoint, string[] args)
    {
        ProcessStartInfo startInfo = new ProcessStartInfo(entryPoint);
        startInfo.UseShellExecute = false;
        startInfo.WorkingDirectory = Path.GetDirectoryName(entryPoint);
        startInfo.Arguments = JoinArguments(args);
        startInfo.EnvironmentVariables.Remove("DOTNET_ROOT");
        startInfo.EnvironmentVariables.Remove("DOTNET_ROOT_X64");
        Process process = Process.Start(startInfo);
        if (process == null)
            throw new InvalidOperationException(Message("ApplicationStartFailed"));
    }

    private static string JoinArguments(string[] args)
    {
        StringBuilder result = new StringBuilder();
        for (int index = 0; index < args.Length; index++)
        {
            if (index > 0) result.Append(' ');
            result.Append(QuoteArgument(args[index]));
        }
        return result.ToString();
    }

    private static string QuoteArgument(string value)
    {
        if (value.Length > 0 && value.IndexOfAny(new[] { ' ', '\t', '\n', '\v', '"' }) < 0)
            return value;

        StringBuilder quoted = new StringBuilder("\"");
        int backslashes = 0;
        foreach (char character in value)
        {
            if (character == '\\')
            {
                backslashes++;
            }
            else if (character == '"')
            {
                quoted.Append('\\', backslashes * 2 + 1);
                quoted.Append('"');
                backslashes = 0;
            }
            else
            {
                quoted.Append('\\', backslashes);
                quoted.Append(character);
                backslashes = 0;
            }
        }
        quoted.Append('\\', backslashes * 2);
        quoted.Append('"');
        return quoted.ToString();
    }

    private static void ShowError(string message)
    {
        MessageBox.Show(message, Message("LaunchError"), MessageBoxButtons.OK, MessageBoxIcon.Error);
    }

    private sealed class DownloadDialog : Form
    {
        private readonly string source;
        private readonly string destination;
        private readonly ProgressBar progress;
        private readonly Label status;
        private WebClient client;
        private Exception error;

        internal DownloadDialog(string source, string destination)
        {
            this.source = source;
            this.destination = destination;
            Text = Message("InstallProgressTitle");
            ClientSize = new Size(440, 92);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            ControlBox = false;
            StartPosition = FormStartPosition.CenterScreen;

            status = new Label();
            status.AutoSize = false;
            status.Text = Message("StartingDownload");
            status.SetBounds(18, 16, 404, 22);
            Controls.Add(status);

            progress = new ProgressBar();
            progress.SetBounds(18, 48, 404, 22);
            progress.Style = ProgressBarStyle.Marquee;
            Controls.Add(progress);
        }

        internal void Download()
        {
            Shown += BeginDownload;
            Application.Run(this);
            if (error != null)
                throw new InvalidOperationException(Message("DownloadFailed", error.Message), error);
            if (!File.Exists(destination) || new FileInfo(destination).Length == 0)
                throw new InvalidDataException(Message("EmptyDownload"));
        }

        private void BeginDownload(object sender, EventArgs args)
        {
            try
            {
                client = new WebClient();
                client.Headers[HttpRequestHeader.UserAgent] = "SparkplugRuntimeBootstrap/1.0";
                client.DownloadProgressChanged += ProgressChanged;
                client.DownloadFileCompleted += DownloadCompleted;
                client.DownloadFileAsync(new Uri(source), destination);
            }
            catch (Exception ex)
            {
                error = ex;
                Close();
            }
        }

        private void ProgressChanged(object sender, DownloadProgressChangedEventArgs args)
        {
            if (args.BytesReceived > MaximumInstallerBytes)
            {
                error = new InvalidDataException(Message("DownloadTooLarge"));
                client.CancelAsync();
                return;
            }
            if (args.TotalBytesToReceive <= 0) return;
            progress.Style = ProgressBarStyle.Continuous;
            progress.Value = Math.Max(0, Math.Min(100, args.ProgressPercentage));
            status.Text = Message("DownloadProgress", args.ProgressPercentage);
        }

        private void DownloadCompleted(object sender, AsyncCompletedEventArgs args)
        {
            if (args.Cancelled && error == null)
                error = new OperationCanceledException(Message("DownloadCancelled"));
            else if (!args.Cancelled)
                error = args.Error;
            Close();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && client != null)
                client.Dispose();
            base.Dispose(disposing);
        }
    }

    private static class Authenticode
    {
        private static readonly Guid VerifyAction = new Guid("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");

        [DllImport("wintrust.dll", CharSet = CharSet.Unicode, ExactSpelling = true)]
        private static extern int WinVerifyTrust(IntPtr window, [MarshalAs(UnmanagedType.LPStruct)] Guid action, WinTrustData data);

        internal static bool IsTrusted(string path)
        {
            using (WinTrustFileInfo file = new WinTrustFileInfo(path))
            using (WinTrustData data = new WinTrustData(file))
                return WinVerifyTrust(IntPtr.Zero, VerifyAction, data) == 0;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private sealed class WinTrustFileInfo : IDisposable
        {
            private uint size = (uint)Marshal.SizeOf(typeof(WinTrustFileInfo));
            private IntPtr filePath;
            private IntPtr fileHandle = IntPtr.Zero;
            private IntPtr knownSubject = IntPtr.Zero;

            internal WinTrustFileInfo(string path)
            {
                filePath = Marshal.StringToCoTaskMemUni(path);
            }

            public void Dispose()
            {
                if (filePath != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(filePath);
                    filePath = IntPtr.Zero;
                }
            }
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private sealed class WinTrustData : IDisposable
        {
            private uint size = (uint)Marshal.SizeOf(typeof(WinTrustData));
            private IntPtr policyCallbackData = IntPtr.Zero;
            private IntPtr sipClientData = IntPtr.Zero;
            private uint uiChoice = 2;
            private uint revocationChecks = 0;
            private uint unionChoice = 1;
            private IntPtr fileInfo;
            private uint stateAction = 0;
            private IntPtr stateData = IntPtr.Zero;
            private string urlReference = null;
            private uint providerFlags = 0x00000080;
            private uint uiContext = 0;

            internal WinTrustData(WinTrustFileInfo file)
            {
                fileInfo = Marshal.AllocCoTaskMem(Marshal.SizeOf(typeof(WinTrustFileInfo)));
                Marshal.StructureToPtr(file, fileInfo, false);
            }

            public void Dispose()
            {
                if (fileInfo != IntPtr.Zero)
                {
                    Marshal.DestroyStructure(fileInfo, typeof(WinTrustFileInfo));
                    Marshal.FreeCoTaskMem(fileInfo);
                    fileInfo = IntPtr.Zero;
                }
            }
        }
    }
}
