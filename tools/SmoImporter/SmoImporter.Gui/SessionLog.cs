using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace SmoImporter.Gui;

internal static class SessionLog
{
    private static readonly object Gate = new();
    private static StreamWriter? _writer;

    public static string FilePath { get; private set; } = string.Empty;

    public static void Initialize()
    {
        lock (Gate)
        {
            if (_writer is not null)
                return;

            string portablePath = Path.Combine(
                AppContext.BaseDirectory,
                "SmoImporter.log");
            string localPath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "SparkplugEngineResearch",
                "SmoImporter",
                "SmoImporter.log");
            Exception? portableFailure = null;
            try
            {
                Open(portablePath);
            }
            catch (Exception exception) when (exception is UnauthorizedAccessException or
                                              IOException or
                                              NotSupportedException)
            {
                portableFailure = exception;
                Open(localPath);
            }

            WriteCore("SESSION", "START",
                $"SmoImporter session started. version={GetVersion()}; " +
                $"process={Environment.ProcessId}; architecture={RuntimeInformation.ProcessArchitecture}; " +
                $"os={Environment.OSVersion}; runtime={Environment.Version}; " +
                $"baseDirectory={AppContext.BaseDirectory}; currentDirectory={Environment.CurrentDirectory}");
            WriteCore("SESSION", "COMMAND_LINE",
                string.Join(" ", Environment.GetCommandLineArgs().Select(Quote)));
            WriteCore("SESSION", "LOG_PATH", FilePath);
            if (portableFailure is not null)
            {
                WriteCore("WARN", "PORTABLE_LOG_FALLBACK",
                    $"Could not create the log beside the executable; LocalAppData is used. " +
                    portableFailure.Message);
            }
        }
    }

    public static void Info(string category, string message) =>
        Write("INFO", category, message);

    public static void Warning(string category, string message) =>
        Write("WARN", category, message);

    public static void Error(string category, Exception exception) =>
        Write("ERROR", category, exception.ToString());

    public static void Error(string category, string message) =>
        Write("ERROR", category, message);

    public static void Flush()
    {
        lock (Gate)
        {
            try
            {
                _writer?.Flush();
            }
            catch
            {
            }
        }
    }

    public static void Shutdown(string reason)
    {
        lock (Gate)
        {
            if (_writer is null)
                return;
            try
            {
                WriteCore("SESSION", "STOP", reason);
                _writer.Flush();
                _writer.Dispose();
            }
            catch
            {
            }
            finally
            {
                _writer = null;
            }
        }
    }

    private static void Write(string level, string category, string message)
    {
        lock (Gate)
        {
            try
            {
                if (_writer is null)
                    Initialize();
                WriteCore(level, category, message);
            }
            catch
            {
                Debug.WriteLine($"SmoImporter log failure: [{level}] {category}: {message}");
            }
        }
    }

    private static void Open(string path)
    {
        string fullPath = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
        var stream = new FileStream(
            fullPath,
            FileMode.Create,
            FileAccess.Write,
            FileShare.ReadWrite | FileShare.Delete);
        // Emit a BOM so Windows PowerShell 5, Notepad and third-party viewers
        // all agree that Russian diagnostic text is UTF-8.
        _writer = new StreamWriter(stream, new UTF8Encoding(encoderShouldEmitUTF8Identifier: true))
        {
            AutoFlush = true
        };
        FilePath = fullPath;
    }

    private static void WriteCore(string level, string category, string message)
    {
        string normalized = (message ?? string.Empty)
            .Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n')
            .Replace("\n", "\n    ", StringComparison.Ordinal);
        _writer?.WriteLine(
            $"{DateTimeOffset.Now:yyyy-MM-dd HH:mm:ss.fff zzz} " +
            $"[T{Environment.CurrentManagedThreadId:D2}] [{level}] [{category}] {normalized}");
    }

    private static string GetVersion() =>
        Assembly.GetEntryAssembly()?.GetCustomAttribute<AssemblyInformationalVersionAttribute>()
            ?.InformationalVersion ??
        Assembly.GetEntryAssembly()?.GetName().Version?.ToString() ??
        "unknown";

    private static string Quote(string value) =>
        value.Any(char.IsWhiteSpace)
            ? $"\"{value.Replace("\"", "\\\"", StringComparison.Ordinal)}\""
            : value;
}
