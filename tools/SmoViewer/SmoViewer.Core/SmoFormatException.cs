namespace SmoViewer.Core;

/// <summary>
/// Indicates that an SMO file cannot be parsed because its structural data is
/// missing or malformed.
/// </summary>
public sealed class SmoFormatException : Exception
{
    public SmoFormatException(string message)
        : base(message)
    {
    }

    public SmoFormatException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
}
