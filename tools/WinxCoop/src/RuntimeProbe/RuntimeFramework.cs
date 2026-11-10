using System.Security.Cryptography;

namespace WinxCoop.RuntimeProbe;

internal static class RuntimeFramework
{
    internal static bool IsAsciiLetterOrDigit(char value)
    {
#if NET6_0
        return value is >= 'A' and <= 'Z' or >= 'a' and <= 'z' or >= '0' and <= '9';
#else
        return char.IsAsciiLetterOrDigit(value);
#endif
    }

    internal static byte[] HashSha256(Stream stream)
    {
#if NET6_0
        using var hash = SHA256.Create();
        return hash.ComputeHash(stream);
#else
        return SHA256.HashData(stream);
#endif
    }
}
