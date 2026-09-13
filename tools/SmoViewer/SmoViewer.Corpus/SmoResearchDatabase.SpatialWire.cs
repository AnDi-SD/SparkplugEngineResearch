using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    // Historical cross-platform profiles describe authored bytes, not a PC
    // ResourceGraph run over PS2 data. Keep archived database evidence readable
    // while refusing to replace it with an incompatible runtime observation.
    private static void RequireSpatialRuntimeProfile(SmoResearchClassReport report)
    {
        if (report.Profiles.Any(profile => profile.PlatformKey != "pc"))
            throw new NotSupportedException(
                "SPATIAL_HISTORICAL_PROFILE: this analysis combines PC and PS2 " +
                "wire evidence, while spatial decoders now expose actual PC " +
                "ResourceGraph state. Cross-platform reanalysis requires a " +
                "separate verified PS2 loader. Existing database evidence " +
                "remains readable; no evidence has been replaced.");
    }

    // This is explicitly physical provenance. The common spSerializer prefix
    // reader supplies the encoding; loaded references cannot supply it.
    private static SmoNodeRelationship ReadSpatialWireReference(
        SmoDocument document,SmoObjectEntry entry,string semantic,int occurrence = 0)
    {
        var fields = SmoObjectFieldReader.Read(document,entry);
        var selected = fields.Where((field,index) =>
            SmoSerializedFieldRegistry.TryDescribeField(entry.TypeHash,fields,index,
                out var descriptor) && descriptor?.Key == semantic).ToArray();
        if (occurrence < 0 || occurrence >= selected.Length ||
            !SmoNodeDecoder.TryDecodeRelationship(document,
                selected[occurrence].Payload.Span,out var reference) || reference is null)
            throw new InvalidDataException($"Cannot inspect authored {semantic} reference.");
        return reference;
    }
}
