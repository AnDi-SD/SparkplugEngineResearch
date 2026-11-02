namespace WinxCoop.RuntimeProbe;

// Host-only cosmetic projection policy. The caller must first verify every
// current native pool/owner/scene/active/Launch identity. This class neither
// validates a native pointer nor changes native physics, reward or wire data.
internal sealed class WorldStarPresentationQuarantine(int maximumActivationIds)
{
    private readonly int _maximumActivationIds = maximumActivationIds is > 0 and <= 100000
        ? maximumActivationIds : throw new ArgumentOutOfRangeException(nameof(maximumActivationIds));
    private readonly HashSet<ulong> _quarantined = [];
    private ulong _omittedRecords, _recoveredRecordsStillOmitted;

    internal int QuarantinedActivationCount => _quarantined.Count;
    internal ulong OmittedRecordCount => _omittedRecords;
    internal object Summary => new
    {
        hostCaptureOnly = true, cosmeticPresentationOnly = true,
        reason = "finite-native-position-outside-existing-presentation-envelope",
        positionMagnitudeLimit = WorldStarState.MaximumPositionMagnitude,
        quarantinedActivationCount = _quarantined.Count,
        omittedCaptureRecords = _omittedRecords,
        recoveredFinitePositionsStillOmitted = _recoveredRecordsStillOmitted,
        maximumRememberedActivationIds = _maximumActivationIds,
        activationIdsSample = _quarantined.Order().Take(64).ToArray(),
        stickyUntilNewOriginalLaunchActivation = true,
        ownerChecksMayBeSkipped = false, nativeAwardTrackingSuppressed = false,
        invalidWireValuesForwarded = false, nativeFieldsWritten = 0,
        nativePhysicsFixed = false, allNativeStarVisualsQualified = false
    };

    internal bool TryInclude(WorldStarState state, out bool firstOmission)
    {
        ArgumentNullException.ThrowIfNull(state);
        // Always revalidate the other scalar/identity conditions, including
        // after quarantine. A remembered ID must not hide a later corruption.
        bool unsupportedPosition = state.HasFinitePositionOutsidePresentationEnvelope();
        firstOmission = false;
        if (unsupportedPosition && !_quarantined.Contains(state.ActivationId))
        {
            if (_quarantined.Count >= _maximumActivationIds)
                throw new InvalidDataException("Star presentation quarantine exceeded its native activation budget.");
            _quarantined.Add(state.ActivationId); firstOmission = true;
        }
        if (!_quarantined.Contains(state.ActivationId)) return true;
        _omittedRecords = checked(_omittedRecords + 1);
        if (!unsupportedPosition) _recoveredRecordsStillOmitted = checked(_recoveredRecordsStillOmitted + 1);
        return false;
    }
}
