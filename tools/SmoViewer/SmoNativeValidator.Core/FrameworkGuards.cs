using System.Runtime.CompilerServices;

namespace SmoNativeValidator.Core;

// Framework guards only. The normal target keeps the BCL implementation;
// the separately built NET6 compatibility target retains the same checks.
internal static class FrameworkGuards
{
    internal static void ThrowIfNullOrWhiteSpace(string? value,
        [CallerArgumentExpression(nameof(value))] string? paramName = null)
    {
#if NET6_0
        ArgumentNullException.ThrowIfNull(value, paramName);
        if (string.IsNullOrWhiteSpace(value))
            throw new ArgumentException("The value cannot be an empty string or composed entirely of whitespace.", paramName);
#else
        ArgumentException.ThrowIfNullOrWhiteSpace(value, paramName);
#endif
    }

    internal static void ThrowIfNegativeOrZero(int value,
        [CallerArgumentExpression(nameof(value))] string? paramName = null)
    {
#if NET6_0
        if (value <= 0)
            throw new ArgumentOutOfRangeException(paramName, value, "The value must be greater than zero.");
#else
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(value, paramName);
#endif
    }

    internal static void ThrowIfDisposed(bool condition, object instance)
    {
#if NET6_0
        if (condition) throw new ObjectDisposedException(instance?.GetType().FullName);
#else
        ObjectDisposedException.ThrowIf(condition, instance);
#endif
    }
}
