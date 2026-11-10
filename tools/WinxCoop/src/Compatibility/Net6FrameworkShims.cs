#if NET6_0
global using WinxCoop.Compatibility;

namespace WinxCoop.Compatibility;

// Missing BCL surface only. Keep these out of NET8 so its binding and behavior
// remain the framework's own implementation.
internal static class Net6FrameworkShims
{
    internal static IOrderedEnumerable<T> Order<T>(this IEnumerable<T> values) =>
        values.OrderBy(static value => value);

    internal static void ReadExactly(this Stream stream, byte[] buffer)
    {
        ArgumentNullException.ThrowIfNull(stream);
        ArgumentNullException.ThrowIfNull(buffer);
        int offset = 0;
        while (offset < buffer.Length)
        {
            int read = stream.Read(buffer, offset, buffer.Length - offset);
            if (read == 0) throw new EndOfStreamException("Unable to read beyond the end of the stream.");
            offset = checked(offset + read);
        }
    }

    internal static int IndexOfAnyExcept(this ReadOnlySpan<byte> values, byte excluded)
    {
        for (int index = 0; index < values.Length; ++index)
            if (values[index] != excluded) return index;
        return -1;
    }

    internal static int IndexOfAnyExcept(this Span<byte> values, byte excluded) =>
        ((ReadOnlySpan<byte>)values).IndexOfAnyExcept(excluded);
}
#endif
