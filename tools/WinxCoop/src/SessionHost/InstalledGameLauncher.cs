using System.Diagnostics;
using System.Security.Cryptography;

namespace WinxCoop.SessionHost;

internal sealed record InstalledGameLaunch(string GameRoot, string Executable, int ProcessId, string? DiagnosticSha256);

// Ordinary process launch, independent of the mod's bundle, profiles and workers.
// Retained Process objects observe exits; disposing them never terminates a game.
internal sealed class InstalledGameLauncher(LauncherLocalState local) : IDisposable
{
    private sealed record RunningGame(Process Process, InstalledGameLaunch Launch);
    private readonly List<RunningGame> _games = [];
    private bool _disposed;

    internal InstalledGameLaunch Launch(string selectedPath)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(InstalledGameLauncher));
        string selected = selectedPath.Trim();
        if (selected.Length == 0 || !Path.IsPathFullyQualified(selected))
            throw new InvalidDataException("Выберите локальную папку установленной игры или WinxClub.exe.");
        string full = Path.GetFullPath(selected);
        string executable = Directory.Exists(full) ? Path.Combine(full, "WinxClub.exe") : full;
        if (!Path.GetFileName(executable).Equals("WinxClub.exe", StringComparison.OrdinalIgnoreCase) || !File.Exists(executable))
            throw new FileNotFoundException("В выбранной папке не найден WinxClub.exe.", executable);
        string directory = Path.GetDirectoryName(executable) ?? throw new InvalidDataException("Не найдена папка игры.");
        string drive = Path.GetPathRoot(directory) ?? throw new InvalidDataException("Требуется локальная папка игры.");
        if (directory.StartsWith(@"\\", StringComparison.Ordinal) ||
            new DriveInfo(drive).DriveType is DriveType.Network or DriveType.Unknown or DriveType.NoRootDirectory)
            throw new InvalidDataException("Выберите игру на локальном диске компьютера.");

        string? hash = null, hashError = null;
        try
        {
            using var input = new FileStream(executable, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            using var sha = SHA256.Create();
            hash = Convert.ToHexString(sha.ComputeHash(input));
        }
        catch (Exception error) { hashError = error.Message; } // A diagnostic hash never admits or rejects a version.
        local.TryWrite("ordinary-game-launch-requested", new { executable, workingDirectory = directory,
            diagnosticSha256 = hash, hashError, hashIsVersionGate = false, arguments = "", modAttached = false });
        // No arguments, environment changes, registry/media broker, debugger,
        // process job or lifetime limit: the selected installation starts itself.
        Process process = Process.Start(new ProcessStartInfo
        {
            FileName = executable, WorkingDirectory = directory, Arguments = "", UseShellExecute = true
        }) ?? throw new IOException("Windows не запустила WinxClub.exe.");
        try
        {
            var launched = new InstalledGameLaunch(directory, executable, process.Id, hash);
            _games.Add(new(process, launched));
            local.TryWrite("ordinary-game-started", new { launched.GameRoot, launched.Executable, launched.ProcessId,
                launched.DiagnosticSha256, hashIsVersionGate = false, modAttached = false, forcedLifetimeLimit = false });
            return launched;
        }
        catch { process.Dispose(); throw; }
    }

    // Called by the existing GUI pump, with no background timer or game input.
    internal void Poll()
    {
        if (_disposed) return;
        for (int index = _games.Count - 1; index >= 0; --index)
        {
            RunningGame game = _games[index];
            try
            {
                if (!game.Process.HasExited) continue;
                local.TryWrite("ordinary-game-exited", new { game.Launch.Executable, game.Launch.ProcessId,
                    game.Launch.DiagnosticSha256, exitCode = game.Process.ExitCode, actualExitObserved = true });
            }
            catch (Exception error)
            {
                local.TryWrite("ordinary-game-observation-failed", new { game.Launch.Executable, game.Launch.ProcessId,
                    error = error.ToString(), exitCode = (int?)null, gameTerminationRequested = false });
            }
            _games.RemoveAt(index); game.Process.Dispose();
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        Poll(); _disposed = true;
        foreach (RunningGame game in _games)
        {
            local.TryWrite("ordinary-game-observation-ended", new { game.Launch.Executable, game.Launch.ProcessId,
                game.Launch.DiagnosticSha256, exitCode = (int?)null, actualExitObserved = false,
                reason = "launcher-closed", gameTerminationRequested = false });
            game.Process.Dispose(); // Close our handle only; the game remains independent.
        }
        _games.Clear();
    }
}
