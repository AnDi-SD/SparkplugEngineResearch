using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

namespace WinxGameControl;

public sealed record TargetIdentity(int Pid, long CreatedFileTimeUtc, long Hwnd, string ImagePath);

/// <summary>Identity/focus boundary and explicit system-cursor placement for one selected game.</summary>
public static class NativeDesktop
{
    private const uint QueryLimitedInformation = 0x1000, TokenQuery = 8, RootAncestor = 2;

    public static void Validate(TargetIdentity target, bool requireForeground)
    {
        ArgumentNullException.ThrowIfNull(target);
        if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException("Winx input requires Windows.");
        if (target.Pid <= 0 || target.CreatedFileTimeUtc <= 0 || target.Hwnd == 0 ||
            string.IsNullOrWhiteSpace(target.ImagePath) || !Path.IsPathFullyQualified(target.ImagePath))
            throw new ArgumentException("A selected PID, creation FILETIME, root HWND and absolute image path are required.", nameof(target));
        string expected = NormalizePath(target.ImagePath);
        if (!string.Equals(Path.GetFileName(expected), "WinxClub.exe", StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("The selected image must be WinxClub.exe.", nameof(target));
        using SafeProcessHandle process = OpenSelectedProcess(target.Pid);
        if (!GetProcessTimes(process, out NativeFileTime created, out _, out _, out _)) ThrowLastError("Read target creation time");
        if (created.Value != target.CreatedFileTimeUtc) throw new InvalidOperationException("The selected game PID was reused or its creation time differs.");
        if (!GetExitCodeProcess(process, out uint exitCode)) ThrowLastError("Read target lifetime");
        if (exitCode != 259) throw new InvalidOperationException("The selected game has exited.");
        var path = new StringBuilder(32768); uint pathLength = (uint)path.Capacity;
        if (!QueryFullProcessImageName(process, 0, path, ref pathLength)) ThrowLastError("Read target image path");
        if (!string.Equals(expected, NormalizePath(path.ToString()), StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("The live game image differs from the selected full image path.");
        if (!ProcessIdToSessionId((uint)target.Pid, out uint targetSession)) ThrowLastError("Read target desktop session");
        if (!ProcessIdToSessionId((uint)Environment.ProcessId, out uint ownSession)) ThrowLastError("Read controller desktop session");
        if (targetSession != ownSession) throw new InvalidOperationException("The game is in a different Windows session.");
        nint window = Window(target);
        uint thread = GetWindowThreadProcessId(window, out uint owner);
        if (!IsWindow(window) || thread == 0 || owner != (uint)target.Pid || GetAncestor(window, RootAncestor) != window)
            throw new InvalidOperationException("The selected HWND is no longer the game's owned root window.");
        if (requireForeground && (GetForegroundWindow() != window || !IsWindowVisible(window) || IsIconic(window)))
            throw new InvalidOperationException("The selected game root window must already be foreground, visible and restored; this tool does not activate it.");
    }

    /// <summary>OS evidence only; a successful input send is not evidence of game consumption.</summary>
    public static object Readback(TargetIdentity target)
    {
        Validate(target, false);
        nint window = Window(target), foreground = GetForegroundWindow();
        uint thread = GetWindowThreadProcessId(window, out _);
        GuiThreadInfo gui = new() { Size = (uint)Marshal.SizeOf<GuiThreadInfo>() };
        bool guiRead = GetGUIThreadInfo(thread, ref gui);
        bool cursorRead = GetCursorPos(out Point cursor);
        bool clientRead = TryClientScreenRect(window, out Rect client);
        bool inside = cursorRead && clientRead && Inside(cursor, client);
        nint underCursor = cursorRead ? WindowFromPoint(cursor) : 0;
        var title = new StringBuilder(1024); GetWindowText(window, title, title.Capacity);
        using SafeProcessHandle process = OpenSelectedProcess(target.Pid);
        using SafeProcessHandle controller = OpenSelectedProcess(Environment.ProcessId);
        int? targetIntegrity = ReadIntegrity(process, out string? targetIntegrityError);
        int? controllerIntegrity = ReadIntegrity(controller, out string? controllerIntegrityError);
        return new
        {
            target.Pid, target.CreatedFileTimeUtc, target.ImagePath, hwnd = target.Hwnd,
            title = title.ToString(), foregroundHwnd = foreground.ToInt64(),
            foregroundIsSelectedRoot = foreground == window,
            visible = IsWindowVisible(window), minimized = IsIconic(window), thread,
            guiThreadRead = guiRead, captureHwnd = guiRead ? gui.Capture.ToInt64() : (long?)null,
            captureIsSelectedRoot = guiRead && gui.Capture == window,
            cursorRead, cursor = cursorRead ? new { x = cursor.X, y = cursor.Y } : null,
            clientScreenRead = clientRead,
            clientScreen = clientRead ? new { left = client.Left, top = client.Top, right = client.Right, bottom = client.Bottom } : null,
            cursorInsideClient = inside,
            cursorRootHwnd = underCursor != 0 ? GetAncestor(underCursor, RootAncestor).ToInt64() : (long?)null,
            asyncDown = new { enter = IsDown(0x0D), escape = IsDown(0x1B), space = IsDown(0x20),
                w = IsDown(0x57), a = IsDown(0x41), s = IsDown(0x53), d = IsDown(0x44),
                shift = IsDown(0x10), control = IsDown(0x11), alt = IsDown(0x12),
                leftWindows = IsDown(0x5B), rightWindows = IsDown(0x5C), leftMouse = IsDown(1), rightMouse = IsDown(2) },
            targetIntegrityRid = targetIntegrity, controllerIntegrityRid = controllerIntegrity,
            targetIntegrityError, controllerIntegrityError,
            targetHigherIntegrity = targetIntegrity.HasValue && controllerIntegrity.HasValue
                ? targetIntegrity > controllerIntegrity : (bool?)null,
            systemInputIsGlobal = true, gameConsumptionClaimed = false, sendInput = NativeInput.Readback()
        };
    }

    public static object CenterSystemCursor(TargetIdentity target)
    {
        Validate(target, true);
        GuardModifiers();
        GuardMouseReleased(false); GuardMouseReleased(true);
        if (!TryClientScreenRect(Window(target), out Rect client) || client.Right <= client.Left || client.Bottom <= client.Top)
            throw new InvalidOperationException("The selected game client rectangle cannot be verified.");
        int x = client.Left + (client.Right - client.Left) / 2;
        int y = client.Top + (client.Bottom - client.Top) / 2;
        nint underCenter = WindowFromPoint(new Point { X = x, Y = y });
        if (underCenter == 0 || GetAncestor(underCenter, RootAncestor) != Window(target))
            throw new InvalidOperationException("The selected game's client center is obscured.");
        if (!SetCursorPos(x, y)) ThrowLastError("Center system cursor in selected game");
        Validate(target, true);
        return new { method = "SetCursorPos", x, y, buttonEventSent = false,
            gameCursorPositionClaimed = false };
    }

    internal static void GuardMouseButton(TargetIdentity target)
    {
        Validate(target, true);
        GuardModifiers();
        nint window = Window(target);
        uint thread = GetWindowThreadProcessId(window, out _);
        GuiThreadInfo gui = new() { Size = (uint)Marshal.SizeOf<GuiThreadInfo>() };
        if (!GetGUIThreadInfo(thread, ref gui)) ThrowLastError("Read game mouse capture");
        if (gui.Capture == window) return;
        if (!GetCursorPos(out Point cursor) || !TryClientScreenRect(window, out Rect client))
            throw new InvalidOperationException("The OS cursor/client rectangle cannot be verified before a game button event.");
        nint underCursor = WindowFromPoint(cursor);
        if (!Inside(cursor, client) || underCursor == 0 || GetAncestor(underCursor, RootAncestor) != window)
            throw new InvalidOperationException("A game mouse button requires capture by its root or an unobscured OS cursor inside its verified client rectangle.");
    }

    internal static void GuardNoWindowsKey()
    {
        if (IsDown(0x5B) || IsDown(0x5C))
            throw new InvalidOperationException("Release physical Windows/Super keys before sending global game input.");
    }

    internal static void GuardModifiers(ushort? ownedScan = null, bool extended = false)
    {
        GuardNoWindowsKey();
        int owned = ownedScan switch
        {
            0x1D => extended ? 0xA3 : 0xA2,
            0x2A when !extended => 0xA0,
            0x36 when !extended => 0xA1,
            0x38 => extended ? 0xA5 : 0xA4,
            _ => 0
        };
        // Only an already-admitted key's hold phase may ignore its own side.
        // In particular, foreign Alt+Return must never toggle the game's display mode.
        for (int key = 0xA0; key <= 0xA5; ++key)
            if (key != owned && IsDown(key))
                throw new InvalidOperationException("Release foreign left/right Shift, Ctrl and Alt modifiers before sending unchorded game input.");
    }

    internal static void GuardKeyReleased(ushort scan, bool extended)
    {
        // Mapping is only an OS readback guard. Injection always uses wVk=0 and scan codes.
        uint virtualKey = MapVirtualKey((uint)scan | (extended ? 0xE000u : 0u), 3);
        if (virtualKey == 0) throw new ArgumentOutOfRangeException(nameof(scan), "Windows does not recognize this scan code.");
        if (IsDown((int)virtualKey)) throw new InvalidOperationException("The requested physical key is already down; the tool will not take ownership of a user's held key.");
    }

    internal static void GuardMouseReleased(bool right)
    {
        if (IsDown(right ? 2 : 1)) throw new InvalidOperationException("The requested mouse button is already down; release it before starting an owned pulse.");
    }

    private static SafeProcessHandle OpenSelectedProcess(int pid)
    {
        SafeProcessHandle result = OpenProcess(QueryLimitedInformation, false, (uint)pid);
        if (result.IsInvalid)
        {
            int error = Marshal.GetLastWin32Error(); result.Dispose();
            throw new Win32Exception(error, "Open selected process for read-only identity");
        }
        return result;
    }

    private static int? ReadIntegrity(SafeProcessHandle process, out string? error)
    {
        error = null;
        if (!OpenProcessToken(process, TokenQuery, out SafeAccessTokenHandle token))
        { error = new Win32Exception(Marshal.GetLastWin32Error()).Message; return null; }
        using (token)
        {
            _ = GetTokenInformation(token, 25, 0, 0, out uint bytes);
            if (bytes == 0 || bytes > 4096) { error = "Token integrity size is unavailable or outside its bound."; return null; }
            nint buffer = Marshal.AllocHGlobal((int)bytes);
            try
            {
                if (!GetTokenInformation(token, 25, buffer, bytes, out _))
                { error = new Win32Exception(Marshal.GetLastWin32Error()).Message; return null; }
                nint sid = Marshal.ReadIntPtr(buffer);
                if (sid == 0 || !IsValidSid(sid)) { error = "Token integrity SID is invalid."; return null; }
                nint countAddress = GetSidSubAuthorityCount(sid);
                if (countAddress == 0) { error = "Token integrity SID count is unavailable."; return null; }
                byte count = Marshal.ReadByte(countAddress);
                if (count == 0 || count > 15) { error = "Token integrity SID count is outside its bound."; return null; }
                nint rid = GetSidSubAuthority(sid, (uint)(count - 1));
                if (rid == 0) { error = "Token integrity RID is unavailable."; return null; }
                return Marshal.ReadInt32(rid);
            }
            finally { Marshal.FreeHGlobal(buffer); }
        }
    }

    private static bool TryClientScreenRect(nint window, out Rect screen)
    {
        screen = default;
        if (!GetClientRect(window, out Rect client)) return false;
        Point first = new() { X = client.Left, Y = client.Top }, last = new() { X = client.Right, Y = client.Bottom };
        if (!ClientToScreen(window, ref first) || !ClientToScreen(window, ref last)) return false;
        screen = new Rect { Left = first.X, Top = first.Y, Right = last.X, Bottom = last.Y };
        return screen.Right > screen.Left && screen.Bottom > screen.Top;
    }

    private static bool Inside(Point point, Rect rect) => point.X >= rect.Left && point.X < rect.Right && point.Y >= rect.Top && point.Y < rect.Bottom;
    private static nint Window(TargetIdentity target) => new(target.Hwnd);
    private static bool IsDown(int key) => (GetAsyncKeyState(key) & 0x8000) != 0;
    private static string NormalizePath(string path)
    {
        string result = Path.GetFullPath(path);
        return result.StartsWith(@"\\?\", StringComparison.Ordinal) ? result[4..] : result;
    }
    private static void ThrowLastError(string operation) => throw new Win32Exception(Marshal.GetLastWin32Error(), operation);

    [StructLayout(LayoutKind.Sequential)] private struct NativeFileTime
    { public uint Low, High; public readonly long Value => ((long)High << 32) | Low; }
    [StructLayout(LayoutKind.Sequential)] private struct Point { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] private struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct GuiThreadInfo
    {
        public uint Size, Flags;
        public nint Active, Focus, Capture, MenuOwner, MoveSize, Caret;
        public Rect CaretRect;
    }

    [DllImport("kernel32.dll", SetLastError = true)] private static extern SafeProcessHandle OpenProcess(uint access, [MarshalAs(UnmanagedType.Bool)] bool inherit, uint pid);
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetProcessTimes(SafeProcessHandle process, out NativeFileTime creation, out NativeFileTime exit, out NativeFileTime kernel, out NativeFileTime user);
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetExitCodeProcess(SafeProcessHandle process, out uint exitCode);
    [DllImport("kernel32.dll", EntryPoint = "QueryFullProcessImageNameW", CharSet = CharSet.Unicode, SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool QueryFullProcessImageName(SafeProcessHandle process, uint flags, StringBuilder path, ref uint size);
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool ProcessIdToSessionId(uint pid, out uint session);
    [DllImport("user32.dll")] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool IsWindow(nint window);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(nint window, out uint pid);
    [DllImport("user32.dll")] private static extern nint GetAncestor(nint window, uint flags);
    [DllImport("user32.dll")] private static extern nint GetForegroundWindow();
    [DllImport("user32.dll")] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool IsWindowVisible(nint window);
    [DllImport("user32.dll")] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool IsIconic(nint window);
    [DllImport("user32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetGUIThreadInfo(uint thread, ref GuiThreadInfo info);
    [DllImport("user32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetClientRect(nint window, out Rect rect);
    [DllImport("user32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool ClientToScreen(nint window, ref Point point);
    [DllImport("user32.dll")] private static extern nint WindowFromPoint(Point point);
    [DllImport("user32.dll", EntryPoint = "GetWindowTextW", CharSet = CharSet.Unicode)] private static extern int GetWindowText(nint window, StringBuilder text, int capacity);
    [DllImport("user32.dll")] private static extern short GetAsyncKeyState(int key);
    [DllImport("user32.dll", EntryPoint = "MapVirtualKeyW")] private static extern uint MapVirtualKey(uint code, uint type);
    [DllImport("advapi32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool OpenProcessToken(SafeProcessHandle process, uint access, out SafeAccessTokenHandle token);
    [DllImport("advapi32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetTokenInformation(SafeAccessTokenHandle token, int kind, nint data, uint length, out uint needed);
    [DllImport("advapi32.dll")] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool IsValidSid(nint sid);
    [DllImport("advapi32.dll")] private static extern nint GetSidSubAuthorityCount(nint sid);
    [DllImport("advapi32.dll")] private static extern nint GetSidSubAuthority(nint sid, uint index);
}
