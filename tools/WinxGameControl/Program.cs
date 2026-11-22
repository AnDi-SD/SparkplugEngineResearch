using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace WinxGameControl;

internal static class Program
{
    private const int ProtocolVersion = 1;
    private const int RebindNeutralMaxAgeMs = 60000;
    private static readonly JsonSerializerOptions Json = new()
    {
        WriteIndented = true, PropertyNameCaseInsensitive = true,
        NumberHandling = JsonNumberHandling.AllowReadingFromString,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow
    };
    private sealed record Request(int Version = 1, uint Sequence = 0, string Action = "",
        TargetIdentity? Target = null, PadReport? Pad = null, ushort Scan = 0, bool Extended = false,
        int Dx = 0, int Dy = 0, string? Button = null, int HoldMs = 150, int NeutralMs = 200);
    private sealed record ObservedAction(bool Successful, object? NativeResult, object? Error,
        object Before, IReadOnlyList<object> During, string DuringStatus, object After, long ElapsedMs);
    private sealed record CacheNeutrality(bool Qualified, bool? CurrentCacheNeutral,
        bool? CurrentAndPreviousCacheNeutral, bool FreshPollQualified, object Raw);
    private sealed record NeutralReceipt(TargetIdentity Target, string Utc, long Tick,
        object PadVerification, object WinxObservation, CacheNeutrality NativePadCache, object Desktop);

    private static object ExceptionInfo(Exception exception) => new
    {
        type = exception.GetType().FullName, exception.Message, exception.HResult,
        detail = exception.ToString()
    };

    private static string FullPath(string path)
    {
        if (string.IsNullOrWhiteSpace(path) || !Path.IsPathFullyQualified(path))
            throw new InvalidDataException("An explicit absolute path is required.");
        if (path.StartsWith(@"\\?\UNC\", StringComparison.OrdinalIgnoreCase)) path = @"\\" + path[8..];
        else if (path.StartsWith(@"\\?\", StringComparison.Ordinal)) path = path[4..];
        return Path.GetFullPath(path);
    }

    private static void WriteNew(string path, object value)
    {
        string pending = path + ".writing";
        using (var stream = new FileStream(pending, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
        {
            JsonSerializer.Serialize(stream, value, Json); stream.Flush(flushToDisk: true);
        }
        File.Move(pending, path, overwrite: false);
    }

    private static object Observe(TargetIdentity target, PadSession pad, string boundary)
    {
        object? desktop = null, native = null, api = null, padState = null;
        var errors = new List<object>();
        try { desktop = NativeDesktop.Readback(target); } catch (Exception error) { errors.Add(new { part = "desktop", error = ExceptionInfo(error) }); }
        try { native = WinxObservation.Read(target); } catch (Exception error) { errors.Add(new { part = "winx", error = ExceptionInfo(error) }); }
        try { api = NativeInput.Readback(); } catch (Exception error) { errors.Add(new { part = "nativeInput", error = ExceptionInfo(error) }); }
        try { padState = pad.Readback(boundary); } catch (Exception error) { errors.Add(new { part = "pad", error = ExceptionInfo(error) }); }
        return new { boundary, utc = DateTime.UtcNow.ToString("o"), tick = Environment.TickCount64,
            desktop, winx = native, nativeInput = api, pad = padState, errors,
            atomic = false, gameConsumptionClaimed = false };
    }

    private static async Task<ObservedAction> ObserveActionAsync(TargetIdentity target, PadSession pad,
        string action, int holdMs, Func<Task<object>> start, CancellationToken token)
    {
        object before = Observe(target, pad, action + "-before");
        var timer = Stopwatch.StartNew(); var during = new List<object>(3);
        object? nativeResult = null, failure = null;
        try
        {
            Task<object> running = start(); // Native Down/Submit happens before its first dwell await.
            int interval = Math.Clamp(holdMs / 3, 20, 100);
            for (int i = 0; i < 3; ++i)
            {
                Task winner = await Task.WhenAny(running, Task.Delay(interval, token));
                if (winner == running || running.IsCompleted || token.IsCancellationRequested) break;
                long elapsed = timer.ElapsedMilliseconds;
                during.Add(new { ordinal = i + 1, actionElapsedMs = elapsed,
                    requestedHoldWindowEstimate = elapsed < holdMs,
                    observation = Observe(target, pad, action + "-during-" + (i + 1)) });
            }
            nativeResult = await running;
        }
        catch (Exception error) { failure = ExceptionInfo(error); }
        object after = Observe(target, pad, action + "-after");
        return new(failure is null, nativeResult, failure, before, during.AsReadOnly(),
            during.Count == 0 ? "None: action completed or cancellation arrived before an in-flight sample" : "Observed while action task was in flight; inspect raw caches for actual Down/Up",
            after, timer.ElapsedMilliseconds);
    }

    private static TargetIdentity BindTarget(TargetIdentity? supplied, string selectedGame)
    {
        if (supplied is null) throw new InvalidDataException("Bind requires Target{Pid,CreatedFileTimeUtc,Hwnd,ImagePath}.");
        string path = FullPath(supplied.ImagePath);
        if (!string.Equals(path, selectedGame, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Target image path differs from the selected --game path.");
        var normalized = supplied with { ImagePath = selectedGame };
        NativeDesktop.Validate(normalized, requireForeground: false);
        return normalized;
    }

    private static Process TrackBoundProcess(TargetIdentity target)
    {
        Process process = Process.GetProcessById(target.Pid);
        try
        {
            // Retain this instance's native handle, so a normal exit can be
            // observed after the PID disappears without following a reused PID.
            _ = process.Handle;
            if (process.StartTime.ToUniversalTime().ToFileTimeUtc() != target.CreatedFileTimeUtc || process.HasExited)
                throw new InvalidOperationException("The selected game instance exited or changed before its lifetime could be tracked.");
            return process;
        }
        catch { process.Dispose(); throw; }
    }

    private static void RequireBoundIdentity(TargetIdentity bound, TargetIdentity? supplied, string selectedGame)
    {
        if (supplied is null) return;
        var normalized = BindTarget(supplied, selectedGame);
        if (normalized.Pid != bound.Pid || normalized.CreatedFileTimeUtc != bound.CreatedFileTimeUtc ||
            normalized.Hwnd != bound.Hwnd || !string.Equals(normalized.ImagePath, bound.ImagePath, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Action target differs from the immutable bound process/window instance.");
    }

    private static CacheNeutrality InspectPadCache(object native)
    {
        JsonElement value = JsonSerializer.SerializeToElement(native, Json);
        if (!value.TryGetProperty("pad", out JsonElement pad))
            return new(false, null, null, false, new { reason = "No native pad cache observation" });
        bool Flag(string name) => pad.TryGetProperty(name, out var flag) && flag.ValueKind == JsonValueKind.True;
        bool? NullableFlag(string name) => !pad.TryGetProperty(name, out var flag) || flag.ValueKind == JsonValueKind.Null
            ? null : flag.ValueKind == JsonValueKind.True ? true : flag.ValueKind == JsonValueKind.False ? false : null;
        return new(Flag("qualified"), NullableFlag("currentCacheNeutral"),
            NullableFlag("currentAndPreviousCacheNeutral"), Flag("freshPollQualified"), pad.Clone());
    }

    private static async Task<NeutralReceipt> CaptureNeutralReceiptAsync(TargetIdentity target,
        PadSession pad, CancellationToken token)
    {
        NativeDesktop.Validate(target, requireForeground: true);
        object verification = await pad.VerifyNeutralAsync(pad.PadEnabled ? "neutral-receipt-os-zero" : "neutral-receipt-no-owned-pad", token);
        object native = WinxObservation.Read(target); var cache = InspectPadCache(native);
        object desktop = NativeDesktop.Readback(target);
        NativeDesktop.Validate(target, requireForeground: true);
        if (pad.PadEnabled && cache.Qualified && cache.CurrentCacheNeutral != true)
            throw new InvalidOperationException("Owned OS pad is zero but Winx's qualified native pad cache is still nonzero; keep the old game foreground and inspect its actual polling.");
        return new(target, DateTime.UtcNow.ToString("o"), Environment.TickCount64,
            verification, native, cache, desktop);
    }

    private static Request ReadRequest(string path, uint next)
    {
        var info = new FileInfo(path);
        if ((info.Attributes & FileAttributes.ReparsePoint) != 0 || info.Length is < 1 or > 65536)
            throw new InvalidDataException("Request is a reparse point, empty or exceeds64KiB.");
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        var request = JsonSerializer.Deserialize<Request>(stream, Json) ?? throw new InvalidDataException("Missing request object.");
        if (request.Version != ProtocolVersion || request.Sequence != next || string.IsNullOrWhiteSpace(request.Action))
            throw new InvalidDataException("Request version, sequence or action is invalid.");
        return request;
    }

    private static async Task<int> Main(string[] args)
    {
        if (!OperatingSystem.IsWindows() || !Environment.Is64BitProcess || args.Length is not (5 or 6) || args[0] != "serve" ||
            (args.Length == 6 && args[5] != "--no-pad") ||
            args[3] != "--game" || !int.TryParse(args[2], out int seconds) || seconds is < 5 or > 1800)
        {
            Console.Error.WriteLine("Use WinxGameControl serve <NEW absolute session directory> <seconds5..1800> --game <full WinxClub.exe path> [--no-pad]; Windows x64/.NET8 only.");
            return 3;
        }
        string directory, selectedGame;
        try
        {
            directory = FullPath(args[1]); selectedGame = FullPath(args[4]);
            if (Directory.Exists(directory) || File.Exists(directory) || !File.Exists(selectedGame) ||
                !string.Equals(Path.GetFileName(selectedGame), "WinxClub.exe", StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Use a fresh session directory and an existing selected WinxClub.exe.");
            Directory.CreateDirectory(directory);
            if ((File.GetAttributes(directory) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException("Session directory must not be a reparse point.");
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 3; }

        DateTime deadlineUtc = DateTime.UtcNow.AddSeconds(seconds);
        using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(seconds));
        using var events = new FileStream(Path.Combine(directory, "events.jsonl"), FileMode.CreateNew, FileAccess.Write, FileShare.Read);
        void Log(string kind, object value)
        {
            JsonSerializer.Serialize(events, new { kind, utc = DateTime.UtcNow.ToString("o"), tick = Environment.TickCount64, value });
            events.WriteByte(10); events.Flush();
        }
        Console.CancelKeyPress += (_, e) => { e.Cancel = true; deadline.Cancel(); };
        var pad = new PadSession(createPad: args.Length == 5); TargetIdentity? bound = null; NeutralReceipt? lastNeutral = null;
        Process? boundProcess = null;
        object? fatal = null, cleanup = null, gameExit = null; string reason = "deadline";
        uint completed = 0; int succeeded = 0, rejected = 0; bool readyPublished = false, stopped = false;
        try
        {
            Log("startup", new { selectedGame, seconds, controllerMode = pad.Mode, driverInstallationRequested = false });
            await pad.StartAsync(selectedGame, deadline.Token);
            WriteNew(Path.Combine(directory, "ready.json"), new
            {
                version = ProtocolVersion, kind = "WinxGameControl-ready", pid = Environment.ProcessId,
                helperCreatedFileTimeUtc = Process.GetCurrentProcess().StartTime.ToUniversalTime().ToFileTimeUtc(),
                selectedGame, seconds, deadlineUtc = deadlineUtc.ToString("o"),
                controllerMode = pad.Mode, padEnabled = pad.PadEnabled,
                pad = pad.Startup,
                actions = pad.PadEnabled ? new[] { "Bind", "Pad", "Key", "Mouse", "CenterSystemCursor", "Snapshot", "Neutral", "Stop" }
                    : new[] { "Bind", "Key", "Mouse", "CenterSystemCursor", "Snapshot", "Neutral", "Stop" },
                buttons = pad.PadEnabled ? PadSession.SupportedButtons : Array.Empty<string>(),
                padHoldMs = pad.PadEnabled ? new[] { 50, 2000 } : Array.Empty<int>(), neutralMs = new[] { 100, 1000 },
                bindingPolicy = pad.PadEnabled
                    ? "verified PID/create-FILETIME/HWND/path; rebind requires explicit prior old-foreground Neutral acknowledgement and current owned OS zero"
                    : "verified PID/create-FILETIME/HWND/path; rebind requires explicit prior old-foreground Neutral acknowledgement; no owned pad or XInput zero claim",
                rebindNeutralMaxAgeMs = RebindNeutralMaxAgeMs,
                activationPerformed = false, settingsOrDriversChanged = false,
                nativeGamePadSelectionQualified = false, gameConsumptionClaimed = false
            });
            readyPublished = true; Log("ready", new { controllerMode = pad.Mode, connected = pad.PadEnabled, ownUserIndex = pad.UserIndex });
            long lastOwnerCheck = 0;
            while (!deadline.IsCancellationRequested)
            {
                if (bound is not null && Environment.TickCount64 - lastOwnerCheck >= 500)
                {
                    if (boundProcess?.HasExited == true)
                    {
                        gameExit = new { target = bound, exitCode = boundProcess.ExitCode, observedUtc = DateTime.UtcNow.ToString("o") };
                        reason = "game-exited"; Log("game-exited", gameExit); break;
                    }
                    try { NativeDesktop.Validate(bound, requireForeground: false); }
                    catch when (boundProcess?.HasExited == true)
                    {
                        gameExit = new { target = bound, exitCode = boundProcess.ExitCode, observedUtc = DateTime.UtcNow.ToString("o") };
                        reason = "game-exited"; Log("game-exited", gameExit); break;
                    }
                    lastOwnerCheck = Environment.TickCount64;
                }
                uint next = checked(completed + 1);
                if (next >= 10000) throw new InvalidOperationException("Session request bound9999 reached.");
                string requestPath = Path.Combine(directory, $"{next:D4}.json");
                if (!File.Exists(requestPath)) { await Task.Delay(50, deadline.Token); continue; }
                Request? request = null; object? detail = null, failure = null;
                bool successful = false;
                try
                {
                    request = ReadRequest(requestPath, next);
                    string action = request.Action.Trim().ToLowerInvariant();
                    if (action == "stop") { stopped = true; reason = "stop-request"; successful = true; }
                    else if (action == "bind")
                    {
                        var target = BindTarget(request.Target, selectedGame);
                        object? previousNative = null;
                        if (bound is not null && target != bound)
                        {
                            if (lastNeutral is null || lastNeutral.Target != bound ||
                                Environment.TickCount64 - lastNeutral.Tick > RebindNeutralMaxAgeMs)
                                throw new InvalidOperationException("Rebind requires a fresh explicit Neutral acknowledgement from the previous bound game while it was foreground.");
                            NativeDesktop.Validate(bound, requireForeground: false);
                            previousNative = WinxObservation.Read(bound);
                            var cache = InspectPadCache(previousNative);
                            if (pad.PadEnabled && cache.Qualified && cache.CurrentCacheNeutral != true)
                                throw new InvalidOperationException("Old Winx's qualified pad cache became nonzero after its Neutral acknowledgement.");
                            NativeDesktop.Validate(target, requireForeground: true);
                        }
                        object padVerification = await pad.VerifyNeutralAsync(pad.PadEnabled ? "bind-os-zero" : "bind-no-owned-pad", deadline.Token);
                        object? priorNeutral = lastNeutral;
                        Process nextProcess = TrackBoundProcess(target);
                        boundProcess?.Dispose(); boundProcess = nextProcess;
                        bound = target; lastNeutral = null;
                        detail = new { observation = Observe(bound, pad, "bound"), controllerMode = pad.Mode,
                            osZero = pad.PadEnabled ? padVerification : null, padVerification,
                            priorNeutral, previousNative, hotplugPerformed = false,
                            nativeFreshPollClaimed = false };
                        successful = true;
                    }
                    else
                    {
                        if (bound is null) throw new InvalidOperationException("Bind a verified game process/window before sending this action.");
                        TargetIdentity target = bound;
                        RequireBoundIdentity(target, request.Target, selectedGame);
                        NativeDesktop.Validate(target, requireForeground: action != "snapshot");
                        if (action == "snapshot") { detail = Observe(target, pad, "snapshot"); successful = true; }
                        else
                        {
                            if (request.HoldMs is < 50 or > 2000 || request.NeutralMs is < 100 or > 1000)
                                throw new InvalidDataException("HoldMs must be50..2000 and NeutralMs100..1000.");
                            if (action is "pad" or "key" or "mouse" or "centersystemcursor" or "neutral") lastNeutral = null;
                            Func<Task<object>> execute = action switch
                            {
                                "pad" => () => pad.PulseAsync(target, request.Pad ?? throw new InvalidDataException("Pad action requires a raw Pad report."), request.HoldMs, request.NeutralMs, deadline.Token),
                                "key" => async () =>
                                {
                                    await NativeInput.SendKeyAsync(target, request.Scan, request.Extended, request.HoldMs, request.NeutralMs, deadline.Token);
                                    return new { sentBy = "NativeInput.SendKeyAsync", request.Scan, request.Extended, api = NativeInput.Readback() };
                                },
                                "mouse" => async () =>
                                {
                                    await NativeInput.SendMouseAsync(target, request.Dx, request.Dy, request.Button, request.HoldMs, request.NeutralMs, deadline.Token);
                                    return new { sentBy = "NativeInput.SendMouseAsync", request.Dx, request.Dy, request.Button, relativeCounts = true, api = NativeInput.Readback() };
                                },
                                "centersystemcursor" => async () =>
                                {
                                    object centered = NativeDesktop.CenterSystemCursor(target);
                                    await PadSession.WaitForegroundAsync(target, request.NeutralMs, deadline.Token);
                                    return centered;
                                },
                                "neutral" => async () =>
                                {
                                    NativeDesktop.Validate(target, requireForeground: true);
                                    object release = await pad.NeutralAsync("explicit-neutral", deadline.Token);
                                    await PadSession.WaitForegroundAsync(target, request.NeutralMs, deadline.Token);
                                    return release;
                                },
                                _ => throw new InvalidDataException("Unsupported action; focus/activation and unbounded holds are not provided.")
                            };
                            ObservedAction receipt = await ObserveActionAsync(target, pad, action, request.HoldMs, execute, deadline.Token);
                            detail = receipt; successful = receipt.Successful;
                            if (!successful) failure = receipt.Error;
                            if (successful && action == "neutral")
                            {
                                lastNeutral = await CaptureNeutralReceiptAsync(target, pad, deadline.Token);
                                detail = new { action = receipt, neutralReceipt = lastNeutral, fullDirectInputFreshPollClaimed = false };
                            }
                        }
                    }
                }
                catch (Exception error) { successful = false; failure = ExceptionInfo(error); }
                if (successful) ++succeeded; else ++rejected;
                WriteNew(Path.Combine(directory, $"{next:D4}-result.json"), new
                {
                    version = ProtocolVersion, sequence = next, request, successful, error = failure, detail,
                    boundTarget = bound, lastNeutralReceipt = lastNeutral, completedUtc = DateTime.UtcNow.ToString("o"),
                    padReadback = pad.Readback("result"), gameConsumptionClaimed = false
                });
                completed = next; Log("request-complete", new { sequence = completed, action = request?.Action, successful });
                if (stopped) break;
                if (pad.SafetyFailed) throw new InvalidOperationException("Pad exact-neutral safety check failed; session is ending.");
            }
        }
        catch (OperationCanceledException) when (deadline.IsCancellationRequested) { reason = "deadline-or-cancel"; }
        catch (Exception error) { fatal = ExceptionInfo(error); reason = "error"; }
        finally
        {
            try { cleanup = await pad.CloseAsync(); }
            catch (Exception error) { fatal ??= ExceptionInfo(error); cleanup = new { success = false, error = ExceptionInfo(error) }; }
            Log("cleanup", cleanup ?? new { success = false });
            WriteNew(Path.Combine(directory, "final.json"), new
            {
                version = ProtocolVersion, kind = "WinxGameControl-final", pid = Environment.ProcessId,
                selectedGame, controllerMode = pad.Mode, padEnabled = pad.PadEnabled,
                boundTarget = bound, readyPublished, completed, succeeded, rejected, stopped,
                reason, gameExit, fatal, cleanup, padSafetyFailed = pad.SafetyFailed,
                driverOrSettingsChanged = false, activationPerformed = false, gameConsumptionClaimed = false
            });
            boundProcess?.Dispose();
        }
        return fatal is null && !pad.SafetyFailed && readyPublished ? 0 : 4;
    }
}
