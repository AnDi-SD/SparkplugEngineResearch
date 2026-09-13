namespace SmoViewer.Core;

/// <summary>
/// One immutable host ID mapping shared by the ranges of an additive plan.
/// This does not discover references or implement a game serializer.
/// </summary>
public sealed class SmoFileReferenceRemapContext
{
    private readonly Dictionary<uint, uint> idMap;
    private readonly Dictionary<uint[], uint[]> catalogs = new(ReferenceEqualityComparer.Instance);

    public SmoFileReferenceRemapContext(IReadOnlyDictionary<uint, uint> idMap)
    {
        ArgumentNullException.ThrowIfNull(idMap);
        this.idMap = new Dictionary<uint, uint>(idMap.Count);
        var destinations = new HashSet<uint>();
        foreach (var pair in idMap)
        {
            if (pair.Key == 0 || pair.Value == 0 ||
                !this.idMap.TryAdd(pair.Key, pair.Value) || !destinations.Add(pair.Value))
                throw new ArgumentException(
                    "Reference ID mapping must be nonzero and one-to-one.", nameof(idMap));
        }
    }

    /// <summary>Unmapped IDs, including a null reference, remain unchanged.</summary>
    public uint MapId(uint id) => idMap.TryGetValue(id, out uint mapped) ? mapped : id;

    /// <summary>
    /// Catalog arrays are immutable, privately owned range metadata. Ranges
    /// from one trace share their array, so a whole plan needs one mapped copy.
    /// Reference identity prevents unrelated catalog snapshots sharing a cache
    /// entry merely because their IDs happen to compare equal.
    /// </summary>
    internal uint[] RemapCatalog(uint[] catalog)
    {
        ArgumentNullException.ThrowIfNull(catalog);
        lock (catalogs)
        {
            if (catalogs.TryGetValue(catalog, out uint[]? cached)) return cached;
            var remapped = new uint[catalog.Length];
            var unique = new HashSet<uint>();
            for (int index = 0; index < catalog.Length; ++index)
            {
                uint mapped = MapId(catalog[index]);
                if (!unique.Add(mapped))
                    throw new InvalidDataException(
                        "SPARKPLUG_REFERENCE_TRACE: remapped IDs collide with retained file resources.");
                remapped[index] = mapped;
            }
            catalogs.Add(catalog, remapped);
            return remapped;
        }
    }
}
