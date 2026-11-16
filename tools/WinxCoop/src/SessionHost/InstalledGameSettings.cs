using System.Globalization;
using System.Security.Cryptography;
using System.Text;

namespace WinxCoop.SessionHost;

internal sealed record InstalledGameSetupReceipt(string IniPath, string BackupPath, string BeforeSha256, string AfterSha256, bool Changed);

// An explicit UI action only. Selecting or starting a session never calls this
// writer. Every setting except the sectionless startLevel value is preserved.
internal static class InstalledGameSettings
{
    private static readonly UTF8Encoding StrictUtf8 = new(false, true);

    internal static InstalledGameSetupReceipt ConfigureCampaign(string gameRoot, LauncherLocalState local)
    {
        string root = SessionConfiguration.AbsoluteAscii(gameRoot);
        RuntimeBundle.Ordinary(root, true);
        RuntimeBundle.Ordinary(Path.Combine(root, "Media"), true);
        RuntimeBundle.Ordinary(Path.Combine(root, "Shaders"), true);
        string executable = Path.Combine(root, "WinxClub.exe");
        RuntimeBundle.Ordinary(executable, false);
        using var executablePin = new FileStream(executable, FileMode.Open, FileAccess.Read, FileShare.Read);
        Require(GameProfile.Hash(executablePin) == GameProfile.OriginalExecutableSha256, "Эта версия WinxClub.exe не поддерживается.");
        string ini = Path.Combine(root, "winx.ini");
        RuntimeBundle.Ordinary(ini, false);
        // Retain the original file while reading, backing up and changing it.
        // A read-only installation fails here without changing permissions.
        using var file = new FileStream(ini, FileMode.Open, FileAccess.ReadWrite, FileShare.Read);
        byte[] before = ReadBounded(file);
        byte[] after = SetStartLevelOne(before); // All format checks precede any write.
        string backupDirectory = Path.Combine(local.RootDirectory, "Backups");
        LauncherLocalState.EnsureLocalDirectory(backupDirectory);
        string backup = Path.Combine(backupDirectory, "winx-" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + "-" + Guid.NewGuid().ToString("N") + ".ini");
        using (var copy = new FileStream(backup, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
        {
            copy.Write(before, 0, before.Length); copy.Flush(flushToDisk: true);
        }
        Require(System.IO.File.ReadAllBytes(backup).AsSpan().SequenceEqual(before), "Не удалось проверить точную резервную копию winx.ini.");
        string beforeSha = Convert.ToHexString(SHA256.HashData(before));
        string afterSha = Convert.ToHexString(SHA256.HashData(after));
        local.Write("game-settings-backup", new { iniPath = ini, backupPath = backup, beforeSha256 = beforeSha });
        bool changed = !before.AsSpan().SequenceEqual(after);
        try
        {
            if (changed)
            {
                file.Position = 0; file.Write(after, 0, after.Length); file.SetLength(after.Length); file.Flush(flushToDisk: true);
            }
            file.Position = 0;
            Require(ReadBounded(file).AsSpan().SequenceEqual(after), "Не удалось проверить записанный winx.ini.");
        }
        catch (Exception error)
        {
            local.TryWrite("game-settings-write-failed", new { iniPath = ini, backupPath = backup, writeAttempted = changed, error = error.ToString() });
            throw new IOException("Не удалось завершить настройку winx.ini. Проверьте файл; точная исходная копия сохранена: " + backup, error);
        }
        var receipt = new InstalledGameSetupReceipt(ini, backup, beforeSha, afterSha, changed);
        local.TryWrite("game-settings-configured", receipt);
        return receipt;
    }

    private static byte[] ReadBounded(FileStream file)
    {
        Require(file.Length is > 0 and <= 65536, "winx.ini должен быть непустым файлом не более 64 КиБ.");
        byte[] bytes = new byte[checked((int)file.Length)];
        int read = 0;
        while (read < bytes.Length)
        {
            int count = file.Read(bytes, read, bytes.Length - read);
            Require(count != 0, "Не удалось полностью прочитать winx.ini."); read += count;
        }
        return bytes;
    }

    private static byte[] SetStartLevelOne(byte[] bytes)
    {
        bool bom = bytes.AsSpan().StartsWith(new byte[] { 0xEF, 0xBB, 0xBF });
        int prefix = bom ? 3 : 0;
        string text;
        try { text = StrictUtf8.GetString(bytes, prefix, bytes.Length - prefix); }
        catch (DecoderFallbackException error) { throw new InvalidDataException("winx.ini должен иметь кодировку ASCII или UTF-8; файл не изменён.", error); }
        Require(StrictUtf8.GetBytes(text).AsSpan().SequenceEqual(bytes.AsSpan(prefix)), "Кодировка winx.ini неоднозначна; файл не изменён.");
        Require(text.All(c => c >= ' ' || c is '\t' or '\r' or '\n'), "winx.ini содержит двоичные данные; файл не изменён.");
        int valueStart = -1, valueLength = 0;
        for (int start = 0; start < text.Length;)
        {
            int next = text.IndexOf('\n', start);
            if (next < 0) next = text.Length;
            Require(next < text.Length || next == start || text[next - 1] != '\r',
                "winx.ini содержит неподдерживаемые окончания строк; файл не изменён.");
            int end = next > start && text[next - 1] == '\r' ? next - 1 : next;
            string line = text[start..end];
            Require(!line.Contains('\r'), "winx.ini содержит неподдерживаемые окончания строк; файл не изменён.");
            string trimmed = line.Trim();
            if (trimmed.Length != 0 && trimmed[0] is not (';' or '#'))
            {
                Require(trimmed[0] != '[', "winx.ini содержит секции; автоматическая настройка не поддерживается. Файл не изменён.");
                int equals = line.IndexOf('=');
                if (equals >= 0 && line[..equals].Trim().Equals("startLevel", StringComparison.OrdinalIgnoreCase))
                {
                    Require(valueStart < 0, "В winx.ini несколько startLevel; файл не изменён.");
                    int from = equals + 1;
                    while (from < line.Length && line[from] is ' ' or '\t') ++from;
                    int to = from;
                    while (to < line.Length && line[to] is >= '0' and <= '9') ++to;
                    string suffix = line[to..].TrimStart(' ', '\t');
                    Require(to > from && uint.TryParse(line[from..to], NumberStyles.None, CultureInfo.InvariantCulture, out _) &&
                        (suffix.Length == 0 || suffix[0] is ';' or '#'), "Значение startLevel неоднозначно; файл не изменён.");
                    valueStart = start + from; valueLength = to - from;
                }
            }
            start = next < text.Length ? next + 1 : text.Length;
        }
        if (valueStart >= 0) text = text[..valueStart] + "1" + text[(valueStart + valueLength)..];
        else
        {
            string newline = text.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
            text += (text.EndsWith('\n') ? "" : newline) + "startLevel=1" + newline;
        }
        byte[] payload = StrictUtf8.GetBytes(text);
        Require(payload.Length + prefix <= 65536, "Настроенный winx.ini превышает 64 КиБ; файл не изменён.");
        byte[] result = new byte[payload.Length + prefix];
        if (bom) bytes.AsSpan(0, prefix).CopyTo(result);
        payload.CopyTo(result, prefix);
        return result;
    }

    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
}
