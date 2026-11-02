using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Our stopped-process ownership check, shared by world observation and the
// opt-in aim adapter. No original transform or targeting algorithm is copied.
internal static class NativeTargetNodeOwnership
{
    internal static void VerifyAtStop(DebuggerHost host, uint bound, uint root, uint scene)
    {
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ImageBase == 0x400000,
            "Target Node ownership requires the stopped original process.");
        HashSet<uint> visited = [];
        for (uint node = bound; visited.Count < 64; node = U(node + 0x2C))
        {
            Require(node >= 0x10000 && (node & 3) == 0 && visited.Add(node) &&
                U(node) == VisualGraphReader.NodeVtable && U(node + 0x3C) == scene,
                $"Target Node ancestry is not typed/acyclic/in-scene: bound={bound:X8}, node={node:X8}, root={root:X8}.");
            if (node == root) return;
        }
        throw new InvalidDataException("Target Node ancestry exceeds64 nodes.");
        uint U(uint address) => Win32Native.ReadUInt32(host.ProcessHandle, address);
    }

    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
}
