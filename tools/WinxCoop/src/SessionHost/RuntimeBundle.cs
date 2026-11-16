using System.Reflection;
using System.Reflection.PortableExecutable;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using WinxCoop.SessionRuntime;

namespace WinxCoop.SessionHost;

internal sealed record RuntimeFile(string Name, string Sha256, string Kind);
internal sealed record RuntimeBundleManifest(uint Version, string Architecture, string SourceProfileSha256, RuntimeFile[] Files);

// A local deployment manifest, not a network serializer. Retained read handles
// deny replacement between qualification and native/managed process creation.
internal sealed class RuntimeBundle : IDisposable
{
    private readonly List<FileStream> _pins = [];
    private readonly Dictionary<string, string> _files = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, string> _imagePins = new(StringComparer.OrdinalIgnoreCase);
    private readonly List<IntPtr> _modules = [];
    private bool _resolverInstalled, _disposed;
    internal string DirectoryPath { get; }
    internal RuntimeBundleManifest Manifest { get; }
    internal IReadOnlyDictionary<string, string> ImagePins => _imagePins;

    internal RuntimeBundle(string directory)
    {
        DirectoryPath = SessionConfiguration.BundleSourceDirectory(directory);
        Ordinary(DirectoryPath, true);
        try
        {
            string path = Path.Combine(DirectoryPath, "WinxCoop.SessionBundle.json");
            using var input = Pin(path);
            Manifest = JsonSerializer.Deserialize<RuntimeBundleManifest>(input) ?? throw new InvalidDataException("Missing bundle manifest.");
            string architecture = Environment.Is64BitProcess ? "x64" : "x86";
            Require(Manifest.Version == 1 && Manifest.Architecture == architecture && Digest(Manifest.SourceProfileSha256) &&
                Manifest.Files is { Length: > 0 and <= 512 }, "Bundle architecture, version or source profile is incompatible.");
            foreach (RuntimeFile row in Manifest.Files)
            {
                Require(row.Name.Length is > 0 and < 128 && row.Name == Path.GetFileName(row.Name) &&
                    !row.Name.Contains(':') && Digest(row.Sha256) && row.Kind is "native" or "managed" or "executable" or "data",
                    "Invalid bundle file descriptor.");
                string file = Path.Combine(DirectoryPath, row.Name);
                Require(!_files.ContainsKey(row.Name), "Duplicate bundle file descriptor.");
                var pin = Pin(file); _pins.Add(pin);
                using var sha = SHA256.Create();
                string actual = Convert.ToHexString(sha.ComputeHash(pin)); pin.Position = 0;
                Require(actual.Equals(row.Sha256, StringComparison.OrdinalIgnoreCase), "Bundle file hash differs: " + row.Name);
                if (row.Kind is "native" or "executable")
                {
                    using var pe = new PEReader(pin, PEStreamOptions.LeaveOpen);
                    var expected = Environment.Is64BitProcess ? System.Reflection.PortableExecutable.Machine.Amd64 : System.Reflection.PortableExecutable.Machine.I386;
                    Require(pe.PEHeaders.CoffHeader.Machine == expected || row.Name.Equals("XpGameProbe.exe", StringComparison.OrdinalIgnoreCase),
                        "Bundle PE architecture differs: " + row.Name);
                    pin.Position = 0;
                }
                _files.Add(row.Name, file);
                if (row.Kind == "executable") _imagePins.Add(file, actual);
            }
            foreach (string required in new[] { "WinxCoop.SessionHost.exe", "WinxCoop.ActorProbe.exe", "WinxCoop.ActorProbe.dll",
                "SmoNativeValidator.Core.dll", "WinxCoop.SessionRuntime.dll", "WinxCoopInputBridge.exe", "WinxCoopWorldBridge.exe",
                "WinxCoop.ProgressionNative.dll", "WinxCoop.PoseInterpolationNative.dll", "WinxCoop.SessionBootstrapNative.dll",
                "WinxCoop.SessionDiscoveryNative.dll", "XpGameProbe.exe" })
                _ = File(required);
        }
        catch { Dispose(); throw; }
    }

    internal string File(string name) => _files.TryGetValue(name, out string? path) ? path :
        throw new InvalidDataException("Required runtime component is absent from the manifest: " + name);

    internal string Hash(string name) => Manifest.Files.Single(row => row.Name.Equals(name, StringComparison.OrdinalIgnoreCase)).Sha256;

    internal void InstallNativeResolver()
    {
        Require(!_resolverInstalled && !_disposed, "Native bundle resolver is single-use.");
        _resolverInstalled = true;
        IntPtr Resolve(string name, Assembly assembly, DllImportSearchPath? search)
        {
            if (!name.StartsWith("WinxCoop.", StringComparison.Ordinal)) return IntPtr.Zero;
            string path = File(name);
            IntPtr module = NativeLibrary.Load(path);
            _modules.Add(module);
            return module;
        }
        NativeLibrary.SetDllImportResolver(typeof(ProgressionNative).Assembly, Resolve);
        NativeLibrary.SetDllImportResolver(typeof(RuntimeBundle).Assembly, Resolve);
    }

    internal static bool Digest(string text) => text is { Length: 64 } && text.All(Uri.IsHexDigit);
    internal static void Ordinary(string path, bool directory)
    {
        FileAttributes attributes = System.IO.File.GetAttributes(path);
        Require(((attributes & FileAttributes.Directory) != 0) == directory && (attributes & FileAttributes.ReparsePoint) == 0,
            "Runtime input must be an ordinary local file/directory: " + path);
    }
    private static FileStream Pin(string path)
    { Ordinary(path, false); return new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read); }
    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        // Native sessions and borrowed endpoints must be disposed by their
        // owner before releasing this bundle's module references/read pins.
        foreach (IntPtr module in _modules) NativeLibrary.Free(module);
        foreach (FileStream pin in _pins) pin.Dispose();
        _modules.Clear(); _pins.Clear();
    }
}
