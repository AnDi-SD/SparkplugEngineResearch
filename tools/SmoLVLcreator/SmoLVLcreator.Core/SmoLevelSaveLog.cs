using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace SmoLVLcreator.Core;

internal sealed class SmoLevelSaveLog : IDisposable
{
    private static readonly object AppendGate = new();
    private readonly StreamWriter _writer;

    private SmoLevelSaveLog(StreamWriter writer, string filePath)
    {
        _writer = writer;
        FilePath = filePath;
    }

    public string FilePath { get; }

    public static string GetAdjacentPath(string outputPath) =>
        Path.GetFullPath(outputPath) + ".SmoLVLcreator.log";

    public static SmoLevelSaveLog Create(string outputPath)
    {
        string adjacentPath = GetAdjacentPath(outputPath);
        try
        {
            return Open(adjacentPath);
        }
        catch (Exception adjacentFailure) when (
            adjacentFailure is UnauthorizedAccessException or IOException or
                NotSupportedException)
        {
            string fallbackDirectory = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "SparkplugEngineResearch",
                "SmoLVLcreator",
                "Logs");
            string fallbackPath = Path.Combine(
                fallbackDirectory,
                $"{Path.GetFileName(outputPath)}-{DateTime.Now:yyyyMMdd-HHmmss-fff}.log");
            SmoLevelSaveLog log = Open(fallbackPath);
            log.Warning(
                "LOG_FALLBACK",
                $"Could not create the log beside the output SMO. " +
                $"requested={adjacentPath}; error={adjacentFailure}");
            return log;
        }
    }

    public static void Append(
        string filePath,
        string level,
        string category,
        string message)
    {
        lock (AppendGate)
        {
            using var stream = new FileStream(
                Path.GetFullPath(filePath),
                FileMode.Append,
                FileAccess.Write,
                FileShare.ReadWrite | FileShare.Delete);
            using var writer = new StreamWriter(
                stream,
                new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
            writer.WriteLine(FormatLine(level, category, message));
        }
    }

    public void Info(string category, string message) =>
        Write("INFO", category, message);

    public void Warning(string category, string message) =>
        Write("WARN", category, message);

    public void Error(string category, Exception exception) =>
        Write("ERROR", category, exception.ToString());

    public void Start(string sourcePath, string outputPath)
    {
        Write(
            "SESSION",
            "START",
            $"SmoLVLcreator save started. version={GetVersion()}; " +
            $"process={Environment.ProcessId}; thread={Environment.CurrentManagedThreadId}; " +
            $"architecture={RuntimeInformation.ProcessArchitecture}; " +
            $"os={Environment.OSVersion}; runtime={Environment.Version}");
        Info("PATHS", $"source={sourcePath}; output={outputPath}; log={FilePath}");
        Info(
            "COMMAND_LINE",
            string.Join(" ", Environment.GetCommandLineArgs().Select(Quote)));
    }

    public void Dispose()
    {
        try
        {
            _writer.Flush();
            _writer.Dispose();
        }
        catch (Exception exception)
        {
            Debug.WriteLine($"SmoLVLcreator log close failure: {exception}");
        }
    }

    private static SmoLevelSaveLog Open(string path)
    {
        string fullPath = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
        var stream = new FileStream(
            fullPath,
            FileMode.Create,
            FileAccess.Write,
            FileShare.ReadWrite | FileShare.Delete);
        var writer = new StreamWriter(
            stream,
            new UTF8Encoding(encoderShouldEmitUTF8Identifier: true))
        {
            AutoFlush = true
        };
        return new SmoLevelSaveLog(writer, fullPath);
    }

    private void Write(string level, string category, string message)
    {
        try
        {
            _writer.WriteLine(FormatLine(level, category, message));
        }
        catch (Exception exception)
        {
            Debug.WriteLine(
                $"SmoLVLcreator log failure: [{level}] [{category}] " +
                $"{message}\n{exception}");
        }
    }

    private static string FormatLine(string level, string category, string message)
    {
        string normalized = (message ?? string.Empty)
            .Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n')
            .Replace("\n", "\n    ", StringComparison.Ordinal);
        return
            $"{DateTimeOffset.Now:yyyy-MM-dd HH:mm:ss.fff zzz} " +
            $"[T{Environment.CurrentManagedThreadId:D2}] [{level}] [{category}] " +
            normalized;
    }

    private static string GetVersion() =>
        Assembly.GetEntryAssembly()?
            .GetCustomAttribute<AssemblyInformationalVersionAttribute>()?
            .InformationalVersion ??
        Assembly.GetEntryAssembly()?.GetName().Version?.ToString() ??
        "unknown";

    private static string Quote(string value) =>
        value.Any(char.IsWhiteSpace)
            ? $"\"{value.Replace("\"", "\\\"", StringComparison.Ordinal)}\""
            : value;
}
