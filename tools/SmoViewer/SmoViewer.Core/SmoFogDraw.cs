using SmoViewer.Sparkplug;
namespace SmoViewer.Core;

/// <summary>Immutable actual renderer state submission, not a second fog reader.
/// ObjectId0 uses the explicit tool default; it does not reconstruct global Fog.</summary>
public sealed record SmoFogDraw(uint ObjectId, uint KnownMask, uint Enabled, uint Mode,
    uint Color, float Start, float End, float Density)
{
    public string? Issue { get; init; }
    internal static SmoFogDraw Read(GraphHandle graph, uint id)
    {
        try
        {
            NativeMethods.Check(NativeMethods.spv_graph_fog_draw(graph, id, out var value));
            return new(id, value.Known, value.Enabled, value.Mode, value.Color, value.Start, value.End, value.Density);
        }
        catch (InvalidDataException error)
        { return new(id, 0, 0, 0, 0, 0, 0, 0) { Issue = error.Message }; }
    }
}
