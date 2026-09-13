using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace SmoNativeValidator.Core;

public sealed class NativeSessionLog : IDisposable
{
    private static readonly JsonSerializerOptions SerializerOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    private readonly StreamWriter _writer;
    private readonly object _gate = new();
    private bool _disposed;

    private NativeSessionLog(string filePath)
    {
        FilePath = Path.GetFullPath(filePath);
        string? directory = Path.GetDirectoryName(FilePath);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);
        _writer = new StreamWriter(
            new FileStream(FilePath, FileMode.Create, FileAccess.Write, FileShare.Read),
            new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
    }

    public string FilePath { get; }

    public static NativeSessionLog Create(string? filePath = null) =>
        new(filePath ?? GetDefaultLogFilePath());

    public void Write(NativeValidationEvent validationEvent)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        string line = SerializeLine(validationEvent);
        lock (_gate)
        {
            _writer.WriteLine(line);
            _writer.Flush();
        }
    }

    public static string SerializeLine(NativeValidationEvent validationEvent) =>
        JsonSerializer.Serialize(validationEvent, SerializerOptions);

    public static NativeValidationEvent DeserializeLine(string json) =>
        JsonSerializer.Deserialize<NativeValidationEvent>(json, SerializerOptions)
        ?? throw new JsonException("The log line did not contain an event.");

    public static string GetDefaultLogFilePath()
    {
        string root = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SparkplugEngineResearch",
            "SmoNativeValidator",
            "Logs");
        string name = $"native-{DateTime.UtcNow:yyyyMMdd-HHmmss}-{Guid.NewGuid():N}.jsonl";
        return Path.Combine(root, name);
    }

    public void Dispose()
    {
        if (_disposed)
            return;
        lock (_gate)
        {
            _disposed = true;
            _writer.Dispose();
        }
    }
}
