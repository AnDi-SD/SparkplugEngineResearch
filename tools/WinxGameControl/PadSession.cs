using System.Diagnostics;
using System.Text.Json;
using Nefarius.ViGEm.Client;
using Nefarius.ViGEm.Client.Targets;
using Nefarius.ViGEm.Client.Targets.Xbox360;

namespace WinxGameControl;

public sealed record PadReport(short LeftThumbX = 0, short LeftThumbY = 0,
    short RightThumbX = 0, short RightThumbY = 0, byte LeftTrigger = 0,
    byte RightTrigger = 0, string[]? Buttons = null);

// One newly created controller on the already installed bus. No driver/service
// operation, focus manipulation or fallback to an arbitrary XInput user index.
public sealed class PadSession
{
    private readonly bool _createPad;
    private bool _started;
    private static readonly IReadOnlyDictionary<string, ushort> ButtonBits = new Dictionary<string, ushort>(StringComparer.OrdinalIgnoreCase)
    {
        ["Up"] = 0x0001, ["Down"] = 0x0002, ["Left"] = 0x0004, ["Right"] = 0x0008,
        ["Start"] = 0x0010, ["Back"] = 0x0020, ["LeftThumb"] = 0x0040, ["RightThumb"] = 0x0080,
        ["LeftShoulder"] = 0x0100, ["RightShoulder"] = 0x0200,
        ["A"] = 0x1000, ["B"] = 0x2000, ["X"] = 0x4000, ["Y"] = 0x8000
    };
    private ViGEmClient? _client;
    private IXbox360Controller? _pad;
    private bool _connected, _closed;
    private int? _userIndex;
    private Guid? _directInputInstance;
    private DirectInputPads.Pad[] _beforeDirectInput = [];
    private XInputReadback.Snapshot? _beforeXInput;
    private object? _startup, _cleanup, _lastNeutral, _lastVerificationFailure;
    public bool SafetyFailed { get; private set; }
    public string Stage { get; private set; } = "not-started";
    public static string[] SupportedButtons => ButtonBits.Keys.ToArray();
    public int? UserIndex => _userIndex;
    public object? Startup => _startup;
    public object? Cleanup => _cleanup;
    public object? LastVerificationFailure => _lastVerificationFailure;
    public bool PadEnabled => _createPad;
    public string Mode => _createPad ? "owned-pad" : "no-owned-pad";

    public PadSession(bool createPad = true) => _createPad = createPad;

    private object NoOwnedPadReceipt(string boundary) => new
    {
        mode = Mode, boundary, deviceCreated = false, reportSubmitted = false,
        ownedPadNeutralityApplicable = false, exactZeroClaimed = false,
        raw = XInputReadback.Read(boundary), observationOnly = true
    };

    public async Task StartAsync(string selectedGame, CancellationToken cancellationToken)
    {
        if (_started || _client is not null || _closed) throw new InvalidOperationException("Pad session start is single-use.");
        _started = true;
        if (!Path.IsPathFullyQualified(selectedGame) ||
            !string.Equals(Path.GetFileName(selectedGame), "WinxClub.exe", StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("Startup requires the selected absolute WinxClub.exe path.", nameof(selectedGame));
        selectedGame = Path.GetFullPath(selectedGame);
        if (!_createPad)
        {
            cancellationToken.ThrowIfCancellationRequested();
            Stage = "ready-no-owned-pad";
            _startup = NoOwnedPadReceipt("no-owned-pad-startup");
            return;
        }
        Stage = "startup-baseline";
        object gameAbsentBeforeConnect = RequireSelectedGameAbsent(selectedGame);
        _beforeDirectInput = DirectInputPads.Read();
        _beforeXInput = XInputReadback.Read("before-connect");
        if (!_beforeXInput.AbiVerified || _beforeXInput.Slots.Any(slot => slot.ErrorCode is not (0 or 1167)))
            throw new InvalidOperationException("No complete XInput baseline; controller identity cannot be established.");
        cancellationToken.ThrowIfCancellationRequested();
        Stage = "before-client-constructor";
        _client = new ViGEmClient();
        Stage = "before-create-controller";
        _pad = _client.CreateXbox360Controller();
        _pad.AutoSubmitReport = false;
        Stage = "before-connect";
        _pad.Connect(); _connected = true;
        Stage = "before-startup-zero-submit";
        Submit(new()); // Startup/cleanup zero is valid before a game is bound.

        Stage = "before-sdk-user-index";
        var timer = Stopwatch.StartNew(); Exception? indexError = null;
        while (timer.ElapsedMilliseconds < 2000)
        {
            cancellationToken.ThrowIfCancellationRequested();
            try
            {
                int index = _pad.UserIndex; // Actual1.21.256 SDK Int32 property.
                if (index is >= 0 and < 4) { _userIndex = index; break; }
            }
            catch (Exception exception) { indexError = exception; }
            await Task.Delay(40, cancellationToken);
        }
        if (_userIndex is not int own) throw new InvalidOperationException("ViGEm did not supply an owned XInput user index.", indexError);
        if (_beforeXInput.Slots[own].ErrorCode != 1167)
            throw new InvalidOperationException("SDK-assigned XInput slot was already occupied before this controller connected.");
        // ViGEmBus XusbPdo.cpp initializes the USB report blob with nonzero
        // sticks while its compared _Packet is zero. An identical zero submit
        // is skipped; first change the packet, then submit and verify zero.
        // The selected game must not exist until ready.json has been published.
        Stage = "before-startup-prime";
        object gameAbsentBeforePrime = RequireSelectedGameAbsent(selectedGame);
        object bootReadback = XInputReadback.Read("before-startup-prime");
        var primeReport = new PadReport(LeftThumbX: 1);
        object? prime = null, neutral = null;
        Exception? primeFailure = null;
        try
        {
            cancellationToken.ThrowIfCancellationRequested();
            Submit(primeReport);
            prime = await VerifyReportAsync(primeReport, "startup-exact-prime", 1200, null, cancellationToken);
        }
        catch (Exception exception) { primeFailure = exception; throw; }
        finally
        {
            Stage = "before-startup-exact-zero";
            try
            {
                Submit(new());
                neutral = await VerifyReportAsync(new(), "startup-exact-zero", 1200, null, CancellationToken.None);
            }
            catch (Exception cleanup)
            {
                SafetyFailed = true;
                throw primeFailure is null ? cleanup : new AggregateException("Startup prime and neutralization failed.", primeFailure, cleanup);
            }
        }
        Stage = "before-directinput-appearance";
        DirectInputPads.Pad[] during = [];
        timer.Restart();
        while (timer.ElapsedMilliseconds < 2000)
        {
            cancellationToken.ThrowIfCancellationRequested();
            during = DirectInputPads.Read();
            var added = during.Where(candidate => !_beforeDirectInput.Any(old => old.Instance == candidate.Instance)).ToArray();
            if (added.Length == 1) { _directInputInstance = added[0].Instance; break; }
            if (added.Length > 1) throw new InvalidOperationException("Multiple new DirectInput controllers appeared; ownership is ambiguous.");
            await Task.Delay(50, cancellationToken);
        }
        if (_directInputInstance is null) throw new InvalidOperationException("Owned controller did not appear in DirectInput GAMECTRL enumeration.");
        object readyNeutral = await VerifyReportAsync(new(), "ready-exact-zero", 500, null, cancellationToken);
        _lastNeutral = readyNeutral;
        _startup = new { sdkAssembly = typeof(ViGEmClient).Assembly.GetName().FullName,
            sdkUserIndex = own, userIndexSource = "IXbox360Controller.UserIndex", guessedIndex = false,
            directInputInstance = _directInputInstance, beforeDirectInput = _beforeDirectInput, duringDirectInput = during,
            beforeXInput = _beforeXInput, gameAbsentBeforeConnect, gameAbsentBeforePrime, bootReadback,
            startupPrime = prime, startupNeutral = neutral, exactNeutral = readyNeutral, autoSubmitReport = false,
            driverOrServiceOrSettingsChanged = false };
        Stage = "ready";
    }

    private static object RequireSelectedGameAbsent(string selectedGame)
    {
        foreach (Process candidate in Process.GetProcessesByName("WinxClub"))
        {
            using (candidate)
            {
                try
                {
                    if (candidate.HasExited) continue;
                    string? image = candidate.MainModule?.FileName;
                    if (image is null) throw new InvalidOperationException("Could not inspect an existing WinxClub process before startup prime.");
                    if (string.Equals(Path.GetFullPath(image), selectedGame, StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("The selected WinxClub.exe is already running; create and neutralize the controller before launching it.");
                }
                catch (InvalidOperationException) when (candidate.HasExited) { }
            }
        }
        return new { selectedGame, noRunningSelectedGame = true, utc = DateTime.UtcNow.ToString("o"),
            observationIsAtomicWithLaterSubmit = false };
    }

    private static ushort Mask(PadReport report)
    {
        string[] buttons = report.Buttons ?? [];
        if (buttons.Length > ButtonBits.Count || buttons.Any(string.IsNullOrWhiteSpace) ||
            buttons.Distinct(StringComparer.OrdinalIgnoreCase).Count() != buttons.Length)
            throw new InvalidDataException("Pad buttons must be a distinct supported list.");
        ushort value = 0;
        foreach (string name in buttons)
        {
            if (!ButtonBits.TryGetValue(name, out ushort bit)) throw new InvalidDataException("Unsupported pad button: " + name);
            value |= bit;
        }
        return value;
    }

    private void Submit(PadReport report)
    {
        if (_pad is null || !_connected || _closed) throw new InvalidOperationException("Owned pad is not connected.");
        ushort mask = Mask(report);
        _pad.ResetReport();
        _pad.SetButtonsFull(mask);
        _pad.SetAxisValue(Xbox360Axis.LeftThumbX, report.LeftThumbX);
        _pad.SetAxisValue(Xbox360Axis.LeftThumbY, report.LeftThumbY);
        _pad.SetAxisValue(Xbox360Axis.RightThumbX, report.RightThumbX);
        _pad.SetAxisValue(Xbox360Axis.RightThumbY, report.RightThumbY);
        _pad.SetSliderValue(Xbox360Slider.LeftTrigger, report.LeftTrigger);
        _pad.SetSliderValue(Xbox360Slider.RightTrigger, report.RightTrigger);
        _pad.SubmitReport();
    }

    private async Task<object> VerifyReportAsync(PadReport report, string boundary, int budgetMs,
        TargetIdentity? foregroundTarget, CancellationToken cancellationToken)
    {
        if (_pad is null || _userIndex is not int own) throw new InvalidOperationException("Owned pad index is not established.");
        ushort mask = Mask(report); XInputReadback.Snapshot? first = null, last = null;
        var timer = Stopwatch.StartNew(); int attempts = 0;
        do
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (foregroundTarget is not null) NativeDesktop.Validate(foregroundTarget, requireForeground: true);
            if (_pad.UserIndex != own) throw new InvalidOperationException("Owned controller SDK user index changed.");
            last = XInputReadback.Read(boundary); first ??= last; ++attempts;
            var slot = last.Slots[own];
            if (last.AbiVerified && slot.StateValid && slot.Buttons == mask &&
                slot.LeftTrigger == report.LeftTrigger && slot.RightTrigger == report.RightTrigger &&
                slot.LeftThumbX == report.LeftThumbX && slot.LeftThumbY == report.LeftThumbY &&
                slot.RightThumbX == report.RightThumbX && slot.RightThumbY == report.RightThumbY)
                return new { verified = true, sdkUserIndex = own, attempts, elapsedMs = timer.ElapsedMilliseconds,
                    requested = report, buttonMask = mask, first, last };
            await Task.Delay(25, cancellationToken);
        } while (timer.ElapsedMilliseconds < budgetMs);
        _lastVerificationFailure = new { verified = false, boundary, sdkUserIndex = own, attempts,
            elapsedMs = timer.ElapsedMilliseconds, budgetMs, requested = report, buttonMask = mask, first, last };
        // Preserve the raw mismatch in exception detail as well as Readback/Cleanup.
        // A startup failure occurs before ready.json and must remain diagnosable.
        throw new InvalidOperationException("Owned XInput report did not match the exact requested raw state within the bound. " +
            JsonSerializer.Serialize(_lastVerificationFailure));
    }

    public object Readback(string boundary) => new
    {
        mode = Mode, padEnabled = _createPad, connected = _connected, stage = Stage, sdkUserIndex = _userIndex, directInputInstance = _directInputInstance,
        raw = XInputReadback.Read(boundary), lastNeutral = _lastNeutral,
        lastVerificationFailure = _lastVerificationFailure, safetyFailed = SafetyFailed
    };

    public async Task<object> NeutralAsync(string boundary, CancellationToken cancellationToken)
    {
        if (!_createPad)
        {
            cancellationToken.ThrowIfCancellationRequested();
            return _lastNeutral = NoOwnedPadReceipt(boundary);
        }
        Submit(new());
        try { return _lastNeutral = await VerifyReportAsync(new(), boundary, 1200, null, cancellationToken); }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested) { throw; }
        catch { SafetyFailed = true; throw; }
    }

    public Task<object> VerifyNeutralAsync(string boundary, CancellationToken cancellationToken)
    {
        if (_createPad) return VerifyReportAsync(new(), boundary, 500, null, cancellationToken);
        cancellationToken.ThrowIfCancellationRequested();
        return Task.FromResult(NoOwnedPadReceipt(boundary));
    }

    public async Task<object> PulseAsync(TargetIdentity target, PadReport report,
        int holdMs, int neutralMs, CancellationToken cancellationToken)
    {
        if (!_createPad) throw new InvalidOperationException("Pad actions are unavailable in --no-pad mode; no controller is owned.");
        if (holdMs is < 50 or > 2000 || neutralMs is < 100 or > 1000)
            throw new InvalidDataException("Pad hold must be50..2000ms and neutral interval100..1000ms.");
        _ = Mask(report); NativeDesktop.Validate(target, requireForeground: true);
        object before = Readback("pad-before-action"), held;
        object? released = null; Exception? original = null;
        try
        {
            await NeutralAsync("pad-pre-action-zero", cancellationToken);
            NativeDesktop.Validate(target, requireForeground: true); cancellationToken.ThrowIfCancellationRequested();
            Submit(report);
            held = await VerifyReportAsync(report, "pad-held", 500, target, cancellationToken);
            await WaitForegroundAsync(target, holdMs, cancellationToken);
        }
        catch (Exception exception) { original = exception; throw; }
        finally
        {
            try { released = await NeutralAsync("pad-finally-zero", CancellationToken.None); }
            catch (Exception cleanup)
            {
                SafetyFailed = true;
                throw original is null ? cleanup : new AggregateException("Pad action and neutralization failed.", original, cleanup);
            }
        }
        await WaitForegroundAsync(target, neutralMs, cancellationToken);
        return new { before, requested = report, held, released, holdMs, neutralMs,
            simultaneousReport = true, after = Readback("pad-after-neutral-gap") };
    }

    public static async Task WaitForegroundAsync(TargetIdentity target, int milliseconds, CancellationToken cancellationToken)
    {
        var timer = Stopwatch.StartNew();
        do
        {
            cancellationToken.ThrowIfCancellationRequested(); NativeDesktop.Validate(target, requireForeground: true);
            if (timer.ElapsedMilliseconds >= milliseconds) return;
            await Task.Delay((int)Math.Max(1, Math.Min(10, milliseconds - timer.ElapsedMilliseconds)), cancellationToken);
        } while (true);
    }

    public async Task<object> CloseAsync()
    {
        if (_closed) return _cleanup ?? new { closed = true };
        string stageBeforeCleanup = Stage; Stage = "cleanup";
        if (!_createPad)
        {
            _closed = true; Stage = "closed";
            return _cleanup = new
            {
                success = true, mode = Mode, stageBeforeCleanup, deviceCreated = false,
                disconnectNeeded = false, disconnectPerformed = false,
                disposalNeeded = false, reportSubmitted = false, exactZeroClaimed = false
            };
        }
        var errors = new List<string>(); object? neutral = null;
        bool disconnected = !_connected, controllerDisposed = _pad is null, clientDisposed = _client is null;
        bool controllerDisposeAvailable = _pad is IDisposable, controllerDisposeCalled = false;
        if (_pad is not null)
        {
            if (_connected)
            {
                try { neutral = await NeutralAsync("cleanup-before-disconnect-zero", CancellationToken.None); }
                catch (Exception exception) { errors.Add("neutral: " + exception); }
                try { _pad.Disconnect(); _connected = false; disconnected = true; }
                catch (Exception exception) { errors.Add("disconnect: " + exception); }
            }
            try
            {
                if (_pad is IDisposable disposable)
                { disposable.Dispose(); controllerDisposeCalled = true; controllerDisposed = true; }
            }
            catch (Exception exception) { errors.Add("controller-dispose: " + exception); }
        }
        try { _client?.Dispose(); clientDisposed = true; }
        catch (Exception exception) { errors.Add("client-dispose: " + exception); }
        _closed = true;
        DirectInputPads.Pad[]? after = null; XInputReadback.Snapshot? xinputAfter = null;
        bool disappeared = false; var timer = Stopwatch.StartNew();
        do
        {
            try
            {
                after = DirectInputPads.Read(); xinputAfter = XInputReadback.Read("after-disconnect");
                disappeared = (_directInputInstance is null || after.All(item => item.Instance != _directInputInstance)) &&
                    (_userIndex is not int own || xinputAfter.Slots[own].ErrorCode == 1167);
                if (disappeared) break;
            }
            catch (Exception exception) { errors.Add("after-disconnect-observation: " + exception); break; }
            await Task.Delay(50);
        } while (timer.ElapsedMilliseconds < 1200);
        if (!disappeared) errors.Add("Owned controller disappearance was not verified.");
        bool success = disconnected && (controllerDisposed || !controllerDisposeAvailable) && clientDisposed && disappeared && errors.Count == 0;
        if (!success) SafetyFailed = true;
        Stage = success ? "closed" : "cleanup-failed";
        return _cleanup = new { success, stageBeforeCleanup, disconnected, controllerDisposed, controllerDisposeAvailable,
            controllerDisposeCalled, clientDisposed, disappeared,
            neutral, afterDirectInput = after, afterXInput = xinputAfter, sdkUserIndex = _userIndex,
            ownedDirectInputInstance = _directInputInstance, lastVerificationFailure = _lastVerificationFailure, errors };
    }
}
