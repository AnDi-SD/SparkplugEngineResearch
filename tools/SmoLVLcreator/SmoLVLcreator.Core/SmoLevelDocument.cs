using System.Collections.ObjectModel;
using System.Numerics;
using SmoImporter.Core;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoLVLcreator.Core;

public readonly record struct SmoPlacementId(
    int MeshObjectIndex,
    int SceneObjectIndex);

/// <summary>One placed object; several physical mesh parts can share it.</summary>
public readonly record struct SmoLevelEntityId(int SceneObjectIndex);

public enum SmoLevelEntityKind
{
    Visual,
    Collision
}

/// <summary>
/// Mutable editor state layered over a read-only decoded workspace. Commands
/// never write to the source SMO file.
/// </summary>
public sealed class SmoLevelDocument
{
    private readonly Dictionary<SmoPlacementId, SmoEditablePlacement> _placements;
    private readonly Dictionary<SmoLevelEntityId, SmoLevelEntity> _entities;
    private readonly Dictionary<SmoLevelEntityId, SmoCompositeModel> _compositeModelsByEntity;
    private readonly Dictionary<SmoLevelEntityId, SmoLevelCollisionLink[]> _collisionLinks;
    private readonly List<SmoLevelEntity> _entityList = [];
    private readonly List<SmoLevelCollisionLink> _collisionLinkList = [];
    private readonly Dictionary<int, SmoLevelTextureReplacement> _textureReplacements = [];
    private readonly Dictionary<int, SmoLevelModelReplacement> _modelReplacements = [];
    private readonly Dictionary<Guid, SmoLevelPlacementAddition> _placementAdditions = [];
    private readonly Dictionary<Guid, SmoLevelExternalModel> _externalModels = [];
    private readonly Dictionary<Guid, SmoLevelExternalPlacement> _externalPlacements = [];
    private readonly Dictionary<SmoLevelEntityId, SmoLevelGeneratedCollision>
        _generatedCollisions = [];
    private readonly HashSet<SmoLevelEntityId> _removedEntityIds = [];
    private readonly List<SmoEditableCollision> _collisions;
    private readonly List<IEditCommand> _history = [];
    private int _historyPosition;
    private int _savedHistoryPosition;
    private SmoTransformSession? _activeTransformSession;
    private SmoPendingPlacementTransformSession? _activePendingTransformSession;
    private int _nextGeneratedEntityIndex = -1;

    public SmoLevelDocument(SmoLevelWorkspace workspace)
    {
        Workspace = workspace ?? throw new ArgumentNullException(nameof(workspace));
        _placements = new Dictionary<SmoPlacementId, SmoEditablePlacement>();
        foreach (SmoLevelAsset asset in workspace.Assets)
        {
            foreach (SmoLevelPlacement placement in asset.Placements)
            {
                var id = new SmoPlacementId(
                    asset.ObjectIndex,
                    placement.SceneObjectIndex);
                if (!_placements.TryAdd(
                    id,
                    new SmoEditablePlacement(
                        id,
                        asset,
                        placement,
                        placement.WorldTransform,
                        placement.WorldTransform)))
                    throw new InvalidDataException(
                        $"REPEATED_RENDERABLE_AUTHORING: mesh [{asset.ObjectIndex}], renderable [{placement.SceneObjectIndex}] has multiple actual support slots. " +
                        "Workspace preserves their OccurrenceKey values; the current editor command model cannot address these slots independently.");
            }
        }

        _entities = new Dictionary<SmoLevelEntityId, SmoLevelEntity>();
        _collisions = workspace.Collisions
            .Select(collision => new SmoEditableCollision(
                collision,
                collision.WorldTransform,
                collision.WorldTransform))
            .ToList();
        foreach (IGrouping<int, SmoEditablePlacement> group in
                 _placements.Values.GroupBy(placement =>
                      SmoPlacementTransformWriter.FindStaticPlacementOwnerIndex(
                          workspace.Document,
                          placement.Source.SceneObjectIndex) ??
                      SmoPlacementTransformWriter.FindNodeTransformOwnerIndex(
                          workspace.Document,
                          placement.Source.SceneObjectIndex) ??
                      placement.Source.SceneObjectIndex))
        {
            var id = new SmoLevelEntityId(group.Key);
            SmoEditablePlacement[] parts = group.ToArray();
            SmoEditablePlacement source = parts[0];
            var entity = new SmoLevelEntity(
                id,
                source.Source.Name.TrimEnd('\0'),
                new ReadOnlyCollection<SmoEditablePlacement>(parts),
                Array.Empty<SmoEditableCollision>(),
                source.WorldTransform,
                source.WorldTransform);
            foreach (SmoEditablePlacement part in parts)
                part.Entity = entity;
            _entities.Add(id, entity);
            _entityList.Add(entity);
        }

        foreach (SmoEditableCollision collision in _collisions)
        {
            var id = new SmoLevelEntityId(collision.Source.CollisionInfoObjectIndex);
            var entity = new SmoLevelEntity(
                id,
                collision.Source.Name,
                Array.Empty<SmoEditablePlacement>(),
                new ReadOnlyCollection<SmoEditableCollision>([collision]),
                collision.WorldTransform,
                collision.WorldTransform);
            collision.Entity = entity;
            _entities.Add(id, entity);
            _entityList.Add(entity);
        }

        CompositeModels = SmoCompositeModelResolver.Resolve(workspace, _entities.Values);
        _compositeModelsByEntity = CompositeModels
            .SelectMany(model => model.Entities.Select(entity => (entity.Id, model)))
            .ToDictionary(pair => pair.Id, pair => pair.model);

        _collisionLinkList.AddRange(SmoLevelCollisionLinker.Build(
            workspace,
            _placements,
            _collisions));
        CollisionLinks = new ReadOnlyCollection<SmoLevelCollisionLink>(
            _collisionLinkList);
        _collisionLinks = _collisionLinkList
            .SelectMany(link => new[]
            {
                (link.VisualEntityId, Link: link),
                (link.CollisionEntityId, Link: link)
            })
            .GroupBy(pair => pair.Item1)
            .ToDictionary(
                group => group.Key,
                group => group.Select(pair => pair.Link).ToArray());

        Placements = new ReadOnlyCollection<SmoEditablePlacement>(
            _placements.Values.ToArray());
        Entities = new ReadOnlyCollection<SmoLevelEntity>(_entityList);
        Collisions = new ReadOnlyCollection<SmoEditableCollision>(_collisions);
    }

    public SmoLevelWorkspace Workspace { get; }
    public IReadOnlyList<SmoEditablePlacement> Placements { get; }
    public IReadOnlyList<SmoLevelEntity> Entities { get; }
    public IReadOnlyList<SmoEditableCollision> Collisions { get; }
    public IReadOnlyList<SmoCompositeModel> CompositeModels { get; }
    public IReadOnlyList<SmoLevelCollisionLink> CollisionLinks { get; }
    public IReadOnlyDictionary<int, SmoLevelTextureReplacement> TextureReplacements =>
        new ReadOnlyDictionary<int, SmoLevelTextureReplacement>(_textureReplacements);
    public IReadOnlyDictionary<int, SmoLevelModelReplacement> ModelReplacements =>
        new ReadOnlyDictionary<int, SmoLevelModelReplacement>(_modelReplacements);
    public IReadOnlyDictionary<Guid, SmoLevelPlacementAddition> PlacementAdditions =>
        new ReadOnlyDictionary<Guid, SmoLevelPlacementAddition>(_placementAdditions);
    public IReadOnlyDictionary<Guid, SmoLevelExternalModel> ExternalModels =>
        new ReadOnlyDictionary<Guid, SmoLevelExternalModel>(_externalModels);
    public IReadOnlyDictionary<Guid, SmoLevelExternalPlacement> ExternalPlacements =>
        new ReadOnlyDictionary<Guid, SmoLevelExternalPlacement>(_externalPlacements);
    public IReadOnlyDictionary<SmoLevelEntityId, SmoLevelGeneratedCollision>
        GeneratedCollisions =>
        new ReadOnlyDictionary<SmoLevelEntityId, SmoLevelGeneratedCollision>(
            _generatedCollisions);
    public IReadOnlySet<SmoLevelEntityId> RemovedEntityIds => _removedEntityIds;
    public bool CanUndo =>
        _activeTransformSession is null &&
        _activePendingTransformSession is null &&
        _historyPosition > 0;
    public bool CanRedo =>
        _activeTransformSession is null &&
        _activePendingTransformSession is null &&
        _historyPosition < _history.Count;
    public bool IsModified =>
        _activeTransformSession is not null ||
        _activePendingTransformSession is not null ||
        _historyPosition != _savedHistoryPosition;
    public string? UndoDescription => CanUndo
        ? _history[_historyPosition - 1].Description
        : null;
    public string? RedoDescription => CanRedo
        ? _history[_historyPosition].Description
        : null;
    public long Revision { get; private set; }
    public bool HasActiveTransformSession =>
        _activeTransformSession is not null ||
        _activePendingTransformSession is not null;

    public event EventHandler? Changed;

    public bool TryGetPlacement(
        SmoPlacementId id,
        out SmoEditablePlacement? placement) =>
        _placements.TryGetValue(id, out placement);

    public SmoEditablePlacement GetPlacement(SmoPlacementId id) =>
        _placements.TryGetValue(id, out SmoEditablePlacement? placement)
            ? placement
            : throw new KeyNotFoundException(
                $"Placement mesh [{id.MeshObjectIndex}] / scene " +
                $"[{id.SceneObjectIndex}] is not part of this level document.");

    public bool TryGetEntity(
        SmoLevelEntityId id,
        out SmoLevelEntity? entity) =>
        _entities.TryGetValue(id, out entity);

    public SmoLevelEntity GetEntity(SmoLevelEntityId id) =>
        _entities.TryGetValue(id, out SmoLevelEntity? entity)
            ? entity
            : throw new KeyNotFoundException(
                $"Level entity [{id.SceneObjectIndex}] is not part of this document.");

    public IReadOnlyList<SmoLevelCollisionLink> GetCollisionLinks(
        SmoLevelEntityId entityId) =>
        _collisionLinks.TryGetValue(entityId, out SmoLevelCollisionLink[]? links)
            ? links.Where(link =>
                    !_removedEntityIds.Contains(link.VisualEntityId) &&
                    !_removedEntityIds.Contains(link.CollisionEntityId))
                .ToArray()
            : Array.Empty<SmoLevelCollisionLink>();

    public bool TryGetCompositeModel(
        SmoLevelEntityId entityId,
        out SmoCompositeModel? model) =>
        _compositeModelsByEntity.TryGetValue(entityId, out model);

    public bool CanPersistTransform(SmoLevelEntityId entityId)
    {
        SmoLevelEntity entity = GetEntity(entityId);
        if (_removedEntityIds.Contains(entityId))
            return false;
        if (_generatedCollisions.ContainsKey(entityId))
            return true;
        if (entity.Kind == SmoLevelEntityKind.Collision)
        {
            return SmoPlacementTransformWriter.CanWriteCollisionTransform(
                Workspace.Document,
                entityId.SceneObjectIndex);
        }
        return SmoPlacementTransformWriter.FindStaticPlacementOwnerIndex(
                   Workspace.Document,
                   entityId.SceneObjectIndex) is not null ||
               SmoPlacementTransformWriter.CanWriteNodeTransform(
                   Workspace.Document,
                   entityId.SceneObjectIndex);
    }

    /// <summary>
    /// Expands inferred split-model parts for editor selection. The source
    /// entities remain independent, so callers can still transform one raw
    /// part by passing its original ID directly to BeginTransform.
    /// </summary>
    public IReadOnlyList<SmoLevelEntityId> ExpandCompositeEntities(
        IEnumerable<SmoLevelEntityId> entityIds)
    {
        ArgumentNullException.ThrowIfNull(entityIds);
        var expanded = new HashSet<SmoLevelEntityId>();
        foreach (SmoLevelEntityId id in entityIds)
        {
            if (_compositeModelsByEntity.TryGetValue(id, out SmoCompositeModel? model))
            {
                foreach (SmoLevelEntity part in model.Entities)
                    expanded.Add(part.Id);
            }
            else
            {
                expanded.Add(id);
            }
        }
        return new ReadOnlyCollection<SmoLevelEntityId>(expanded.ToArray());
    }

    /// <summary>
    /// Adds editor-linked visual/collision peers without changing the source
    /// hierarchy. Passing the result to BeginTransform moves the pair as one
    /// undoable operation; passing the original IDs keeps movement independent.
    /// </summary>
    public IReadOnlyList<SmoLevelEntityId> ExpandLinkedEntities(
        IEnumerable<SmoLevelEntityId> entityIds)
    {
        ArgumentNullException.ThrowIfNull(entityIds);
        var expanded = new HashSet<SmoLevelEntityId>(entityIds);
        foreach (SmoLevelEntityId id in expanded.ToArray())
        {
            foreach (SmoLevelCollisionLink link in GetCollisionLinks(id))
            {
                expanded.Add(link.VisualEntityId);
                expanded.Add(link.CollisionEntityId);
            }
        }
        return new ReadOnlyCollection<SmoLevelEntityId>(expanded.ToArray());
    }

    public bool SetPlacementTransform(
        SmoPlacementId id,
        Matrix4x4 worldTransform,
        string description = "Transform placement") =>
        SetEntityTransform(
            GetPlacement(id).Entity.Id,
            worldTransform,
            description);

    public bool ReplaceTexture(
        int textureObjectIndex,
        ReadOnlyMemory<byte> encodedImage,
        string sourcePath,
        bool replaceAlpha = false)
    {
        EnsureNoTransformSession();
        if ((uint)textureObjectIndex >= (uint)Workspace.Document.Objects.Count ||
            Workspace.Document.Objects[textureObjectIndex].TypeHash !=
                SmoClassIds.TextureData)
        {
            throw new ArgumentOutOfRangeException(
                nameof(textureObjectIndex),
                textureObjectIndex,
                "Object is not an spTextureData resource in this level.");
        }
        if (encodedImage.IsEmpty)
            throw new ArgumentException("Replacement image is empty.", nameof(encodedImage));

        _textureReplacements.TryGetValue(
            textureObjectIndex,
            out SmoLevelTextureReplacement? before);
        var after = new SmoLevelTextureReplacement(
            textureObjectIndex,
            encodedImage.ToArray(),
            Path.GetFullPath(sourcePath),
            replaceAlpha);
        if (before is not null && before.ContentEquals(after))
            return false;
        Execute(new ReplaceTextureCommand(
            $"Replace texture [{textureObjectIndex}]",
            textureObjectIndex,
            before,
            after));
        return true;
    }

    public bool ReplaceModel(
        int meshObjectIndex,
        ImportedScene importedScene,
        ReplacementTransform transform,
        Matrix4x4 referenceWorldTransform,
        string sourcePath)
    {
        EnsureNoTransformSession();
        ArgumentNullException.ThrowIfNull(importedScene);
        ArgumentNullException.ThrowIfNull(transform);
        if ((uint)meshObjectIndex >= (uint)Workspace.Document.Objects.Count ||
            Workspace.Document.Objects[meshObjectIndex].TypeHash !=
                SmoClassIds.MeshData)
        {
            throw new ArgumentOutOfRangeException(
                nameof(meshObjectIndex),
                meshObjectIndex,
                "Object is not an spMeshData resource in this level.");
        }
        if (importedScene.Meshes.Count == 0)
            throw new ArgumentException(
                "Imported scene contains no meshes.", nameof(importedScene));
        ValidateTransform(referenceWorldTransform);

        SmoLevelModelGraphPlan graph = SmoLevelModelGraphReplacer.ResolveWritablePlan(
            Workspace.Document,
            meshObjectIndex);
        SmoLevelImportValidator.ThrowIfInvalid(
            SmoLevelImportValidator.Validate(
                importedScene,
                SmoLevelImportPurpose.ReplaceModelResource,
                graph.Components.Count));
        if (importedScene.Meshes.Count != graph.Components.Count)
        {
            throw new InvalidOperationException(
                $"Complete SMO model contains {graph.Components.Count} mesh parts, " +
                $"but the imported model contains {importedScene.Meshes.Count}.");
        }
        importedScene = SmoLevelEmbeddedTextureBudget.Prepare(importedScene);
        // One history/resource entry owns the entire authoring model even when
        // replacement was invoked from a secondary mesh part.
        meshObjectIndex = graph.Components[0].MeshObjectIndex;

        _modelReplacements.TryGetValue(
            meshObjectIndex,
            out SmoLevelModelReplacement? before);
        var after = new SmoLevelModelReplacement(
            meshObjectIndex,
            importedScene,
            transform,
            referenceWorldTransform,
            Path.GetFullPath(sourcePath));
        if (before is not null && before.ContentEquals(after))
            return false;
        Execute(new ReplaceModelCommand(
            $"Replace model [{meshObjectIndex}]",
            meshObjectIndex,
            before,
            after));
        return true;
    }

    public Guid AddSharedPlacement(
        int meshObjectIndex,
        Matrix4x4 worldTransform,
        string name)
        => AddSharedPlacements(
            [new SmoSharedPlacementRequest(meshObjectIndex, worldTransform, name)])[0];

    public IReadOnlyList<Guid> AddSharedPlacements(
        IReadOnlyList<SmoSharedPlacementRequest> requests)
    {
        EnsureNoTransformSession();
        ArgumentNullException.ThrowIfNull(requests);
        if (requests.Count == 0)
            throw new ArgumentException("At least one placement is required.", nameof(requests));
        var additions = new List<SmoLevelPlacementAddition>(requests.Count);
        Guid groupId = Guid.NewGuid();
        foreach (SmoSharedPlacementRequest request in requests)
        {
            ValidateTransform(request.WorldTransform);
            if ((uint)request.MeshObjectIndex >= (uint)Workspace.Document.Objects.Count ||
                Workspace.Document.Objects[request.MeshObjectIndex].TypeHash !=
                    SmoClassIds.MeshData)
                throw new ArgumentOutOfRangeException(
                    nameof(requests),
                    request.MeshObjectIndex,
                    "Object is not an spMeshData resource in this level.");
            if (!SmoSharedPlacementCloner.CanClone(
                    Workspace.Document,
                    request.MeshObjectIndex))
                throw new InvalidOperationException(
                    $"Mesh [{request.MeshObjectIndex}] has no reference-only placement template yet.");
            additions.Add(new SmoLevelPlacementAddition(
                Guid.NewGuid(),
                groupId,
                request.MeshObjectIndex,
                request.WorldTransform,
                string.IsNullOrWhiteSpace(request.Name)
                    ? $"Mesh_{request.MeshObjectIndex}"
                    : request.Name.Trim()));
        }
        Execute(new AddPlacementsCommand(
            additions.Count == 1
                ? $"Add placement of mesh [{additions[0].MeshObjectIndex}]"
                : $"Add composite placement ({additions.Count} parts)",
            additions,
            Add: true));
        return new ReadOnlyCollection<Guid>(additions.Select(item => item.Id).ToArray());
    }

    public bool RemoveSharedPlacement(Guid placementId)
    {
        EnsureNoTransformSession();
        if (!_placementAdditions.TryGetValue(
                placementId,
                out SmoLevelPlacementAddition? addition))
            return false;
        Execute(new AddPlacementsCommand(
            $"Remove placement of mesh [{addition.MeshObjectIndex}]",
            [addition],
            Add: false));
        return true;
    }

    public bool RemoveSharedPlacementGroup(Guid groupId)
    {
        EnsureNoTransformSession();
        SmoLevelPlacementAddition[] additions = _placementAdditions.Values
            .Where(addition => addition.GroupId == groupId)
            .ToArray();
        if (additions.Length == 0)
            return false;
        Execute(new AddPlacementsCommand(
            additions.Length == 1
                ? $"Remove placement of mesh [{additions[0].MeshObjectIndex}]"
                : $"Remove composite placement ({additions.Length} parts)",
            additions,
            Add: false));
        return true;
    }

    public Guid AddExternalModel(
        ImportedScene importedScene,
        string sourcePath,
        string? name = null)
    {
        EnsureNoTransformSession();
        ArgumentNullException.ThrowIfNull(importedScene);
        SmoLevelImportValidator.ThrowIfInvalid(
            SmoLevelImportValidator.Validate(
                importedScene,
                SmoLevelImportPurpose.AddExternalModel));
        importedScene = SmoLevelEmbeddedTextureBudget.Prepare(importedScene);
        int templateMeshObjectIndex = SmoExternalLevelModelAppender
            .FindTemplateMeshObjectIndex(
                Workspace.Document,
                requireMaterial: importedScene.Textures.Count > 0);
        Guid id = Guid.NewGuid();
        string fullPath = Path.GetFullPath(sourcePath);
        var model = new SmoLevelExternalModel(
            id,
            importedScene,
            fullPath,
            string.IsNullOrWhiteSpace(name)
                ? Path.GetFileNameWithoutExtension(fullPath)
                : name.Trim(),
            templateMeshObjectIndex);
        Execute(new AddExternalModelCommand($"Import model {model.Name}", model));
        return id;
    }

    public Guid ReplaceCompositeModels(
        IReadOnlyList<SmoCompositeModel> instances,
        ImportedScene importedScene,
        ReplacementTransform transform,
        Matrix4x4 referenceWorldTransform,
        string sourcePath,
        string? name = null)
    {
        EnsureNoTransformSession();
        ArgumentNullException.ThrowIfNull(instances);
        ArgumentNullException.ThrowIfNull(importedScene);
        ArgumentNullException.ThrowIfNull(transform);
        if (instances.Count == 0)
            throw new ArgumentException(
                "At least one composite placement is required.", nameof(instances));
        if (importedScene.Meshes.Count == 0)
            throw new ArgumentException(
                "Imported scene contains no meshes.", nameof(importedScene));
        SmoLevelImportValidator.ThrowIfInvalid(
            SmoLevelImportValidator.Validate(
                importedScene,
                SmoLevelImportPurpose.ReplaceCompositeModel));
        importedScene = SmoLevelEmbeddedTextureBudget.Prepare(importedScene);
        ValidateTransform(referenceWorldTransform);
        if (!Matrix4x4.Invert(referenceWorldTransform, out Matrix4x4 inverseReference))
            throw new ArgumentException(
                "Composite reference transform must be invertible.",
                nameof(referenceWorldTransform));

        SmoLevelEntityId[] removedIds = instances
            .SelectMany(instance => instance.Entities)
            .Select(entity => entity.Id)
            .Distinct()
            .ToArray();
        foreach (SmoLevelEntityId id in removedIds)
        {
            SmoLevelEntity entity = GetEntity(id);
            if (entity.Kind != SmoLevelEntityKind.Visual ||
                _removedEntityIds.Contains(id))
            {
                throw new InvalidOperationException(
                    $"Composite source entity [{id.SceneObjectIndex}] is not replaceable.");
            }
        }

        int templateMeshObjectIndex = SmoExternalLevelModelAppender
            .FindTemplateMeshObjectIndex(
                Workspace.Document,
                requireMaterial: importedScene.Textures.Count > 0);
        Guid modelId = Guid.NewGuid();
        string fullPath = Path.GetFullPath(sourcePath);
        string modelName = string.IsNullOrWhiteSpace(name)
            ? Path.GetFileNameWithoutExtension(fullPath)
            : name.Trim();
        var model = new SmoLevelExternalModel(
            modelId,
            importedScene,
            fullPath,
            modelName,
            templateMeshObjectIndex);

        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        Matrix4x4 fittedReferenceWorld =
            reflection * transform.Matrix * reflection;
        ValidateTransform(fittedReferenceWorld);
        var placements = new List<SmoLevelExternalPlacement>(instances.Count);
        foreach ((SmoCompositeModel instance, int index) in instances.Select(
                     (instance, index) => (instance, index)))
        {
            Matrix4x4 instanceWorld = instance.Entities[0].WorldTransform;
            // System.Numerics uses row-vector composition: the instance is first
            // made relative to the selected reference, then placed at the fitted
            // replacement transform.
            Matrix4x4 relative = instanceWorld * inverseReference;
            Matrix4x4 worldTransform = relative * fittedReferenceWorld;
            ValidateTransform(worldTransform);
            if (!Matrix4x4.Invert(worldTransform, out _))
                throw new InvalidOperationException(
                    $"Composite replacement placement {index + 1} is singular.");
            placements.Add(new SmoLevelExternalPlacement(
                Guid.NewGuid(),
                modelId,
                worldTransform,
                $"{modelName}_{index + 1}"));
        }

        Execute(new ReplaceCompositeModelsCommand(
            $"Replace composite model {instances[0].Name}",
            model,
            placements,
            removedIds));
        return modelId;
    }

    public Guid AddExternalPlacement(
        Guid modelId,
        Matrix4x4 worldTransform,
        string? name = null)
    {
        EnsureNoTransformSession();
        ValidateTransform(worldTransform);
        if (!_externalModels.TryGetValue(modelId, out SmoLevelExternalModel? model))
            throw new KeyNotFoundException($"External model {modelId} is not in the catalog.");
        Guid id = Guid.NewGuid();
        var placement = new SmoLevelExternalPlacement(
            id,
            modelId,
            worldTransform,
            string.IsNullOrWhiteSpace(name) ? model.Name : name.Trim());
        Execute(new AddExternalPlacementCommand(
            $"Place external model {model.Name}",
            placement,
            Add: true));
        return id;
    }

    public bool RemoveExternalPlacement(Guid placementId)
    {
        EnsureNoTransformSession();
        if (!_externalPlacements.TryGetValue(
                placementId,
                out SmoLevelExternalPlacement? placement))
            return false;
        Execute(new AddExternalPlacementCommand(
            $"Remove external placement {placement.Name}",
            placement,
            Add: false));
        return true;
    }

    public bool RemoveEntities(IEnumerable<SmoLevelEntityId> entityIds)
    {
        EnsureNoTransformSession();
        ArgumentNullException.ThrowIfNull(entityIds);
        SmoLevelEntityId[] ids = entityIds.Distinct()
            .Where(id => _entities.ContainsKey(id) &&
                         !_generatedCollisions.ContainsKey(id) &&
                         !_removedEntityIds.Contains(id))
            .ToArray();
        if (ids.Length == 0)
            return false;
        Execute(new RemoveEntitiesCommand(
            ids.Length == 1 ? $"Remove entity [{ids[0].SceneObjectIndex}]" :
                $"Remove {ids.Length} entities",
            ids,
            Remove: true));
        return true;
    }

    public SmoLevelEntityId AddGeneratedCollision(
        string name,
        IReadOnlyList<Vector3> positions,
        IReadOnlyList<int> triangleIndices,
        SmoLevelEntityId? sourceVisualEntityId = null,
        Guid? sourcePendingPlacementId = null,
        bool sourcePendingExternal = false)
    {
        EnsureNoTransformSession();
        ArgumentNullException.ThrowIfNull(positions);
        ArgumentNullException.ThrowIfNull(triangleIndices);
        if (positions.Count < 4 || positions.Count > ushort.MaxValue ||
            triangleIndices.Count < 12 || triangleIndices.Count % 3 != 0 ||
            triangleIndices.Any(index => (uint)index >= (uint)positions.Count) ||
            positions.Any(position =>
                !float.IsFinite(position.X) ||
                !float.IsFinite(position.Y) ||
                !float.IsFinite(position.Z)))
        {
            throw new ArgumentException("Generated collision geometry is invalid.");
        }
        if (sourceVisualEntityId is SmoLevelEntityId sourceId &&
            (GetEntity(sourceId).Kind != SmoLevelEntityKind.Visual ||
             _removedEntityIds.Contains(sourceId)))
        {
            throw new ArgumentException(
                "Generated collision source must be a visible visual entity.",
                nameof(sourceVisualEntityId));
        }
        if (sourceVisualEntityId is not null && sourcePendingPlacementId is not null)
            throw new ArgumentException(
                "Generated collision cannot have two movement owners.");
        if (sourcePendingPlacementId is Guid pendingId &&
            (sourcePendingExternal
                ? !_externalPlacements.ContainsKey(pendingId)
                : !_placementAdditions.Values.Any(addition =>
                    addition.GroupId == pendingId)))
        {
            throw new ArgumentException(
                "Generated collision pending-placement owner does not exist.",
                nameof(sourcePendingPlacementId));
        }

        while (_entities.ContainsKey(new SmoLevelEntityId(_nextGeneratedEntityIndex)))
            _nextGeneratedEntityIndex--;
        var id = new SmoLevelEntityId(_nextGeneratedEntityIndex--);
        var generated = new SmoLevelGeneratedCollision(
            id,
            string.IsNullOrWhiteSpace(name) ? "GeneratedCollision" : name.Trim(),
            new ReadOnlyCollection<Vector3>(positions.ToArray()),
            new ReadOnlyCollection<int>(triangleIndices.ToArray()),
            sourceVisualEntityId,
            sourcePendingPlacementId,
            sourcePendingExternal);
        Execute(new AddGeneratedCollisionCommand(
            $"Create collision {generated.Name}",
            generated,
            Add: true));
        return id;
    }

    public bool RemoveGeneratedCollision(SmoLevelEntityId id)
    {
        EnsureNoTransformSession();
        if (!_generatedCollisions.TryGetValue(
                id,
                out SmoLevelGeneratedCollision? collision))
        {
            return false;
        }
        Execute(new AddGeneratedCollisionCommand(
            $"Remove collision {collision.Name}",
            collision,
            Add: false));
        return true;
    }

    public bool ReplaceGeneratedCollision(
        SmoLevelEntityId id,
        IReadOnlyList<Vector3> positions,
        IReadOnlyList<int> triangleIndices)
    {
        EnsureNoTransformSession();
        if (!_generatedCollisions.TryGetValue(
                id,
                out SmoLevelGeneratedCollision? before))
        {
            return false;
        }
        ValidateGeneratedCollisionGeometry(positions, triangleIndices);
        var after = before with
        {
            Positions = new ReadOnlyCollection<Vector3>(positions.ToArray()),
            TriangleIndices = new ReadOnlyCollection<int>(triangleIndices.ToArray())
        };
        Execute(new ReplaceGeneratedCollisionCommand(
            $"Regenerate collision {before.Name}",
            before,
            after));
        return true;
    }

    public bool SetCollisionLink(
        SmoLevelEntityId visualEntityId,
        SmoLevelEntityId collisionEntityId)
    {
        EnsureNoTransformSession();
        SmoLevelEntity visual = GetEntity(visualEntityId);
        SmoLevelEntity collision = GetEntity(collisionEntityId);
        if (visual.Kind != SmoLevelEntityKind.Visual ||
            collision.Kind != SmoLevelEntityKind.Collision)
        {
            throw new ArgumentException(
                "A collision link requires one visual and one collision entity.");
        }
        if (_collisionLinkList.Any(link =>
                link.VisualEntityId == visualEntityId &&
                link.CollisionEntityId == collisionEntityId))
        {
            return false;
        }
        if (!TryCalculateEntityBounds(visual, out SmoLevelBounds visualBounds) ||
            !TryCalculateCollisionEntityBounds(
                collision,
                out SmoLevelBounds collisionBounds))
        {
            throw new InvalidOperationException(
                "Could not calculate bounds for the selected link pair.");
        }
        var link = new SmoLevelCollisionLink(
            visualEntityId,
            collisionEntityId,
            1,
            visualBounds,
            collisionBounds);
        Execute(new SetCollisionLinkCommand(
            $"Link {visual.Name} to {collision.Name}",
            link,
            Present: true));
        return true;
    }

    public bool RemoveCollisionLink(
        SmoLevelEntityId visualEntityId,
        SmoLevelEntityId collisionEntityId)
    {
        EnsureNoTransformSession();
        SmoLevelCollisionLink? link = _collisionLinkList.FirstOrDefault(candidate =>
            candidate.VisualEntityId == visualEntityId &&
            candidate.CollisionEntityId == collisionEntityId);
        if (link is null)
            return false;
        Execute(new SetCollisionLinkCommand(
            $"Unlink [{visualEntityId.SceneObjectIndex}] and " +
            $"[{collisionEntityId.SceneObjectIndex}]",
            link,
            Present: false));
        return true;
    }

    public bool SetEntityTransform(
        SmoLevelEntityId id,
        Matrix4x4 worldTransform,
        string description = "Transform entity")
    {
        EnsureNoTransformSession();
        ValidateTransform(worldTransform);
        SmoLevelEntity entity = GetEntity(id);
        if (entity.WorldTransform.Equals(worldTransform))
            return false;
        ValidatePersistableTransform(entity, worldTransform);

        Execute(new TransformEntitiesCommand(
            NormalizeDescription(description),
            new Dictionary<SmoLevelEntityId, Matrix4x4>
            {
                [id] = entity.WorldTransform
            },
            new Dictionary<SmoLevelEntityId, Matrix4x4>
            {
                [id] = worldTransform
            }));
        return true;
    }

    public SmoTransformSession BeginTransform(
        IEnumerable<SmoLevelEntityId> entityIds)
    {
        ArgumentNullException.ThrowIfNull(entityIds);
        EnsureNoTransformSession();
        Dictionary<SmoLevelEntityId, Matrix4x4> originals = entityIds
            .Distinct()
            .ToDictionary(id => id, id => GetEntity(id).WorldTransform);
        if (originals.Count == 0)
            throw new ArgumentException("A transform session needs at least one entity.");

        _activeTransformSession = new SmoTransformSession(this, originals);
        return _activeTransformSession;
    }

    public SmoPendingPlacementTransformSession BeginPendingPlacementTransform(
        Guid placementOrGroupId,
        bool external)
    {
        EnsureNoTransformSession();
        Dictionary<Guid, Matrix4x4> originals = external
            ? _externalPlacements.Values
                .Where(placement => placement.Id == placementOrGroupId)
                .ToDictionary(placement => placement.Id, placement =>
                    placement.WorldTransform)
            : _placementAdditions.Values
                .Where(placement => placement.GroupId == placementOrGroupId)
                .ToDictionary(placement => placement.Id, placement =>
                    placement.WorldTransform);
        if (originals.Count == 0)
        {
            throw new KeyNotFoundException(
                external
                    ? $"External placement {placementOrGroupId} is no longer present."
                    : $"Shared placement group {placementOrGroupId} is no longer present.");
        }

        _activePendingTransformSession = new SmoPendingPlacementTransformSession(
            this,
            external,
            originals);
        return _activePendingTransformSession;
    }

    public bool Undo()
    {
        if (!CanUndo)
            return false;
        IEditCommand command = _history[--_historyPosition];
        command.Undo(this);
        NotifyChanged();
        return true;
    }

    public bool Redo()
    {
        if (!CanRedo)
            return false;
        IEditCommand command = _history[_historyPosition++];
        command.Execute(this);
        NotifyChanged();
        return true;
    }

    public void MarkSaved()
    {
        EnsureNoTransformSession();
        _savedHistoryPosition = _historyPosition;
        NotifyChanged(incrementRevision: false);
    }

    internal void PreviewTransform(
        SmoTransformSession session,
        IReadOnlyDictionary<SmoLevelEntityId, Matrix4x4> transforms)
    {
        EnsureActiveSession(session);
        SetEntityTransforms(transforms);
        NotifyChanged();
    }

    internal bool CommitTransform(
        SmoTransformSession session,
        IReadOnlyDictionary<SmoLevelEntityId, Matrix4x4> originals,
        IReadOnlyDictionary<SmoLevelEntityId, Matrix4x4> finalTransforms,
        string description)
    {
        EnsureActiveSession(session);
        _activeTransformSession = null;
        bool changed = originals.Any(pair =>
            !finalTransforms[pair.Key].Equals(pair.Value));
        if (!changed)
        {
            SetEntityTransforms(originals);
            NotifyChanged();
            return false;
        }

        foreach ((SmoLevelEntityId id, Matrix4x4 finalTransform) in finalTransforms)
        {
            if (!finalTransform.Equals(originals[id]))
                ValidatePersistableTransform(GetEntity(id), finalTransform);
        }

        SetEntityTransforms(originals);
        Execute(new TransformEntitiesCommand(
            NormalizeDescription(description),
            new Dictionary<SmoLevelEntityId, Matrix4x4>(originals),
            new Dictionary<SmoLevelEntityId, Matrix4x4>(finalTransforms)));
        return true;
    }

    private void ValidatePersistableTransform(
        SmoLevelEntity entity,
        Matrix4x4 worldTransform)
    {
        if (_generatedCollisions.ContainsKey(entity.Id))
            return;
        SmoPlacementTransformWriter.ValidateEdit(
            Workspace.Document,
            new SmoPlacementTransformEdit(
                entity.Id.SceneObjectIndex,
                entity.OriginalWorldTransform,
                worldTransform));
    }

    internal void CancelTransform(
        SmoTransformSession session,
        IReadOnlyDictionary<SmoLevelEntityId, Matrix4x4> originals)
    {
        EnsureActiveSession(session);
        _activeTransformSession = null;
        SetEntityTransforms(originals);
        NotifyChanged();
    }

    internal void PreviewPendingPlacementTransform(
        SmoPendingPlacementTransformSession session,
        IReadOnlyDictionary<Guid, Matrix4x4> transforms)
    {
        EnsureActivePendingSession(session);
        SetPendingPlacementTransforms(session.External, transforms);
        NotifyChanged();
    }

    internal bool CommitPendingPlacementTransform(
        SmoPendingPlacementTransformSession session,
        IReadOnlyDictionary<Guid, Matrix4x4> originals,
        IReadOnlyDictionary<Guid, Matrix4x4> finalTransforms,
        string description)
    {
        EnsureActivePendingSession(session);
        _activePendingTransformSession = null;
        bool changed = originals.Any(pair =>
            !finalTransforms[pair.Key].Equals(pair.Value));
        if (!changed)
        {
            SetPendingPlacementTransforms(session.External, originals);
            NotifyChanged();
            return false;
        }

        SetPendingPlacementTransforms(session.External, originals);
        Execute(new TransformPendingPlacementsCommand(
            NormalizeDescription(description),
            session.External,
            new Dictionary<Guid, Matrix4x4>(originals),
            new Dictionary<Guid, Matrix4x4>(finalTransforms)));
        return true;
    }

    internal void CancelPendingPlacementTransform(
        SmoPendingPlacementTransformSession session,
        IReadOnlyDictionary<Guid, Matrix4x4> originals)
    {
        EnsureActivePendingSession(session);
        _activePendingTransformSession = null;
        SetPendingPlacementTransforms(session.External, originals);
        NotifyChanged();
    }

    private void Execute(IEditCommand command)
    {
        if (_historyPosition < _history.Count)
        {
            _history.RemoveRange(
                _historyPosition,
                _history.Count - _historyPosition);
            if (_savedHistoryPosition > _historyPosition)
                _savedHistoryPosition = -1;
        }

        command.Execute(this);
        _history.Add(command);
        _historyPosition++;
        NotifyChanged();
    }

    private void SetEntityTransforms(
        IReadOnlyDictionary<SmoLevelEntityId, Matrix4x4> transforms)
    {
        foreach ((SmoLevelEntityId id, Matrix4x4 transform) in transforms)
        {
            ValidateTransform(transform);
            SmoLevelEntity entity = GetEntity(id);
            entity.WorldTransform = transform;
            foreach (SmoEditablePlacement part in entity.Parts)
                part.WorldTransform = transform;
            foreach (SmoEditableCollision collision in entity.Collisions)
                collision.WorldTransform = transform;
        }
    }

    private void SetPendingPlacementTransforms(
        bool external,
        IReadOnlyDictionary<Guid, Matrix4x4> transforms)
    {
        SynchronizeGeneratedCollisionsWithPendingPlacements(external, transforms);
        foreach ((Guid id, Matrix4x4 transform) in transforms)
        {
            ValidateTransform(transform);
            if (external)
            {
                if (!_externalPlacements.TryGetValue(
                        id,
                        out SmoLevelExternalPlacement? placement))
                    throw new KeyNotFoundException(
                        $"External placement {id} is no longer present.");
                _externalPlacements[id] = placement with
                {
                    WorldTransform = transform
                };
            }
            else
            {
                if (!_placementAdditions.TryGetValue(
                        id,
                        out SmoLevelPlacementAddition? placement))
                    throw new KeyNotFoundException(
                        $"Shared placement {id} is no longer present.");
                _placementAdditions[id] = placement with
                {
                    WorldTransform = transform
                };
            }
        }
    }

    private void SynchronizeGeneratedCollisionsWithPendingPlacements(
        bool external,
        IReadOnlyDictionary<Guid, Matrix4x4> transforms)
    {
        foreach (SmoLevelGeneratedCollision generated in
                 _generatedCollisions.Values.Where(item =>
                     item.SourcePendingPlacementId is not null &&
                     item.SourcePendingExternal == external))
        {
            Matrix4x4 before;
            Matrix4x4 after;
            if (external)
            {
                Guid placementId = generated.SourcePendingPlacementId!.Value;
                if (!_externalPlacements.TryGetValue(
                        placementId,
                        out SmoLevelExternalPlacement? placement) ||
                    !transforms.TryGetValue(placementId, out after))
                {
                    continue;
                }
                before = placement.WorldTransform;
            }
            else
            {
                Guid groupId = generated.SourcePendingPlacementId!.Value;
                SmoLevelPlacementAddition? placement = _placementAdditions.Values
                    .FirstOrDefault(item =>
                        item.GroupId == groupId && transforms.ContainsKey(item.Id));
                if (placement is null ||
                    !transforms.TryGetValue(placement.Id, out after))
                {
                    continue;
                }
                before = placement.WorldTransform;
            }
            if (!Matrix4x4.Invert(before, out Matrix4x4 inverseBefore))
                throw new InvalidOperationException(
                    "Pending placement has a singular transform.");
            Matrix4x4 delta = inverseBefore * after;
            SmoLevelEntity collisionEntity = GetEntity(generated.EntityId);
            Matrix4x4 desired = collisionEntity.WorldTransform * delta;
            ValidateTransform(desired);
            collisionEntity.WorldTransform = desired;
            foreach (SmoEditableCollision collision in collisionEntity.Collisions)
                collision.WorldTransform = desired;
        }
    }

    private void SetTextureReplacement(
        int textureObjectIndex,
        SmoLevelTextureReplacement? replacement)
    {
        if (replacement is null)
            _textureReplacements.Remove(textureObjectIndex);
        else
            _textureReplacements[textureObjectIndex] = replacement;
    }

    private void SetModelReplacement(
        int meshObjectIndex,
        SmoLevelModelReplacement? replacement)
    {
        if (replacement is null)
            _modelReplacements.Remove(meshObjectIndex);
        else
            _modelReplacements[meshObjectIndex] = replacement;
    }

    private void SetPlacementAddition(
        SmoLevelPlacementAddition addition,
        bool present)
    {
        if (present)
            _placementAdditions[addition.Id] = addition;
        else
            _placementAdditions.Remove(addition.Id);
    }

    private void SetExternalModel(SmoLevelExternalModel model, bool present)
    {
        if (present)
            _externalModels[model.Id] = model;
        else
            _externalModels.Remove(model.Id);
    }

    private void SetExternalPlacement(
        SmoLevelExternalPlacement placement,
        bool present)
    {
        if (present)
            _externalPlacements[placement.Id] = placement;
        else
            _externalPlacements.Remove(placement.Id);
    }

    private void SetCompositeReplacement(
        SmoLevelExternalModel model,
        IReadOnlyList<SmoLevelExternalPlacement> placements,
        IReadOnlyList<SmoLevelEntityId> removedEntityIds,
        bool present)
    {
        if (present)
        {
            SetExternalModel(model, present: true);
            foreach (SmoLevelExternalPlacement placement in placements)
                SetExternalPlacement(placement, present: true);
            SetEntitiesRemoved(removedEntityIds, removed: true);
        }
        else
        {
            SetEntitiesRemoved(removedEntityIds, removed: false);
            foreach (SmoLevelExternalPlacement placement in placements)
                SetExternalPlacement(placement, present: false);
            SetExternalModel(model, present: false);
        }
    }

    private void SetGeneratedCollision(
        SmoLevelGeneratedCollision generated,
        bool present)
    {
        if (present)
        {
            var source = new SmoCollisionMesh(
                -1,
                generated.EntityId.SceneObjectIndex,
                -1,
                generated.Name,
                generated.Positions,
                generated.TriangleIndices,
                Matrix4x4.Identity);
            var collision = new SmoEditableCollision(
                source,
                Matrix4x4.Identity,
                Matrix4x4.Identity);
            var entity = new SmoLevelEntity(
                generated.EntityId,
                generated.Name,
                Array.Empty<SmoEditablePlacement>(),
                new ReadOnlyCollection<SmoEditableCollision>([collision]),
                Matrix4x4.Identity,
                Matrix4x4.Identity);
            collision.Entity = entity;
            _generatedCollisions.Add(generated.EntityId, generated);
            _collisions.Add(collision);
            _entities.Add(generated.EntityId, entity);
            _entityList.Add(entity);

            if (generated.SourceVisualEntityId is SmoLevelEntityId sourceId &&
                TryCalculateEntityBounds(GetEntity(sourceId), out SmoLevelBounds visualBounds))
            {
                SmoLevelBounds collisionBounds = CalculateBounds(generated.Positions);
                _collisionLinkList.Add(new SmoLevelCollisionLink(
                    sourceId,
                    generated.EntityId,
                    1,
                    visualBounds,
                    collisionBounds));
            }
        }
        else
        {
            _generatedCollisions.Remove(generated.EntityId);
            _collisions.RemoveAll(collision =>
                collision.Entity.Id == generated.EntityId);
            if (_entities.Remove(generated.EntityId, out SmoLevelEntity? entity))
                _entityList.Remove(entity);
            _collisionLinkList.RemoveAll(link =>
                link.CollisionEntityId == generated.EntityId);
        }
        RebuildCollisionLinkLookup();
    }

    private void ReplaceGeneratedCollisionState(
        SmoLevelGeneratedCollision before,
        SmoLevelGeneratedCollision after,
        bool useAfter)
    {
        SetGeneratedCollision(useAfter ? before : after, present: false);
        SetGeneratedCollision(useAfter ? after : before, present: true);
    }

    private void SetCollisionLinkState(
        SmoLevelCollisionLink link,
        bool present)
    {
        if (present)
        {
            if (!_collisionLinkList.Any(candidate =>
                    candidate.VisualEntityId == link.VisualEntityId &&
                    candidate.CollisionEntityId == link.CollisionEntityId))
            {
                _collisionLinkList.Add(link);
            }
        }
        else
        {
            _collisionLinkList.RemoveAll(candidate =>
                candidate.VisualEntityId == link.VisualEntityId &&
                candidate.CollisionEntityId == link.CollisionEntityId);
        }
        RebuildCollisionLinkLookup();
    }

    private static void ValidateGeneratedCollisionGeometry(
        IReadOnlyList<Vector3> positions,
        IReadOnlyList<int> triangleIndices)
    {
        ArgumentNullException.ThrowIfNull(positions);
        ArgumentNullException.ThrowIfNull(triangleIndices);
        if (positions.Count < 4 || positions.Count > ushort.MaxValue ||
            triangleIndices.Count < 12 || triangleIndices.Count % 3 != 0 ||
            triangleIndices.Any(index => (uint)index >= (uint)positions.Count) ||
            positions.Any(position =>
                !float.IsFinite(position.X) ||
                !float.IsFinite(position.Y) ||
                !float.IsFinite(position.Z)))
        {
            throw new ArgumentException("Generated collision geometry is invalid.");
        }
    }

    private bool TryCalculateEntityBounds(
        SmoLevelEntity entity,
        out SmoLevelBounds bounds)
    {
        var points = new List<Vector3>();
        foreach (SmoSceneMesh mesh in Workspace.PreparedScene.Meshes)
        {
            var placementId = new SmoPlacementId(
                mesh.Mesh.ObjectIndex,
                mesh.SceneObjectIndex);
            if (!_placements.TryGetValue(
                    placementId,
                    out SmoEditablePlacement? placement) ||
                placement.Entity.Id != entity.Id)
            {
                continue;
            }
            points.AddRange(mesh.Mesh.Positions.Select(position =>
                Vector3.Transform(position, entity.WorldTransform)));
        }
        if (points.Count == 0)
        {
            bounds = default;
            return false;
        }
        bounds = CalculateBounds(points);
        return true;
    }

    private static bool TryCalculateCollisionEntityBounds(
        SmoLevelEntity entity,
        out SmoLevelBounds bounds)
    {
        var points = entity.Collisions
            .SelectMany(collision => collision.Source.Positions.Select(position =>
                Vector3.Transform(position, collision.WorldTransform)))
            .ToArray();
        if (points.Length == 0)
        {
            bounds = default;
            return false;
        }
        bounds = CalculateBounds(points);
        return true;
    }

    private static SmoLevelBounds CalculateBounds(IEnumerable<Vector3> positions)
    {
        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);
        foreach (Vector3 position in positions)
        {
            minimum = Vector3.Min(minimum, position);
            maximum = Vector3.Max(maximum, position);
        }
        return new SmoLevelBounds(minimum, maximum);
    }

    private void RebuildCollisionLinkLookup()
    {
        _collisionLinks.Clear();
        foreach (var group in _collisionLinkList
                     .SelectMany(link => new[]
                     {
                         (Id: link.VisualEntityId, Link: link),
                         (Id: link.CollisionEntityId, Link: link)
                     })
                     .GroupBy(pair => pair.Item1))
        {
            _collisionLinks[group.Key] = group.Select(pair => pair.Link).ToArray();
        }
    }

    private void SetEntitiesRemoved(
        IReadOnlyList<SmoLevelEntityId> entityIds,
        bool removed)
    {
        foreach (SmoLevelEntityId id in entityIds)
        {
            if (removed)
                _removedEntityIds.Add(id);
            else
                _removedEntityIds.Remove(id);
        }
    }

    private void NotifyChanged(bool incrementRevision = true)
    {
        if (incrementRevision)
            Revision++;
        Changed?.Invoke(this, EventArgs.Empty);
    }

    private void EnsureNoTransformSession()
    {
        if (_activeTransformSession is not null ||
            _activePendingTransformSession is not null)
        {
            throw new InvalidOperationException(
                "Complete or cancel the active transform session first.");
        }
    }

    private void EnsureActiveSession(SmoTransformSession session)
    {
        if (!ReferenceEquals(_activeTransformSession, session))
            throw new InvalidOperationException("The transform session is no longer active.");
    }

    private void EnsureActivePendingSession(
        SmoPendingPlacementTransformSession session)
    {
        if (!ReferenceEquals(_activePendingTransformSession, session))
        {
            throw new InvalidOperationException(
                "The pending placement transform session is no longer active.");
        }
    }

    private static string NormalizeDescription(string description) =>
        string.IsNullOrWhiteSpace(description)
            ? "Transform entities"
            : description;

    private static void ValidateTransform(Matrix4x4 transform)
    {
        ReadOnlySpan<float> values =
        [
            transform.M11, transform.M12, transform.M13, transform.M14,
            transform.M21, transform.M22, transform.M23, transform.M24,
            transform.M31, transform.M32, transform.M33, transform.M34,
            transform.M41, transform.M42, transform.M43, transform.M44
        ];
        foreach (float value in values)
        {
            if (!float.IsFinite(value))
                throw new ArgumentException("Placement transform must contain finite values.");
        }
    }

    private interface IEditCommand
    {
        string Description { get; }
        void Execute(SmoLevelDocument document);
        void Undo(SmoLevelDocument document);
    }

    private sealed record TransformEntitiesCommand(
        string Description,
        IReadOnlyDictionary<SmoLevelEntityId, Matrix4x4> Before,
        IReadOnlyDictionary<SmoLevelEntityId, Matrix4x4> After) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetEntityTransforms(After);

        public void Undo(SmoLevelDocument document) =>
            document.SetEntityTransforms(Before);
    }

    private sealed record TransformPendingPlacementsCommand(
        string Description,
        bool External,
        IReadOnlyDictionary<Guid, Matrix4x4> Before,
        IReadOnlyDictionary<Guid, Matrix4x4> After) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetPendingPlacementTransforms(External, After);

        public void Undo(SmoLevelDocument document) =>
            document.SetPendingPlacementTransforms(External, Before);
    }

    private sealed record ReplaceTextureCommand(
        string Description,
        int TextureObjectIndex,
        SmoLevelTextureReplacement? Before,
        SmoLevelTextureReplacement After) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetTextureReplacement(TextureObjectIndex, After);

        public void Undo(SmoLevelDocument document) =>
            document.SetTextureReplacement(TextureObjectIndex, Before);
    }

    private sealed record ReplaceModelCommand(
        string Description,
        int MeshObjectIndex,
        SmoLevelModelReplacement? Before,
        SmoLevelModelReplacement After) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetModelReplacement(MeshObjectIndex, After);

        public void Undo(SmoLevelDocument document) =>
            document.SetModelReplacement(MeshObjectIndex, Before);
    }

    private sealed record AddPlacementsCommand(
        string Description,
        IReadOnlyList<SmoLevelPlacementAddition> Additions,
        bool Add = true) : IEditCommand
    {
        public void Execute(SmoLevelDocument document)
        {
            foreach (SmoLevelPlacementAddition addition in Additions)
                document.SetPlacementAddition(addition, present: Add);
        }

        public void Undo(SmoLevelDocument document)
        {
            foreach (SmoLevelPlacementAddition addition in Additions)
                document.SetPlacementAddition(addition, present: !Add);
        }
    }

    private sealed record AddExternalModelCommand(
        string Description,
        SmoLevelExternalModel Model) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetExternalModel(Model, present: true);

        public void Undo(SmoLevelDocument document) =>
            document.SetExternalModel(Model, present: false);
    }

    private sealed record AddExternalPlacementCommand(
        string Description,
        SmoLevelExternalPlacement Placement,
        bool Add) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetExternalPlacement(Placement, present: Add);

        public void Undo(SmoLevelDocument document) =>
            document.SetExternalPlacement(Placement, present: !Add);
    }

    private sealed record ReplaceCompositeModelsCommand(
        string Description,
        SmoLevelExternalModel Model,
        IReadOnlyList<SmoLevelExternalPlacement> Placements,
        IReadOnlyList<SmoLevelEntityId> RemovedEntityIds) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetCompositeReplacement(
                Model,
                Placements,
                RemovedEntityIds,
                present: true);

        public void Undo(SmoLevelDocument document) =>
            document.SetCompositeReplacement(
                Model,
                Placements,
                RemovedEntityIds,
                present: false);
    }

    private sealed record RemoveEntitiesCommand(
        string Description,
        IReadOnlyList<SmoLevelEntityId> EntityIds,
        bool Remove) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetEntitiesRemoved(EntityIds, Remove);

        public void Undo(SmoLevelDocument document) =>
            document.SetEntitiesRemoved(EntityIds, !Remove);
    }

    private sealed record AddGeneratedCollisionCommand(
        string Description,
        SmoLevelGeneratedCollision Collision,
        bool Add) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetGeneratedCollision(Collision, Add);

        public void Undo(SmoLevelDocument document) =>
            document.SetGeneratedCollision(Collision, !Add);
    }

    private sealed record ReplaceGeneratedCollisionCommand(
        string Description,
        SmoLevelGeneratedCollision Before,
        SmoLevelGeneratedCollision After) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.ReplaceGeneratedCollisionState(Before, After, useAfter: true);

        public void Undo(SmoLevelDocument document) =>
            document.ReplaceGeneratedCollisionState(Before, After, useAfter: false);
    }

    private sealed record SetCollisionLinkCommand(
        string Description,
        SmoLevelCollisionLink Link,
        bool Present) : IEditCommand
    {
        public void Execute(SmoLevelDocument document) =>
            document.SetCollisionLinkState(Link, Present);

        public void Undo(SmoLevelDocument document) =>
            document.SetCollisionLinkState(Link, !Present);
    }
}

public sealed record SmoLevelTextureReplacement(
    int TextureObjectIndex,
    ReadOnlyMemory<byte> EncodedImage,
    string SourcePath,
    bool ReplaceAlpha)
{
    internal bool ContentEquals(SmoLevelTextureReplacement other) =>
        TextureObjectIndex == other.TextureObjectIndex &&
        ReplaceAlpha == other.ReplaceAlpha &&
        EncodedImage.Span.SequenceEqual(other.EncodedImage.Span);
}

public sealed record SmoLevelModelReplacement(
    int MeshObjectIndex,
    ImportedScene ImportedScene,
    ReplacementTransform Transform,
    Matrix4x4 ReferenceWorldTransform,
    string SourcePath)
{
    internal bool ContentEquals(SmoLevelModelReplacement other) =>
        MeshObjectIndex == other.MeshObjectIndex &&
        Transform == other.Transform &&
        ReferenceWorldTransform.Equals(other.ReferenceWorldTransform) &&
        string.Equals(SourcePath, other.SourcePath, StringComparison.OrdinalIgnoreCase);
}

public sealed record SmoLevelPlacementAddition(
    Guid Id,
    Guid GroupId,
    int MeshObjectIndex,
    Matrix4x4 WorldTransform,
    string Name);

public sealed record SmoSharedPlacementRequest(
    int MeshObjectIndex,
    Matrix4x4 WorldTransform,
    string Name);

public sealed record SmoLevelExternalModel(
    Guid Id,
    ImportedScene ImportedScene,
    string SourcePath,
    string Name,
    int TemplateMeshObjectIndex)
{
    // glTF 2.0 defines linear distances in metres, while authored Winx level
    // geometry uses centimetre-scale world coordinates.
    public float SuggestedPlacementScale => Path.GetExtension(SourcePath).Equals(
        ".glb", StringComparison.OrdinalIgnoreCase)
            ? 100f
            : 1f;
}

public sealed record SmoLevelExternalPlacement(
    Guid Id,
    Guid ModelId,
    Matrix4x4 WorldTransform,
    string Name);

public sealed record SmoLevelGeneratedCollision(
    SmoLevelEntityId EntityId,
    string Name,
    IReadOnlyList<Vector3> Positions,
    IReadOnlyList<int> TriangleIndices,
    SmoLevelEntityId? SourceVisualEntityId,
    Guid? SourcePendingPlacementId,
    bool SourcePendingExternal);

public sealed class SmoPendingPlacementTransformSession : IDisposable
{
    private readonly SmoLevelDocument _document;
    private readonly IReadOnlyDictionary<Guid, Matrix4x4> _originals;
    private Dictionary<Guid, Matrix4x4> _current;

    internal SmoPendingPlacementTransformSession(
        SmoLevelDocument document,
        bool external,
        IReadOnlyDictionary<Guid, Matrix4x4> originals)
    {
        _document = document;
        External = external;
        _originals = new Dictionary<Guid, Matrix4x4>(originals);
        _current = new Dictionary<Guid, Matrix4x4>(originals);
        PlacementIds = new ReadOnlyCollection<Guid>(originals.Keys.ToArray());
    }

    public bool External { get; }
    public IReadOnlyList<Guid> PlacementIds { get; }
    public Vector3 TranslationDelta { get; private set; }
    public Vector3 RotationAxis { get; private set; }
    public float RotationRadians { get; private set; }
    public Vector3 ScaleFactors { get; private set; } = Vector3.One;
    public Vector3 TransformPivot { get; private set; }
    public bool IsActive { get; private set; } = true;

    public void PreviewTranslation(Vector3 worldDelta)
    {
        EnsureActive();
        ValidateVector(worldDelta, nameof(worldDelta));
        TranslationDelta = worldDelta;
        _current = _originals.ToDictionary(
            pair => pair.Key,
            pair => WithTranslationDelta(pair.Value, worldDelta));
        _document.PreviewPendingPlacementTransform(this, _current);
    }

    public void PreviewRotation(
        Vector3 worldAxis,
        float radians,
        Vector3 worldPivot)
    {
        EnsureActive();
        ValidateVector(worldAxis, nameof(worldAxis));
        ValidateVector(worldPivot, nameof(worldPivot));
        if (!float.IsFinite(radians) || worldAxis.LengthSquared() < 1e-12f)
            throw new ArgumentException("Rotation needs a finite angle and axis.");

        RotationAxis = Vector3.Normalize(worldAxis);
        RotationRadians = radians;
        TransformPivot = worldPivot;
        Matrix4x4 worldDelta =
            Matrix4x4.CreateTranslation(-worldPivot) *
            Matrix4x4.CreateFromAxisAngle(RotationAxis, radians) *
            Matrix4x4.CreateTranslation(worldPivot);
        PreviewWorldDelta(worldDelta);
    }

    public void PreviewScale(
        Vector3 factors,
        Quaternion worldOrientation,
        Vector3 worldPivot)
    {
        EnsureActive();
        ValidateVector(factors, nameof(factors));
        ValidateVector(worldPivot, nameof(worldPivot));
        if (factors.X <= 0 || factors.Y <= 0 || factors.Z <= 0 ||
            !float.IsFinite(worldOrientation.X) ||
            !float.IsFinite(worldOrientation.Y) ||
            !float.IsFinite(worldOrientation.Z) ||
            !float.IsFinite(worldOrientation.W) ||
            worldOrientation.LengthSquared() < 1e-12f)
        {
            throw new ArgumentException(
                "Scale needs positive finite factors and a finite orientation.");
        }

        worldOrientation = Quaternion.Normalize(worldOrientation);
        ScaleFactors = factors;
        TransformPivot = worldPivot;
        Matrix4x4 orientation = Matrix4x4.CreateFromQuaternion(worldOrientation);
        if (!Matrix4x4.Invert(orientation, out Matrix4x4 inverseOrientation))
            throw new ArgumentException("Scale orientation is singular.");
        Matrix4x4 worldDelta =
            Matrix4x4.CreateTranslation(-worldPivot) *
            inverseOrientation *
            Matrix4x4.CreateScale(factors) *
            orientation *
            Matrix4x4.CreateTranslation(worldPivot);
        PreviewWorldDelta(worldDelta);
    }

    public bool Commit(string description = "Move pending placement")
    {
        EnsureActive();
        IsActive = false;
        return _document.CommitPendingPlacementTransform(
            this,
            _originals,
            _current,
            description);
    }

    public void Cancel()
    {
        if (!IsActive)
            return;
        IsActive = false;
        _document.CancelPendingPlacementTransform(this, _originals);
    }

    public void Dispose() => Cancel();

    private void EnsureActive()
    {
        if (!IsActive)
        {
            throw new InvalidOperationException(
                "The pending placement transform session has completed.");
        }
    }

    private void PreviewWorldDelta(Matrix4x4 worldDelta)
    {
        _current = _originals.ToDictionary(
            pair => pair.Key,
            pair => pair.Value * worldDelta);
        _document.PreviewPendingPlacementTransform(this, _current);
    }

    private static void ValidateVector(Vector3 value, string parameterName)
    {
        if (!float.IsFinite(value.X) ||
            !float.IsFinite(value.Y) ||
            !float.IsFinite(value.Z))
        {
            throw new ArgumentException(
                "Vector must contain finite values.",
                parameterName);
        }
    }

    private static Matrix4x4 WithTranslationDelta(
        Matrix4x4 transform,
        Vector3 delta)
    {
        transform.M41 += delta.X;
        transform.M42 += delta.Y;
        transform.M43 += delta.Z;
        return transform;
    }
}

public sealed class SmoTransformSession : IDisposable
{
    private readonly SmoLevelDocument _document;
    private readonly IReadOnlyDictionary<SmoLevelEntityId, Matrix4x4> _originals;
    private Dictionary<SmoLevelEntityId, Matrix4x4> _current;

    internal SmoTransformSession(
        SmoLevelDocument document,
        IReadOnlyDictionary<SmoLevelEntityId, Matrix4x4> originals)
    {
        _document = document;
        _originals = new Dictionary<SmoLevelEntityId, Matrix4x4>(originals);
        _current = new Dictionary<SmoLevelEntityId, Matrix4x4>(originals);
        EntityIds = new ReadOnlyCollection<SmoLevelEntityId>(
            originals.Keys.ToArray());
    }

    public IReadOnlyList<SmoLevelEntityId> EntityIds { get; }
    public Vector3 TranslationDelta { get; private set; }
    public Vector3 RotationAxis { get; private set; }
    public float RotationRadians { get; private set; }
    public Vector3 ScaleFactors { get; private set; } = Vector3.One;
    public Vector3 TransformPivot { get; private set; }
    public bool IsActive { get; private set; } = true;

    public void PreviewTranslation(Vector3 worldDelta)
    {
        EnsureActive();
        if (!float.IsFinite(worldDelta.X) ||
            !float.IsFinite(worldDelta.Y) ||
            !float.IsFinite(worldDelta.Z))
        {
            throw new ArgumentException("Translation delta must be finite.");
        }

        TranslationDelta = worldDelta;
        _current = _originals.ToDictionary(
            pair => pair.Key,
            pair => WithTranslationDelta(pair.Value, worldDelta));
        _document.PreviewTransform(this, _current);
    }

    public void PreviewRotation(
        Vector3 worldAxis,
        float radians,
        Vector3 worldPivot)
    {
        EnsureActive();
        ValidateVector(worldAxis, nameof(worldAxis));
        ValidateVector(worldPivot, nameof(worldPivot));
        if (!float.IsFinite(radians) || worldAxis.LengthSquared() < 1e-12f)
            throw new ArgumentException("Rotation needs a finite angle and axis.");

        RotationAxis = Vector3.Normalize(worldAxis);
        RotationRadians = radians;
        TransformPivot = worldPivot;
        Matrix4x4 worldDelta =
            Matrix4x4.CreateTranslation(-worldPivot) *
            Matrix4x4.CreateFromAxisAngle(RotationAxis, radians) *
            Matrix4x4.CreateTranslation(worldPivot);
        PreviewWorldDelta(worldDelta);
    }

    public void PreviewScale(
        Vector3 factors,
        Quaternion worldOrientation,
        Vector3 worldPivot)
    {
        EnsureActive();
        ValidateVector(factors, nameof(factors));
        ValidateVector(worldPivot, nameof(worldPivot));
        if (factors.X <= 0 || factors.Y <= 0 || factors.Z <= 0 ||
            !float.IsFinite(worldOrientation.X) ||
            !float.IsFinite(worldOrientation.Y) ||
            !float.IsFinite(worldOrientation.Z) ||
            !float.IsFinite(worldOrientation.W) ||
            worldOrientation.LengthSquared() < 1e-12f)
        {
            throw new ArgumentException(
                "Scale needs positive finite factors and a finite orientation.");
        }

        worldOrientation = Quaternion.Normalize(worldOrientation);
        ScaleFactors = factors;
        TransformPivot = worldPivot;
        Matrix4x4 orientation = Matrix4x4.CreateFromQuaternion(worldOrientation);
        if (!Matrix4x4.Invert(orientation, out Matrix4x4 inverseOrientation))
            throw new ArgumentException("Scale orientation is singular.");
        Matrix4x4 worldDelta =
            Matrix4x4.CreateTranslation(-worldPivot) *
            inverseOrientation *
            Matrix4x4.CreateScale(factors) *
            orientation *
            Matrix4x4.CreateTranslation(worldPivot);
        PreviewWorldDelta(worldDelta);
    }

    public bool Commit(string description = "Move entities")
    {
        EnsureActive();
        bool changed = _document.CommitTransform(
            this,
            _originals,
            _current,
            description);
        IsActive = false;
        return changed;
    }

    public void Cancel()
    {
        if (!IsActive)
            return;
        IsActive = false;
        _document.CancelTransform(this, _originals);
    }

    public void Dispose() => Cancel();

    private void EnsureActive()
    {
        if (!IsActive)
            throw new InvalidOperationException("The transform session has completed.");
    }

    private void PreviewWorldDelta(Matrix4x4 worldDelta)
    {
        _current = _originals.ToDictionary(
            pair => pair.Key,
            pair => pair.Value * worldDelta);
        _document.PreviewTransform(this, _current);
    }

    private static void ValidateVector(Vector3 value, string parameterName)
    {
        if (!float.IsFinite(value.X) ||
            !float.IsFinite(value.Y) ||
            !float.IsFinite(value.Z))
        {
            throw new ArgumentException("Vector must contain finite values.", parameterName);
        }
    }

    private static Matrix4x4 WithTranslationDelta(
        Matrix4x4 transform,
        Vector3 delta)
    {
        transform.M41 += delta.X;
        transform.M42 += delta.Y;
        transform.M43 += delta.Z;
        return transform;
    }
}

public sealed class SmoLevelEntity
{
    internal SmoLevelEntity(
        SmoLevelEntityId id,
        string name,
        IReadOnlyList<SmoEditablePlacement> parts,
        IReadOnlyList<SmoEditableCollision> collisions,
        Matrix4x4 originalWorldTransform,
        Matrix4x4 worldTransform)
    {
        Id = id;
        Name = string.IsNullOrWhiteSpace(name)
            ? $"Entity_{id.SceneObjectIndex}"
            : name;
        Parts = parts;
        Collisions = collisions;
        OriginalWorldTransform = originalWorldTransform;
        WorldTransform = worldTransform;
    }

    public SmoLevelEntityId Id { get; }
    public string Name { get; }
    public IReadOnlyList<SmoEditablePlacement> Parts { get; }
    public IReadOnlyList<SmoEditableCollision> Collisions { get; }
    public SmoLevelEntityKind Kind => Collisions.Count > 0
        ? SmoLevelEntityKind.Collision
        : SmoLevelEntityKind.Visual;
    public Matrix4x4 OriginalWorldTransform { get; }
    public Matrix4x4 WorldTransform { get; internal set; }
    public bool IsModified => !WorldTransform.Equals(OriginalWorldTransform);
}

/// <summary>
/// A model that the original authoring pipeline split into numbered sibling
/// meshes. It can use separate static render objects or baked geometry inside
/// one partition renderable; the source entities remain independent.
/// </summary>
public sealed class SmoCompositeModel
{
    internal SmoCompositeModel(
        string name,
        int parentObjectIndex,
        Matrix4x4 worldTransform,
        IReadOnlyList<SmoLevelEntity> entities)
    {
        Name = name;
        ParentObjectIndex = parentObjectIndex;
        WorldTransform = worldTransform;
        Entities = entities;
        Parts = new ReadOnlyCollection<SmoEditablePlacement>(
            entities.SelectMany(entity => entity.Parts).ToArray());
    }

    public string Name { get; }
    public int ParentObjectIndex { get; }
    public Matrix4x4 WorldTransform { get; }
    public IReadOnlyList<SmoLevelEntity> Entities { get; }
    public IReadOnlyList<SmoEditablePlacement> Parts { get; }
}

public sealed class SmoEditableCollision
{
    internal SmoEditableCollision(
        SmoCollisionMesh source,
        Matrix4x4 originalWorldTransform,
        Matrix4x4 worldTransform)
    {
        Source = source;
        OriginalWorldTransform = originalWorldTransform;
        WorldTransform = worldTransform;
    }

    public SmoCollisionMesh Source { get; }
    public SmoLevelEntity Entity { get; internal set; } = null!;
    public Matrix4x4 OriginalWorldTransform { get; }
    public Matrix4x4 WorldTransform { get; internal set; }
    public bool IsModified => !WorldTransform.Equals(OriginalWorldTransform);
}

public sealed class SmoEditablePlacement
{
    internal SmoEditablePlacement(
        SmoPlacementId id,
        SmoLevelAsset asset,
        SmoLevelPlacement source,
        Matrix4x4 originalWorldTransform,
        Matrix4x4 worldTransform)
    {
        Id = id;
        Asset = asset;
        Source = source;
        OriginalWorldTransform = originalWorldTransform;
        WorldTransform = worldTransform;
    }

    public SmoPlacementId Id { get; }
    public SmoLevelAsset Asset { get; }
    public SmoLevelPlacement Source { get; }
    public SmoLevelEntity Entity { get; internal set; } = null!;
    public Matrix4x4 OriginalWorldTransform { get; }
    public Matrix4x4 WorldTransform { get; internal set; }
    public bool IsModified => !WorldTransform.Equals(OriginalWorldTransform);
}
