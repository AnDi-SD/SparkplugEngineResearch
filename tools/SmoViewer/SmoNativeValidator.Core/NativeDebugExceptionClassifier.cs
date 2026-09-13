namespace SmoNativeValidator.Core;

/// <summary>
/// Classifies native and WOW64 exception codes emitted for debugger breakpoints.
/// A 32-bit debuggee reports the WX86 codes when observed by a 64-bit debugger.
/// </summary>
public static class NativeDebugExceptionClassifier
{
    public const uint Breakpoint = 0x80000003;
    public const uint SingleStep = 0x80000004;
    public const uint Wow64Breakpoint = 0x4000001F;
    public const uint Wow64SingleStep = 0x4000001E;

    public static bool IsSoftwareBreakpoint(uint exceptionCode) =>
        exceptionCode is Breakpoint or Wow64Breakpoint;

    public static bool IsSingleStep(uint exceptionCode) =>
        exceptionCode is SingleStep or Wow64SingleStep;
}
