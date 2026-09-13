namespace SmoViewer.Corpus;

public sealed record SmoCorpusUpdateResult(
    string DatabasePath,
    string SourceRoot,
    int DiscoveredFiles,
    int ScannedFiles,
    int UnchangedFiles,
    int FailedFiles,
    int RemovedFiles,
    long Objects,
    long DirectFields,
    int KnownClasses,
    int UnknownClasses,
    long DatabaseBytes,
    TimeSpan Elapsed);

public sealed record SmoCorpusSummary(
    string DatabasePath,
    string? SourceRoot,
    int Files,
    int ParsedFiles,
    int FailedFiles,
    long Objects,
    long DirectFields,
    int Classes,
    int KnownClasses,
    int UnknownClasses,
    int VariantDefinitions,
    int VariantAssignments,
    int FieldDefinitions,
    long DatabaseBytes);

public sealed record SmoCorpusClassSummary(
    uint TypeHash,
    string? EngineName,
    bool IsKnown,
    long ObjectCount,
    int FileCount,
    int FamilyCount);

public sealed record SmoCorpusClassMetrics(
    uint TypeHash,
    string? EngineName,
    long ObjectCount,
    int FileCount,
    int FamilyCount,
    int SerializedSizeCount,
    int FieldShapeCount,
    int StructuralVariantCount,
    long MinimumSerializedSize,
    long MaximumSerializedSize,
    double AverageSerializedSize,
    int MinimumFieldCount,
    int MaximumFieldCount,
    double AverageFieldCount,
    long FieldParseErrors);
