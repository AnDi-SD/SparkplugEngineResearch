namespace SmoViewer.Corpus;

public sealed record SmoResearchUpdateResult(
    string DatabasePath,
    string CorpusKey,
    string PlatformKey,
    string Provenance,
    string SourceKind,
    int Containers,
    long Occurrences,
    int ScannedResources,
    int UnchangedResources,
    int ParsedSmo,
    int FailedSmo,
    int PlatformConflicts,
    int RemovedOccurrences,
    long UniqueResources,
    long SmoResources,
    long Objects,
    long DirectFields,
    TimeSpan Elapsed);

public sealed record SmoResearchCorpusSummary(
    string CorpusKey,
    string PlatformKey,
    string Provenance,
    string SourceKind,
    long UniqueResources,
    long PhysicalOccurrences,
    long SmoResources,
    long ParsedSmo,
    long FailedSmo,
    long PlatformConflicts,
    long Objects,
    long DirectFields,
    int Classes);

public sealed record SmoResearchSummary(
    string DatabasePath,
    int SchemaVersion,
    IReadOnlyList<SmoResearchCorpusSummary> Corpora,
    int Classes,
    int PcOnlyClasses,
    int Ps2OnlyClasses,
    int CommonClasses,
    long DatabaseBytes);

public sealed record SmoResearchRegisteredSource(
    string CorpusKey,
    string PlatformKey,
    string Provenance,
    string SourceKind,
    string SourceRoot,
    string? ExecutablePath);

public sealed record SmoResearchClassCount(
    string CorpusKey,
    string PlatformKey,
    uint TypeHash,
    string? EngineName,
    long UniqueObjectCount,
    int UniqueResourceCount,
    long PhysicalObjectOccurrences);

public sealed record SmoResearchClassProfile(
    string CorpusKey,
    string PlatformKey,
    long UniqueObjectCount,
    int UniqueResourceCount,
    long PhysicalObjectOccurrences,
    long NamedObjectCount,
    int MinimumSerializedSize,
    int MaximumSerializedSize);

public sealed record SmoResearchClassResource(
    string CorpusKey,
    string RelativePath,
    long UniqueObjectCount,
    long PhysicalObjectOccurrences);

public sealed record SmoResearchClassCounterpart(
    string SourceCorpusKey,
    string SourceRelativePath,
    string CandidateCorpusKey,
    string CandidatePlatformKey,
    string CandidateRelativePath,
    string CandidateSha256,
    int CandidateObjectCount,
    int CandidateClassObjectCount,
    int PhysicalOccurrences);

public sealed record SmoResearchClassRelation(
    string CorpusKey,
    string Direction,
    uint? RelatedTypeHash,
    string? RelatedEngineName,
    long RelationCount,
    int ResourceCount);

public sealed record SmoResearchClassVariantCandidate(
    string CorpusKey,
    string PlatformKey,
    int SerializedSize,
    string FieldShape,
    long ObjectCount,
    int ResourceCount);

public sealed record SmoResearchClassFieldShape(
    string CorpusKey,
    string PlatformKey,
    int SectionIndex,
    int SectionFromEnd,
    int FieldType,
    int Occurrence,
    int PayloadSize,
    long OccurrenceCount,
    int ResourceCount,
    string? SemanticKey,
    string? PayloadLayout);

public sealed record SmoResearchClassFieldSample(
    string CorpusKey,
    int SectionFromEnd,
    int FieldType,
    int Occurrence,
    int PayloadSize,
    string PayloadPreviewHex,
    string? DecodedValue,
    string RelativePath,
    int ObjectIndex,
    string ObjectName);

public sealed record SmoResearchClassExample(
    string CorpusKey,
    string RelativePath,
    int ObjectIndex,
    uint ObjectId,
    string ObjectName,
    int? ParentIndex,
    string? ParentName,
    uint? ParentTypeHash,
    int SerializedSize,
    string FieldShape);

public sealed record SmoResearchClassReport(
    uint TypeHash,
    string? EngineName,
    IReadOnlyList<SmoResearchClassProfile> Profiles,
    IReadOnlyList<SmoResearchClassResource> Resources,
    IReadOnlyList<SmoResearchClassCounterpart> Counterparts,
    IReadOnlyList<SmoResearchClassRelation> Relations,
    IReadOnlyList<SmoResearchClassVariantCandidate> Variants,
    IReadOnlyList<SmoResearchClassFieldShape> Fields,
    IReadOnlyList<SmoResearchClassFieldSample> FieldSamples,
    IReadOnlyList<SmoResearchClassExample> Examples);

public sealed record SmoResearchClassAnalysisResult(
    uint TypeHash,
    string EngineName,
    string Status,
    int Corpora,
    long UniqueObjects,
    int UniqueResources,
    int DistinctPayloads,
    int VariantAssignments,
    int EvidenceRows,
    string Notes);

public sealed record SmoResearchRuntimeEvidence(
    string? TypeIdentifier,
    string? PlatformKey,
    string? CorpusKey,
    string EvidenceKind,
    string SourcePath,
    string? Locator,
    string Observation,
    string Confidence,
    string? SourceSha256,
    string CreatedUtc);

public sealed record SmoResearchPlatformConflict(
    string CorpusKey,
    string RelativePath,
    uint PlatformMask,
    string? ResourcePlatformKey,
    string? Note);

public sealed record SmoResearchHeaderProfile(
    string CorpusKey,
    string PlatformKey,
    uint SerializerVersion,
    uint PlatformMask,
    int ResourceCount,
    int DistinctUnknown08,
    uint MinimumUnknown08,
    uint MaximumUnknown08);

public sealed record SmoResearchResourceMatch(
    string CorpusKey,
    string PlatformKey,
    string RelativePath,
    string Extension,
    long ByteSize,
    string Sha256,
    uint? SerializerVersion,
    uint? Unknown08,
    uint? PlatformMask,
    string ParseStatus,
    int ObjectCount,
    int PhysicalOccurrences);

public sealed record SmoResearchResourceDifference(
    string NormalizedPath,
    string Status,
    long? LeftByteSize,
    long? RightByteSize,
    string? LeftSha256,
    string? RightSha256);

public sealed record SmoResearchCorpusComparison(
    string LeftCorpus,
    string RightCorpus,
    int LeftPaths,
    int RightPaths,
    int CommonPaths,
    int IdenticalPaths,
    int ChangedPaths,
    int LeftOnlyPaths,
    int RightOnlyPaths,
    IReadOnlyList<SmoResearchResourceDifference> Differences);

public sealed record GameResourceFormatSummary(
    string FormatKey,
    string DisplayName,
    string Category,
    string DecodeStatus,
    string WriteStatus,
    string EvidenceStatus,
    string Extensions,
    int Variants,
    long UniqueResourceVersions,
    long LogicalPaths,
    long PhysicalOccurrences,
    long DecodedResources,
    long PartialResources,
    long InventoryOnlyResources,
    long ErrorResources);

public sealed record GameResourceDependencySummary(
    string CorpusKey,
    string SourceFormat,
    string RelationKind,
    string ResolutionStatus,
    long DependencyCount);

public sealed record GameResourceAnalysisError(
    string CorpusKey,
    string PlatformKey,
    string RelativePath,
    string FormatKey,
    string RecognitionStatus,
    string Error);

public sealed record GameResourceFileRecord(
    string CorpusKey,
    string PlatformKey,
    string RelativePath,
    string Extension,
    long ByteSize,
    string Sha256,
    string FormatKey,
    string RecognitionStatus,
    string DecodeStatus,
    string? Error,
    int PhysicalOccurrences);

public sealed record GameResourceAudit(
    string DatabasePath,
    int SchemaVersion,
    long TotalResources,
    long AssignedResources,
    long UnassignedResources,
    long AnalysisErrors,
    long Dependencies,
    long ResolvedDependencies,
    long AmbiguousDependencies,
    long UnresolvedDependencies,
    IReadOnlyList<GameResourceFormatSummary> Formats,
    IReadOnlyList<GameResourceDependencySummary> DependencyStatuses);
