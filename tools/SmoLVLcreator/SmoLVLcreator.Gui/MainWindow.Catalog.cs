using SmoLVLcreator.Core;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using Microsoft.Win32;
using SmoImporter.Core;

namespace SmoLVLcreator.Gui;

public partial class MainWindow
{
    private readonly List<CatalogItem> _catalogItems = [];
    private readonly Dictionary<string, ImageSource> _catalogPreviewCache = [];
    private readonly Dictionary<int, ImageSource> _textureReplacementPreviews = [];
    private readonly Dictionary<Guid, SmoSceneMesh[]> _externalModelPreviewMeshes = [];
    private int _nextExternalPreviewObjectIndex = -1_000_000_000;
    private CatalogItem[] _pendingCatalogPreviews = [];
    private CatalogSection _catalogSection = CatalogSection.Models;
    private SmoLevelWorkspace? _catalogPreviewWorkspace;
    private int _catalogPreviewGeneration;
    private bool _catalogPreviewPumpRunning;
    private Point _catalogDragStart;
    private CatalogItem? _catalogDragItem;

    private void AssetTab_Checked(object sender, RoutedEventArgs e)
    {
        if (sender is not RadioButton { Tag: string tag } ||
            !Enum.TryParse(tag, out CatalogSection section))
        {
            return;
        }

        _catalogSection = section;
        if (AssetList is not null)
        {
            AssetFilterBox.Clear();
            ApplyCatalogFilter();
            StatusText.Text = LevelText.Get("Catalog.SectionStatus", CatalogSectionTitle(section));
        }
    }

    private void AssetList_PreviewMouseLeftButtonDown(
        object sender,
        MouseButtonEventArgs e)
    {
        _catalogDragStart = e.GetPosition(AssetList);
        ListBoxItem? container = ItemsControl.ContainerFromElement(
            AssetList,
            e.OriginalSource as DependencyObject) as ListBoxItem;
        _catalogDragItem = container?.DataContext as CatalogItem ??
            AssetList.SelectedItem as CatalogItem;
        if (_catalogDragItem is not null &&
            !ReferenceEquals(AssetList.SelectedItem, _catalogDragItem))
            AssetList.SelectedItem = _catalogDragItem;
        if (CatalogPlaceButton is not null)
            CatalogPlaceButton.IsEnabled = !_isBusy &&
                _catalogSection == CatalogSection.Models &&
                _catalogDragItem is not null;
    }

    private void AssetList_PreviewMouseMove(object sender, MouseEventArgs e)
    {
        if (e.LeftButton != MouseButtonState.Pressed ||
            _catalogSection != CatalogSection.Models ||
            _document is null || _isBusy)
            return;
        Point current = e.GetPosition(AssetList);
        if (Math.Abs(current.X - _catalogDragStart.X) <
                SystemParameters.MinimumHorizontalDragDistance &&
            Math.Abs(current.Y - _catalogDragStart.Y) <
                SystemParameters.MinimumVerticalDragDistance)
            return;

        CatalogItem? item = _catalogDragItem ?? AssetList.SelectedItem as CatalogItem;
        if (item is null)
            return;
        if (!TryCreateCatalogDrag(item, out CatalogModelDrag? drag, out string error))
        {
            StatusText.Text = error;
            return;
        }
        var data = new DataObject();
        data.SetData(CatalogModelDrag.Format, drag, autoConvert: false);
        DragDrop.DoDragDrop(AssetList, data, DragDropEffects.Copy);
        _catalogDragItem = null;
    }

    private void ApplyCatalogFilter()
    {
        if (AssetList is null)
            return;

        if (!ReferenceEquals(_catalogPreviewWorkspace, _workspace))
        {
            _catalogPreviewWorkspace = _workspace;
            _catalogPreviewCache.Clear();
            _textureReplacementPreviews.Clear();
            _externalModelPreviewMeshes.Clear();
        }

        RebuildCatalogItems();
        string filter = AssetFilterBox?.Text.Trim() ?? string.Empty;
        CatalogItem[] visible = _catalogItems
            .Where(item => filter.Length == 0 ||
                item.SearchText.Contains(filter, StringComparison.OrdinalIgnoreCase))
            .ToArray();

        _syncingSelection = true;
        AssetList.ItemsSource = visible;
        _syncingSelection = false;
        if (CatalogPlaceButton is not null)
            CatalogPlaceButton.IsEnabled = false;
        _catalogPreviewGeneration++;
        _pendingCatalogPreviews = [];
        Dispatcher.BeginInvoke(
            QueueVisibleCatalogPreviews,
            DispatcherPriority.Background);
        UpdateCatalogLanguageCaptions(visible.Length);
    }

    private void UpdateCatalogLanguageCaptions(int visibleCount)
    {
        AssetCountText.Text = LevelText.Get(_catalogSection switch
        {
            CatalogSection.Models => "Catalog.ModelCount",
            CatalogSection.Placements => "Catalog.PlacementCount",
            CatalogSection.Textures => "Catalog.TextureCount",
            _ => "Catalog.DiagnosticCount"
        }, visibleCount);
        if (AssetFilterBox is not null)
        {
            AssetFilterBox.ToolTip =
                LevelText.Get("Catalog.SearchSection", CatalogSectionTitle(_catalogSection).ToLowerInvariant());
        }
    }

    private void RefreshCatalogLanguage()
    {
        UpdateCatalogLanguageCaptions(AssetList.Items.Count);
        foreach (CatalogItem item in _catalogItems)
            item.RefreshLanguage();

        if (AssetList.SelectedItem is CatalogItem selected)
        {
            if (selected.Texture is not null)
                UpdateTextureInspector(selected);
            else if (_catalogSection == CatalogSection.Unknown)
                UpdateUnknownInspector(selected);
        }
    }

    private void RebuildCatalogItems()
    {
        _catalogItems.Clear();
        IReadOnlyList<SmoSceneMesh> catalogMeshes =
            (_renderPreparedScene ?? _workspace?.PreparedScene)?.Meshes ??
            Array.Empty<SmoSceneMesh>();
        switch (_catalogSection)
        {
            case CatalogSection.Models:
                var groupedAssets = new HashSet<int>();
                if (_document is not null && _workspace is not null)
                {
                    foreach (IGrouping<string, SmoCompositeModel> resource in
                             _document.CompositeModels
                                 .Where(model => model.Entities.All(entity =>
                                     !_document.RemovedEntityIds.Contains(entity.Id)))
                                 .GroupBy(model => string.Join(
                                 ',',
                                 model.Parts.Select(part => part.Asset.ObjectIndex)
                                     .Distinct()
                                     .Order())))
                    {
                        SmoCompositeModel[] models = resource.ToArray();
                        SmoCompositeModel model = models[0];
                        CatalogVisualPart[] parts = model.Parts
                            .Select(TryCreateCatalogVisualPart)
                            .Where(part => part is not null)
                            .Select(part => part!)
                            .ToArray();
                        if (parts.Length != model.Parts.Count)
                            continue;
                        _catalogItems.Add(CatalogItem.FromCompositeModel(
                            model,
                            parts,
                            models));
                        foreach (CatalogVisualPart part in parts)
                            groupedAssets.Add(part.AssetItem.Asset!.ObjectIndex);
                    }
                }
                foreach (EditorAssetItem item in _allAssets.Where(item =>
                             (item.Asset is null ||
                              item.Asset.Placements.Count == 0 ||
                              item.Asset.Placements.Any(placement =>
                                  _document is null ||
                                  !_document.RemovedEntityIds.Contains(
                                      new SmoLevelEntityId(
                                          placement.SceneObjectIndex)))) &&
                             (item.Asset is null ||
                              !groupedAssets.Contains(item.Asset.ObjectIndex))))
                {
                    SmoSceneMesh? previewMesh = catalogMeshes
                        .FirstOrDefault(mesh =>
                            mesh.Mesh.ObjectIndex == item.Asset?.ObjectIndex);
                    _catalogItems.Add(CatalogItem.FromModel(item, previewMesh));
                }
                if (_document is not null)
                {
                    foreach (SmoLevelExternalModel model in _document.ExternalModels.Values
                                 .OrderBy(model => model.Name, StringComparer.OrdinalIgnoreCase))
                    {
                        _catalogItems.Add(CatalogItem.FromExternalModel(
                            model,
                            GetExternalPreviewMeshes(model),
                            _document.ExternalPlacements.Values.Count(placement =>
                                placement.ModelId == model.Id)));
                    }
                }
                break;
            case CatalogSection.Placements:
                foreach (EditorAssetItem item in _allAssets)
                {
                    if (item.Asset is not SmoLevelAsset asset)
                        continue;
                    for (int index = 0; index < asset.Placements.Count; index++)
                    {
                        SmoLevelPlacement placement = asset.Placements[index];
                        Matrix4x4 transform = placement.WorldTransform;
                        if (_document?.TryGetPlacement(
                                new SmoPlacementId(asset.ObjectIndex, placement.SceneObjectIndex, placement.OccurrenceKey),
                                out SmoEditablePlacement? editable) == true)
                        {
                            transform = editable!.WorldTransform;
                        }
                        _catalogItems.Add(CatalogItem.FromPlacement(
                            item,
                            index,
                            transform,
                            catalogMeshes.FirstOrDefault(mesh =>
                                mesh.Mesh.ObjectIndex == asset.ObjectIndex &&
                                mesh.SceneObjectIndex == placement.SceneObjectIndex && mesh.OccurrenceKey == placement.OccurrenceKey)));
                    }
                }
                break;
            case CatalogSection.Textures:
                if (_workspace is not null)
                {
                    foreach (SmoLevelTexture texture in _workspace.Textures
                                 .OrderBy(texture => texture.Name, StringComparer.OrdinalIgnoreCase))
                    {
                        EditorAssetItem[] users = _allAssets.Where(item =>
                                item.Asset?.Texture?.ObjectIndex == texture.ObjectIndex)
                            .ToArray();
                        _catalogItems.Add(CatalogItem.FromTexture(texture, users));
                    }
                }
                break;
            case CatalogSection.Unknown:
                BuildUnknownCatalog();
                break;
        }
        if (_catalogSection == CatalogSection.Models)
            ApplyPendingModelCatalogState();
        ApplyCatalogVisibilityState();
    }

    private void ApplyCatalogVisibilityState()
    {
        if (_catalogSection != CatalogSection.Placements || _document is null)
            return;
        foreach (CatalogItem item in _catalogItems)
        {
            CatalogVisualPart[] parts = item.VisualParts;
            item.IsHidden = parts.Length > 0 && parts.All(part =>
            {
                if (part.AssetItem.Asset is not SmoLevelAsset asset ||
                    (uint)part.PlacementIndex >= (uint)asset.Placements.Count)
                {
                    return false;
                }
                SmoLevelPlacement placement = asset.Placements[part.PlacementIndex];
                return _document.TryGetPlacement(
                           new SmoPlacementId(
                               asset.ObjectIndex,
                               placement.SceneObjectIndex, placement.OccurrenceKey),
                           out SmoEditablePlacement? editable) &&
                       _hiddenEntities.Contains(editable!.Entity.Id);
            });
        }
    }

    private CatalogVisualPart? TryCreateCatalogVisualPart(
        SmoEditablePlacement placement)
    {
        EditorAssetItem? item = _allAssets.FirstOrDefault(candidate =>
            candidate.Asset?.ObjectIndex == placement.Asset.ObjectIndex);
        if (item?.Asset is not SmoLevelAsset asset)
            return null;
        int placementIndex = FindPlacementIndex(
            asset,
            placement.Source.SceneObjectIndex, placement.Source.OccurrenceKey);
        IReadOnlyList<SmoSceneMesh> catalogMeshes =
            (_renderPreparedScene ?? _workspace?.PreparedScene)?.Meshes ??
            Array.Empty<SmoSceneMesh>();
        SmoSceneMesh? previewMesh = catalogMeshes.FirstOrDefault(mesh =>
            mesh.Mesh.ObjectIndex == asset.ObjectIndex &&
            mesh.SceneObjectIndex == placement.Source.SceneObjectIndex && mesh.OccurrenceKey == placement.Source.OccurrenceKey);
        return new CatalogVisualPart(item, placementIndex, previewMesh);
    }

    private void BuildUnknownCatalog()
    {
        if (_workspace is null)
        {
            _catalogItems.AddRange(_allAssets
                .Where(item => item.HasIssue)
                .Select(CatalogItem.FromModelIssue));
            return;
        }

        foreach (SmoObjectEntry entry in _workspace.Document.Objects.Where(entry =>
                     entry.ClassName is null))
        {
            _catalogItems.Add(CatalogItem.FromUnknownObject(entry));
        }
        foreach (SmoDiagnostic diagnostic in _workspace.Document.Diagnostics)
            _catalogItems.Add(CatalogItem.FromDiagnostic(diagnostic));
        foreach (string issue in _workspace.DecodeIssues)
            _catalogItems.Add(CatalogItem.FromIssue(LevelText.Label("Catalog.DecodeIssue"), issue));
        foreach (string issue in _workspace.PreparedScene.TextureIssues)
            _catalogItems.Add(CatalogItem.FromIssue(LevelText.Label("Catalog.TextureIssue"), issue));
    }

    private void HandleCatalogSelectionChanged()
    {
        if (_syncingSelection)
            return;

        CatalogItem[] selection = AssetList.SelectedItems
            .OfType<CatalogItem>()
            .ToArray();
        if (CatalogPlaceButton is not null)
        {
            CatalogPlaceButton.IsEnabled = !_isBusy && selection.Length == 1 &&
                _catalogSection == CatalogSection.Models;
        }
        if (selection.Length == 0)
            return;

        if (_catalogSection is CatalogSection.Models or CatalogSection.Placements)
        {
            StatusText.Text = selection.Length == 1
                ? LevelText.Get("Catalog.OneItemSelected", selection[0].Name)
                : LevelText.Get("Catalog.ItemsSelected", selection.Length);
            return;
        }

        ClearSceneSelectionForResource();
        CatalogItem active = AssetList.SelectedItem as CatalogItem ?? selection[^1];
        if (active.Texture is not null)
            UpdateTextureInspector(active);
        else
            UpdateUnknownInspector(active);
    }

    private void AssetList_Loaded(object sender, RoutedEventArgs e) =>
        QueueVisibleCatalogPreviews();

    private void AssetList_ScrollChanged(object sender, ScrollChangedEventArgs e)
    {
        if (Math.Abs(e.HorizontalChange) > 0.01 ||
            Math.Abs(e.ViewportWidthChange) > 0.01 ||
            Math.Abs(e.ExtentWidthChange) > 0.01)
        {
            QueueVisibleCatalogPreviews();
        }
    }

    private void QueueVisibleCatalogPreviews()
    {
        if (AssetList.Items.Count == 0)
        {
            _catalogPreviewGeneration++;
            _pendingCatalogPreviews = [];
            return;
        }

        ScrollViewer? scrollViewer = FindVisualDescendant<ScrollViewer>(AssetList);
        const double cardWidth = 184;
        double offset = scrollViewer?.HorizontalOffset ?? 0;
        double viewportWidth = scrollViewer?.ViewportWidth ?? AssetList.ActualWidth;
        int visibleFirst = Math.Max(0, (int)(offset / cardWidth));
        int visibleCount = Math.Max(8, (int)Math.Ceiling(viewportWidth / cardWidth) + 1);
        int visibleLast = Math.Min(AssetList.Items.Count, visibleFirst + visibleCount);
        var prioritized = new List<CatalogItem>(visibleCount + 4);

        void AddCandidate(int index)
        {
            if (AssetList.Items[index] is not CatalogItem { Preview: null } item)
                return;
            if (_catalogPreviewCache.TryGetValue(
                    item.PreviewCacheKey,
                    out ImageSource? cached))
            {
                item.Preview = cached;
                return;
            }
            prioritized.Add(item);
        }

        for (int index = visibleFirst; index < visibleLast; index++)
            AddCandidate(index);
        for (int distance = 1; distance <= 2; distance++)
        {
            int before = visibleFirst - distance;
            int after = visibleLast - 1 + distance;
            if (before >= 0)
                AddCandidate(before);
            if (after < AssetList.Items.Count)
                AddCandidate(after);
        }

        _catalogPreviewGeneration++;
        _pendingCatalogPreviews = prioritized.ToArray();
        if (!_catalogPreviewPumpRunning)
            _ = ProcessCatalogPreviewQueueAsync();
    }

    private static T? FindVisualDescendant<T>(DependencyObject root)
        where T : DependencyObject
    {
        int count = VisualTreeHelper.GetChildrenCount(root);
        for (int index = 0; index < count; index++)
        {
            DependencyObject child = VisualTreeHelper.GetChild(root, index);
            if (child is T match)
                return match;
            T? descendant = FindVisualDescendant<T>(child);
            if (descendant is not null)
                return descendant;
        }
        return null;
    }

    private async Task ProcessCatalogPreviewQueueAsync()
    {
        _catalogPreviewPumpRunning = true;
        try
        {
            while (true)
            {
                int generation = _catalogPreviewGeneration;
                CatalogItem[] batch = _pendingCatalogPreviews;
                _pendingCatalogPreviews = [];
                foreach (CatalogItem item in batch)
                {
                    await Dispatcher.Yield(DispatcherPriority.Background);
                    if (generation != _catalogPreviewGeneration)
                        break;
                    GenerateCatalogPreview(item);
                }

                if (generation == _catalogPreviewGeneration &&
                    _pendingCatalogPreviews.Length == 0)
                {
                    return;
                }
            }
        }
        finally
        {
            _catalogPreviewPumpRunning = false;
            if (_pendingCatalogPreviews.Length > 0)
                _ = ProcessCatalogPreviewQueueAsync();
        }
    }

    private void GenerateCatalogPreview(CatalogItem item)
    {
        if (item.Preview is not null || !item.TryBeginPreviewLoad())
            return;

        try
        {
            if (_catalogPreviewCache.TryGetValue(
                    item.PreviewCacheKey,
                    out ImageSource? cached))
            {
                item.Preview = cached;
                return;
            }

            ImageSource? preview = item.Texture is SmoLevelTexture texture &&
                                   _document?.TextureReplacements.ContainsKey(
                                       texture.ObjectIndex) == true &&
                                   _textureReplacementPreviews.TryGetValue(
                                       texture.ObjectIndex,
                                       out ImageSource? replacementPreview)
                ? replacementPreview
                : item.Texture is SmoLevelTexture decodedTexture &&
                  decodedTexture.Bgra32Pixels.Length > 0
                    ? SmoThumbnailRenderer.CreateTextureThumbnail(
                        decodedTexture.Width,
                        decodedTexture.Height,
                        decodedTexture.Bgra32Pixels)
                : item.PreviewMeshes.Length > 0
                    ? SmoThumbnailRenderer.CreateModelThumbnail(item.PreviewMeshes)
                    : null;
            if (preview is not null)
            {
                _catalogPreviewCache[item.PreviewCacheKey] = preview;
                item.Preview = preview;
            }
        }
        catch (Exception exception)
        {
            item.PreviewError = LevelText.Error(exception.Message);
        }
        finally
        {
            item.CompletePreviewLoad();
        }
    }

    private void ApplyCatalogVisualSelection(IReadOnlyList<CatalogItem> selection)
    {
        if (_document is null)
        {
            if (selection[^1].AssetItem is EditorAssetItem mock)
                UpdateInspector(mock, Math.Max(selection[^1].PlacementIndex, 0));
            return;
        }

        var resolved = new List<(
            CatalogItem Item,
            EditorAssetItem AssetItem,
            int PlacementIndex,
            SmoEditablePlacement Placement)>();
        foreach (CatalogItem catalogItem in selection)
        {
            CatalogVisualPart[] visualParts = catalogItem.VisualParts.Length > 0
                ? catalogItem.VisualParts
                : catalogItem.AssetItem is EditorAssetItem fallback
                    ? [new CatalogVisualPart(
                        fallback,
                        Math.Max(catalogItem.PlacementIndex, 0),
                        catalogItem.PreviewMesh)]
                    : [];
            foreach (CatalogVisualPart visualPart in visualParts)
            {
                if (visualPart.AssetItem.Asset is not SmoLevelAsset asset ||
                    asset.Placements.Count == 0)
                {
                    continue;
                }
                int index = Math.Clamp(
                    visualPart.PlacementIndex,
                    0,
                    asset.Placements.Count - 1);
                SmoLevelPlacement source = asset.Placements[index];
                if (_document.TryGetPlacement(
                        new SmoPlacementId(asset.ObjectIndex, source.SceneObjectIndex, source.OccurrenceKey),
                        out SmoEditablePlacement? placement))
                {
                    resolved.Add((
                        catalogItem,
                        visualPart.AssetItem,
                        index,
                        placement!));
                }
            }
        }
        if (resolved.Count == 0)
            return;

        _selectedEntities.Clear();
        _selectedEntities.UnionWith(_document.ExpandCompositeEntities(
            resolved.Select(pair => pair.Placement.Entity.Id)));

        CatalogItem active = AssetList.SelectedItem as CatalogItem ?? resolved[^1].Item;
        (CatalogItem Item, EditorAssetItem AssetItem, int PlacementIndex,
            SmoEditablePlacement Placement) activeResolved =
            resolved.FirstOrDefault(pair => ReferenceEquals(pair.Item, active));
        if (activeResolved.Item is null)
            activeResolved = resolved[^1];

        EditorAssetItem activeAssetItem = activeResolved.AssetItem;
        SmoLevelAsset activeAsset = activeAssetItem.Asset!;
        int activeIndex = activeResolved.PlacementIndex;
        SmoLevelPlacement sourcePlacement = activeAsset.Placements[activeIndex];
        _activePlacementIndex = activeIndex;
        _activeSelectionKey = new PlacementSelectionKey(
            activeAsset.ObjectIndex,
            sourcePlacement.SceneObjectIndex, sourcePlacement.OccurrenceKey);
        _activeCollisionEntityIndex = null;
        ApplyPlacementHighlights();
        UpdateInspector(activeAssetItem, activeIndex, updateViewportSelection: false);
        UpdateSelectionCaption(active.Name, activeIndex);
        if (active.VisualParts.Length > 1)
        {
            FrameMeshes(active.VisualParts
                .Select(part => part.PreviewMesh)
                .Where(mesh => mesh is not null)
                .Select(mesh => mesh!));
        }
        else if (_selectedEntities.Count == 1)
        {
            FocusPlacement(activeAsset, activeIndex);
        }
        RevealCatalogSelectionInTree(selectNode: true);
        StatusText.Text = _selectedEntities.Count == 1
            ? LevelText.Get("Catalog.PlacementSelected", active.Name, activeIndex + 1)
            : LevelText.Get("Catalog.ObjectsSelected", _selectedEntities.Count);
    }

    private void ClearSceneSelectionForResource()
    {
        _selectedEntities.Clear();
        _selectedPendingPlacement = null;
        _activeSelectionKey = null;
        _activeCollisionEntityIndex = null;
        ApplyPlacementHighlights();
    }

    private void UpdateTextureInspector(CatalogItem item)
    {
        SmoLevelTexture texture = item.Texture!;
        SmoObjectEntry? entry = _workspace?.Document.Objects.ElementAtOrDefault(
            texture.ObjectIndex);
        SelectedNameText.Text = texture.Name.TrimEnd('\0');
        SelectedPathText.Text = LevelText.Get("Catalog.TextureCatalogPath", texture.ObjectIndex);
        ObjectIdentityText.Text = $"spTextureData · [{texture.ObjectIndex}]";
        ObjectIdText.Text = entry is null ? "—" : $"0x{entry.Id:X8}";
        SetPositionEditor(null);
        SetRotationScaleEditor(null, null);
        PlacementCountText.Text = LevelText.Get("Catalog.TextureModelCount", item.RelatedAssets.Length);
        GeometryCountText.Text = $"{texture.Width:N0} × {texture.Height:N0}";
        VertexLayoutText.Text = LevelText.Get("Catalog.TextureFormat", texture.FormatCode);
        ChannelsText.Text = texture.SourceLayout;
        TextureNameText.Text = texture.Name.TrimEnd('\0');
        MaterialText.Text = item.RelatedAssets.Length == 0
            ? LevelText.Get("Catalog.NoVerifiedBinding")
            : string.Join(" · ", item.RelatedAssets.Take(3).Select(asset => asset.Name));
        BindingsText.Text = item.RelatedAssets.Length == 0
            ? LevelText.Get("Catalog.StandaloneTexture")
            : LevelText.Get("Catalog.TextureBindings", item.RelatedAssets.Length);
        DiagnosticsText.Text = LevelText.Get("Catalog.SharedTextureDecoded");
        DiagnosticsText.Foreground = HealthyBrush;
        ViewportSelectionText.Text = LevelText.Get("Catalog.TextureViewportResource", texture.Name.TrimEnd('\0'));
        StatusText.Text = LevelText.Get("Catalog.TextureStatus", texture.Width, texture.Height, texture.FormatCode);
        UpdateRawData((LevelText.Get("Catalog.TextureResource"), texture.ObjectIndex));
    }

    private void UpdateUnknownInspector(CatalogItem item)
    {
        SelectedNameText.Text = item.Name;
        SelectedPathText.Text = item.Path;
        ObjectIdentityText.Text = item.Identity;
        ObjectIdText.Text = item.ObjectIdText;
        SetPositionEditor(null);
        SetRotationScaleEditor(null, null);
        PlacementCountText.Text = "—";
        GeometryCountText.Text = item.Details;
        VertexLayoutText.Text = item.Kind;
        ChannelsText.Text = "—";
        TextureNameText.Text = "—";
        MaterialText.Text = "—";
        BindingsText.Text = item.Path;
        DiagnosticsText.Text = item.Message;
        DiagnosticsText.Foreground = item.StatusColor;
        ViewportSelectionText.Text = LevelText.Get("Catalog.ViewportResource", item.Kind, item.Name);
        StatusText.Text = $"{item.Kind} · {item.Name}";
        UpdateRawData((LevelText.Get("Catalog.CatalogObject"), item.RawObjectIndex));
    }

    private void FocusCatalogSelection()
    {
        if (AssetList.SelectedItem is not CatalogItem item ||
            item.AssetItem?.Asset is not SmoLevelAsset asset)
        {
            StatusText.Text = LevelText.Get("Catalog.ResourceHasNoPlacement");
            return;
        }
        if (item.VisualParts.Length > 1)
        {
            FrameMeshes(item.VisualParts
                .Select(part => part.PreviewMesh)
                .Where(mesh => mesh is not null)
                .Select(mesh => mesh!));
            StatusText.Text = LevelText.Get("Catalog.FocusParts", item.Name, item.VisualParts.Length);
            return;
        }
        int index = Math.Clamp(item.PlacementIndex, 0, asset.Placements.Count - 1);
        FocusPlacement(asset, index);
        StatusText.Text = LevelText.Get("Catalog.FocusPlacement", item.AssetItem.Name, index + 1);
    }

    private void RevealCatalogSelectionInTree(bool selectNode)
    {
        if (AssetList.SelectedItem is not CatalogItem {
                AssetItem: EditorAssetItem assetItem } catalogItem)
        {
            return;
        }
        int placementIndex = Math.Max(catalogItem.PlacementIndex, 0);
        TreeViewItem? treeItem = FindSceneTreeItem(
            SceneTree.Items.OfType<TreeViewItem>(),
            assetItem,
            placementIndex);
        if (treeItem is null && !string.IsNullOrEmpty(OutlinerFilterBox.Text))
        {
            OutlinerFilterBox.Clear();
            treeItem = FindSceneTreeItem(
                SceneTree.Items.OfType<TreeViewItem>(),
                assetItem,
                placementIndex);
        }
        if (treeItem is null)
            return;
        if (selectNode)
        {
            _syncingSelection = true;
            treeItem.IsSelected = true;
            _syncingSelection = false;
        }
        treeItem.BringIntoView();
    }

    private void SelectCatalogItemForPlacement(
        EditorAssetItem assetItem,
        int placementIndex)
    {
        if (_catalogSection is CatalogSection.Textures or CatalogSection.Unknown)
            return;

        CatalogItem? catalogItem = _catalogSection == CatalogSection.Models
            ? AssetList.Items.OfType<CatalogItem>().FirstOrDefault(item =>
                item.ContainsVisualPart(assetItem, placementIndex, matchPlacement: false))
            : AssetList.Items.OfType<CatalogItem>().FirstOrDefault(item =>
                item.ContainsVisualPart(assetItem, placementIndex, matchPlacement: true));
        if (catalogItem is null && !string.IsNullOrWhiteSpace(AssetFilterBox.Text))
        {
            AssetFilterBox.Clear();
            catalogItem = _catalogSection == CatalogSection.Models
                ? AssetList.Items.OfType<CatalogItem>().FirstOrDefault(item =>
                    item.ContainsVisualPart(assetItem, placementIndex, matchPlacement: false))
                : AssetList.Items.OfType<CatalogItem>().FirstOrDefault(item =>
                    item.ContainsVisualPart(assetItem, placementIndex, matchPlacement: true));
        }
        if (catalogItem is null)
            return;

        _syncingSelection = true;
        AssetList.SelectedItems.Clear();
        AssetList.SelectedItem = catalogItem;
        AssetList.ScrollIntoView(catalogItem);
        _syncingSelection = false;
    }

    private void CatalogFocus_Click(object sender, RoutedEventArgs e) =>
        FocusCatalogSelection();

    private void CatalogRevealInTree_Click(object sender, RoutedEventArgs e) =>
        RevealCatalogSelectionInTree(selectNode: true);

    private async void CatalogImportModel_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null)
            return;
        var dialog = new OpenFileDialog
        {
            Title = LevelText.Get("Catalog.ImportTitle"),
            Filter = LevelText.Get("Catalog.ModelFilter"),
            CheckFileExists = true,
            Multiselect = false
        };
        if (dialog.ShowDialog(this) != true)
            return;
        try
        {
            ImportedScene? imported = await ReadCatalogDonorAsync(dialog.FileName);
            if (imported is null)
                return;
            if (!ConfirmImportValidation(SmoLevelImportValidator.Validate(
                    imported,
                    SmoLevelImportPurpose.AddExternalModel)))
            {
                return;
            }
            if (HasProject)
            {
                await AddProjectExternalModelAsync(
                    imported,
                    dialog.FileName,
                    Path.GetFileNameWithoutExtension(dialog.FileName));
                return;
            }
            SetBusy(true, LevelText.Get("Catalog.AddingModel"));
            Guid modelId = _document.AddExternalModel(
                imported,
                dialog.FileName,
                Path.GetFileNameWithoutExtension(dialog.FileName));
            ModelsCatalogTab.IsChecked = true;
            ApplyCatalogFilter();
            CatalogItem? added = _catalogItems.FirstOrDefault(item =>
                item.ExternalModelId == modelId);
            if (added is not null)
            {
                AssetList.SelectedItem = added;
                AssetList.ScrollIntoView(added);
            }
            SetBusy(false,
                LevelText.Get("Catalog.ModelAdded", Path.GetFileName(dialog.FileName)));
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Catalog.ModelAddFailed"));
            MessageBox.Show(this, LevelText.Error(exception.Message),
                LevelText.Get("Catalog.ModelAddError"),
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private async Task<ImportedScene?> ReadCatalogDonorAsync(string path)
    {
        SetBusy(true, LevelText.Get("Catalog.ReadingModel", Path.GetFileName(path)));
        ImportedScene source = await Task.Run(() => ImportedModelReader.Read(path));
        SetBusy(false, LevelText.Get("Language.Ready"));
        var options = new ModelImportWindow(path, source) { Owner = this };
        if (options.ShowDialog() != true)
            return null;

        ImportedScene effective = options.EffectiveScene;
        SetBusy(true, LevelText.Get("Dialog.Import.Preparing"));
        ImportedScene prepared = await Task.Run(() => SmoLevelEmbeddedTextureBudget.Prepare(effective));
        SetBusy(false, LevelText.Get("Catalog.ModelChecked"));
        return prepared;
    }

    private async void CatalogPlace_Click(object sender, RoutedEventArgs e)
    {
        CatalogItem? item = AssetList.SelectedItem as CatalogItem ?? _catalogDragItem;
        if (item is null)
        {
            StatusText.Text = LevelText.Get("Catalog.SelectModelFirst");
            return;
        }
        if (HasProject)
        {
            if (!TryCreateCatalogDrag(item, out CatalogModelDrag? projectDrag, out string error))
            {
                StatusText.Text = error;
                return;
            }
            await PlaceProjectCatalogModelAsync(
                projectDrag!,
                ResolveDropPosition(new Point(
                    Math.Max(ViewportSurface.ActualWidth, 1) * 0.5,
                    Math.Max(ViewportSurface.ActualHeight, 1) * 0.5)));
            return;
        }
        StatusText.Text = LevelText.Get("Catalog.PlacingItem", item.Name);
        PlaceCatalogItem(item, ResolveDropPosition(new Point(
            Math.Max(ViewportSurface.ActualWidth, 1) * 0.5,
            Math.Max(ViewportSurface.ActualHeight, 1) * 0.5)));
    }

    private bool TryCreateCatalogDrag(
        CatalogItem item,
        out CatalogModelDrag? drag,
        out string error)
    {
        drag = null;
        error = string.Empty;
        Vector3 sourceAnchor = CalculateCatalogSourceAnchor(item);
        if (item.ExternalModelId is Guid externalId)
        {
            SmoLevelExternalModel externalModel =
                _document!.ExternalModels[externalId];
            drag = new CatalogModelDrag(
                externalId,
                [],
                item.Name,
                sourceAnchor,
                externalModel.SuggestedPlacementScale);
            return true;
        }
        CatalogPlacementPart[] parts = item.VisualParts
            .Where(part => part.AssetItem.Asset is not null)
            .Select(part =>
            {
                SmoLevelAsset asset = part.AssetItem.Asset!;
                int sceneIndex = part.PreviewMesh?.SceneObjectIndex ??
                    asset.Placements[part.PlacementIndex].SceneObjectIndex;
                return new CatalogPlacementPart(
                    asset.ObjectIndex,
                    _document!.GetPlacement(new SmoPlacementId(
                        asset.ObjectIndex,
                        sceneIndex, part.PreviewMesh?.OccurrenceKey ?? asset.Placements[part.PlacementIndex].OccurrenceKey)).WorldTransform,
                    asset.DisplayName);
            })
            .GroupBy(part => (part.MeshObjectIndex, part.SourceWorldTransform))
            .Select(group => group.First())
            .ToArray();
        if (parts.Length == 0)
        {
            error = LevelText.Get("Catalog.NoPlaceableGeometry", item.Name);
            return false;
        }
        foreach (CatalogPlacementPart part in parts)
        {
            if (!HasProject && !SmoSharedPlacementCloner.CanClone(
                    _workspace!.Document,
                    part.MeshObjectIndex))
            {
                error = LevelText.Get("Catalog.NoReferenceTemplate", part.Name);
                return false;
            }
        }
        drag = new CatalogModelDrag(null, parts, item.Name, sourceAnchor, 1f);
        return true;
    }

    private static Vector3 CalculateCatalogSourceAnchor(CatalogItem item)
    {
        bool hasGeometry = false;
        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);
        foreach (SmoSceneMesh sceneMesh in item.PreviewMeshes)
        {
            foreach (Vector3 position in sceneMesh.Mesh.Positions)
            {
                Vector3 world = Vector3.Transform(
                    position,
                    sceneMesh.WorldTransform);
                minimum = Vector3.Min(minimum, world);
                maximum = Vector3.Max(maximum, world);
                hasGeometry = true;
            }
        }

        return hasGeometry
            ? (minimum + maximum) * 0.5f
            : Vector3.Zero;
    }

    private SmoSceneMesh[] GetExternalPreviewMeshes(SmoLevelExternalModel model)
    {
        if (_externalModelPreviewMeshes.TryGetValue(model.Id, out SmoSceneMesh[]? cached))
            return cached;
        if (_workspace is null)
            return [];
        SmoSceneMesh template = _workspace.PreparedScene.Meshes.First(mesh =>
            mesh.Mesh.ObjectIndex == model.TemplateMeshObjectIndex);
        var result = new List<SmoSceneMesh>(model.ImportedScene.Meshes.Count);
        var previewTextures = new Dictionary<int, SmoTexture>();
        for (int index = 0; index < model.ImportedScene.Meshes.Count; index++)
        {
            int meshObjectIndex = AllocateExternalPreviewObjectIndex();
            ImportedMesh importedMesh = model.ImportedScene.Meshes[index];
            var onePart = new ImportedScene(
                [importedMesh],
                model.ImportedScene.Textures,
                model.ImportedScene.Materials);
            SmoMesh mesh = SmoMeshResourceReplacer.CreatePreviewMesh(
                _workspace.Document,
                model.TemplateMeshObjectIndex,
                onePart,
                ReplacementTransform.Identity,
                referenceWorldTransform: Matrix4x4.Identity,
                transientObjectIndex: meshObjectIndex);
            SmoTexture? texture = ResolveImportedPreviewTexture(
                model.ImportedScene,
                importedMesh,
                AllocateExternalPreviewObjectIndex(),
                previewTextures);
            SmoMaterialRenderStateInfo? materialState =
                SmoLevelModelGraphReplacer.CreatePreviewMaterialState(
                    model.ImportedScene,
                    importedMesh,
                    mesh,
                    texture);
            result.Add(template with
            {
                Mesh = mesh,
                Texture = texture,
                UsesAlphaBlend = materialState?.UsesAlphaBlend ?? false,
                MaterialRenderState = materialState,
                WorldTransform = Matrix4x4.Identity,
                SceneObjectIndex = int.MinValue + index,
                SharedInstance = null,
                AnimationFrames = null,
                AnimationFrameDuration = null,
                BaseTexture = null
            });
        }
        cached = result.ToArray();
        _externalModelPreviewMeshes[model.Id] = cached;
        return cached;
    }

    private int AllocateExternalPreviewObjectIndex()
    {
        if (_nextExternalPreviewObjectIndex == int.MinValue)
            throw new InvalidOperationException(
                LevelText.Get("Catalog.PreviewIdsExhausted"));
        return _nextExternalPreviewObjectIndex--;
    }

    private async void CatalogReplace_Click(object sender, RoutedEventArgs e)
    {
        if (AssetList.SelectedItem is not CatalogItem item)
            return;
        if (item.Texture is not null)
            await CatalogReplaceTextureAsync(item);
        else
            await ReplaceCatalogModelAsync(item);
    }

    private async Task CatalogReplaceTextureAsync(CatalogItem item)
    {
        if (_document is null || _workspace is null)
            return;
        if (item.Texture is not SmoLevelTexture texture)
        {
            MessageBox.Show(
                this,
                LevelText.Get("Catalog.SelectReplaceableItem"),
                LevelText.Get("Catalog.ReplaceModelDialog"),
                MessageBoxButton.OK,
                MessageBoxImage.Information);
            return;
        }

        var dialog = new OpenFileDialog
        {
            Title = LevelText.Get("Catalog.ReplaceTextureTitle", texture.Name.TrimEnd('\0')),
            Filter = LevelText.Get("Catalog.ImageFilter"),
            CheckFileExists = true,
            Multiselect = false
        };
        if (dialog.ShowDialog(this) != true)
            return;

        try
        {
            byte[] encoded = File.ReadAllBytes(dialog.FileName);
            MessageBoxResult alphaChoice = MessageBox.Show(
                this,
                LevelText.Get("Catalog.ChooseAlphaMode"),
                LevelText.Get("Catalog.AlphaModeTitle"),
                MessageBoxButton.YesNoCancel,
                MessageBoxImage.Question);
            if (alphaChoice == MessageBoxResult.Cancel)
                return;
            bool replaceAlpha = alphaChoice == MessageBoxResult.Yes;
            ImageSource preview = DecodeReplacementPreview(encoded);
            if (HasProject)
            {
                uint textureObjectId = _workspace.Document.Objects[texture.ObjectIndex].Id;
                SetBusy(true, LevelText.Get("Catalog.ReplacingProjectTexture", texture.Name.TrimEnd('\0')));
                bool changed = await Task.Run(() =>
                    _projectSession!.Execute(
                        LevelText.Label("Catalog.ReplaceTextureAction", texture.Name.TrimEnd('\0')),
                        project => SmoProjectTextureReplacement.Replace(
                            project,
                            textureObjectId,
                            encoded,
                            replaceAlpha)));
                if (!changed)
                {
                    SetBusy(false, LevelText.Get("Catalog.TextureUnchanged"));
                    return;
                }
                _textureReplacementPreviews[texture.ObjectIndex] = preview;
                _catalogPreviewCache.Remove($"T:{texture.ObjectIndex}");
                await RefreshProjectPreviewAsync(
                    LevelText.Get("Catalog.TextureReplaced", texture.Name.TrimEnd('\0')) +
                    (replaceAlpha ? LevelText.Get("Catalog.RgbAndAlpha") : LevelText.Get("Catalog.RgbOriginalAlpha")));
                return;
            }
            if (!_document.ReplaceTexture(
                    texture.ObjectIndex,
                    encoded,
                    dialog.FileName,
                    replaceAlpha))
            {
                return;
            }
            _textureReplacementPreviews[texture.ObjectIndex] = preview;
            _catalogPreviewCache.Remove($"T:{texture.ObjectIndex}");
            item.Preview = preview;
            item.Status = replaceAlpha
                ? LevelText.Label("Catalog.ReplacedRgbAndAlpha")
                : LevelText.Label("Catalog.ReplacedRgbOriginalAlpha");
            StatusText.Text =
                LevelText.Get("Catalog.TextureReplacedResize", texture.Name.TrimEnd('\0'));
        }
        catch (Exception exception)
        {
            if (_isBusy)
                SetBusy(false, LevelText.Get("Catalog.TextureReplaceFailed"));
            MessageBox.Show(
                this,
                LevelText.Error(exception.Message),
                LevelText.Get("Catalog.TextureReplaceError"),
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
    }

    private async Task ReplaceCatalogModelAsync(CatalogItem item)
    {
        if (_document is null || _workspace is null)
            return;

        CatalogVisualPart[] parts = item.VisualParts
            .Where(part => part.AssetItem.Asset is not null &&
                part.PreviewMesh is not null)
            .ToArray();
        if (parts.Length == 0)
        {
            MessageBox.Show(
                this,
                LevelText.Get("Catalog.ItemHasNoGeometry"),
                LevelText.Get("Catalog.ReplaceModelDialog"),
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
            return;
        }

        var dialog = new OpenFileDialog
        {
            Title = LevelText.Get("Catalog.ReplaceModelTitle", item.Name),
            Filter = LevelText.Get("Catalog.ModelFilter"),
            CheckFileExists = true,
            Multiselect = false
        };
        if (dialog.ShowDialog(this) != true)
            return;

        try
        {
            ImportedScene? imported = await ReadCatalogDonorAsync(dialog.FileName);
            if (imported is null)
                return;

            if (item.CompositeModels.Length > 0)
            {
                if (!ConfirmImportValidation(SmoLevelImportValidator.Validate(
                        imported,
                        SmoLevelImportPurpose.ReplaceCompositeModel)))
                {
                    return;
                }
                await ReplaceCompositeCatalogModelAsync(
                    item,
                    imported,
                    dialog.FileName);
                return;
            }

            if (HasProject)
            {
                if (!ConfirmImportValidation(SmoLevelImportValidator.Validate(
                        imported,
                        SmoLevelImportPurpose.ReplaceCompositeModel)))
                {
                    return;
                }
                SmoSceneMesh[] sourceMeshes = parts
                    .Select(part => part.PreviewMesh!)
                    .DistinctBy(mesh => (mesh.Mesh.ObjectIndex, mesh.SceneObjectIndex))
                    .ToArray();
                Matrix4x4 projectReferenceWorld = sourceMeshes[0].WorldTransform;
                SetBusy(false, LevelText.Get("Catalog.FullReplacementFitOpened"));
                var projectFitWindow = new ModelFitWindow(
                    _workspace.Document,
                    sourceMeshes,
                    imported,
                    projectReferenceWorld,
                    dialog.FileName,
                    item.Name,
                    parts[0].AssetItem.Asset!.Placements.Count)
                {
                    Owner = this
                };
                if (projectFitWindow.ShowDialog() != true)
                    return;
                await ReplaceProjectCatalogModelAsync(
                    item,
                    imported,
                    projectFitWindow.Transform,
                    projectReferenceWorld,
                    dialog.FileName);
                return;
            }

            CatalogVisualPart[] orderedCandidates = parts
                .OrderByDescending(part =>
                    _activeSelectionKey is PlacementSelectionKey active &&
                    part.AssetItem.Asset!.ObjectIndex == active.MeshObjectIndex &&
                    part.PreviewMesh!.SceneObjectIndex == active.SceneObjectIndex && part.PreviewMesh.OccurrenceKey == active.OccurrenceKey)
                .ToArray();
            CatalogVisualPart? targetPart = null;
            SmoLevelModelGraphPlan? modelPlan = null;
            var componentCounts = new HashSet<int>();
            foreach (CatalogVisualPart candidate in orderedCandidates)
            {
                SmoLevelModelGraphPlan candidatePlan =
                    SmoLevelModelGraphReplacer.ResolvePlan(
                        _workspace.Document,
                        candidate.AssetItem.Asset!.ObjectIndex);
                componentCounts.Add(candidatePlan.Components.Count);
                if (candidatePlan.Components.Count != imported.Meshes.Count)
                    continue;
                targetPart = candidate;
                modelPlan = candidatePlan;
                break;
            }
            if (targetPart is null || modelPlan is null)
            {
                throw new InvalidOperationException(
                    LevelText.Get("Catalog.MeshCountMismatch", item.Name, string.Join("/", componentCounts.Order()), imported.Meshes.Count));
            }

            if (!ConfirmImportValidation(SmoLevelImportValidator.Validate(
                    imported,
                    SmoLevelImportPurpose.ReplaceModelResource,
                    modelPlan.Components.Count)))
            {
                return;
            }

            SmoLevelAsset asset = targetPart.AssetItem.Asset!;
            SmoSceneMesh sourceMesh = targetPart.PreviewMesh!;
            Matrix4x4 referenceWorld = _document.TryGetPlacement(
                    new SmoPlacementId(
                        asset.ObjectIndex,
                        sourceMesh.SceneObjectIndex, sourceMesh.OccurrenceKey),
                    out SmoEditablePlacement? placement)
                ? placement!.WorldTransform
                : sourceMesh.WorldTransform;
            SetBusy(false, LevelText.Get("Catalog.ModelFitOpened"));
            var fitWindow = new ModelFitWindow(
                _workspace.Document,
                sourceMesh,
                imported,
                referenceWorld,
                dialog.FileName)
            {
                Owner = this
            };
            if (fitWindow.ShowDialog() != true)
                return;

            if (_document.ReplaceModel(
                    asset.ObjectIndex,
                    imported,
                    fitWindow.Transform,
                    referenceWorld,
                    dialog.FileName))
            {
                item.Status = LevelText.Label("Catalog.SharedResourceReplacementPending");
                _catalogPreviewCache.Remove(item.PreviewCacheKey);
                StatusText.Text =
                    LevelText.Get("Catalog.GeometryReplaced", asset.DisplayName, asset.Placements.Count);
            }
        }
        catch (Exception exception)
        {
            SetBusy(false, LevelText.Get("Catalog.ModelImportFailed"));
            MessageBox.Show(
                this,
                LevelText.Error(exception.Message),
                LevelText.Get("Catalog.ModelImportError"),
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
        finally
        {
            if (_isBusy)
                SetBusy(false, LevelText.Get("Catalog.ImportCompleted"));
        }
    }

    private bool ConfirmImportValidation(
        SmoLevelImportValidationReport report)
    {
        if (!report.CanImport)
        {
            SmoLevelImportValidator.ThrowIfInvalid(report);
            return false;
        }
        if (report.Warnings.Count == 0)
            return true;
        string warnings = string.Join(
            "\n• ",
            report.Warnings.Take(10).Select(LevelText.Error));
        if (report.Warnings.Count > 10)
            warnings += LevelText.Get("Catalog.MoreImportWarnings", report.Warnings.Count - 10);
        return MessageBox.Show(
                   this,
                   LevelText.Get("Catalog.ImportWarningsLead") +
                   warnings +
                   LevelText.Get("Catalog.ContinueImportPrompt"),
                   LevelText.Get("Catalog.ImportValidationTitle"),
                   MessageBoxButton.OKCancel,
                   MessageBoxImage.Warning) == MessageBoxResult.OK;
    }

    private Task ReplaceCompositeCatalogModelAsync(
        CatalogItem item,
        ImportedScene imported,
        string sourcePath)
    {
        if (_document is null || _workspace is null ||
            item.CompositeModels.Length == 0)
        {
            return Task.CompletedTask;
        }

        SmoCompositeModel reference = item.CompositeModels[0];
        SmoSceneMesh[] sourceMeshes = item.VisualParts
            .Select(part => part.PreviewMesh)
            .Where(mesh => mesh is not null)
            .Select(mesh => mesh!)
            .ToArray();
        if (sourceMeshes.Length == 0)
            throw new InvalidOperationException(
                LevelText.Get("Catalog.NoCompositeFitGeometry"));

        Matrix4x4 referenceWorld = reference.Entities[0].WorldTransform;
        SetBusy(false, LevelText.Get("Catalog.CompositeFitOpened"));
        var fitWindow = new ModelFitWindow(
            _workspace.Document,
            sourceMeshes,
            imported,
            referenceWorld,
            sourcePath,
            item.Name,
            item.CompositeModels.Length)
        {
            Owner = this
        };
        if (fitWindow.ShowDialog() != true)
            return Task.CompletedTask;

        if (HasProject)
        {
            return ReplaceProjectCatalogModelAsync(
                item,
                imported,
                fitWindow.Transform,
                referenceWorld,
                sourcePath);
        }

        Guid modelId = _document.ReplaceCompositeModels(
            item.CompositeModels,
            imported,
            fitWindow.Transform,
            referenceWorld,
            sourcePath,
            item.Name);
        _catalogPreviewCache.Remove(item.PreviewCacheKey);
        ApplyCatalogFilter();
        CatalogItem? replacementItem = AssetList.Items
            .OfType<CatalogItem>()
            .FirstOrDefault(candidate => candidate.ExternalModelId == modelId);
        if (replacementItem is not null)
        {
            replacementItem.Status =
                LevelText.Label("Catalog.CompositeReplacementPending", item.CompositeModels.Length);
            AssetList.SelectedItem = replacementItem;
            AssetList.ScrollIntoView(replacementItem);
        }
        StatusText.Text =
            LevelText.Get("Catalog.CompositeReplaced", item.Name, item.CompositeModels.Length, modelId);
        return Task.CompletedTask;
    }

    private static ImageSource DecodeReplacementPreview(ReadOnlyMemory<byte> encoded)
    {
        using var stream = new MemoryStream(encoded.ToArray(), writable: false);
        var bitmap = new BitmapImage();
        bitmap.BeginInit();
        bitmap.CacheOption = BitmapCacheOption.OnLoad;
        bitmap.StreamSource = stream;
        bitmap.EndInit();
        bitmap.Freeze();
        return bitmap;
    }

    private void RefreshCatalogResourceEdits()
    {
        if (_document is null)
            return;

        if (_catalogSection is CatalogSection.Models or CatalogSection.Placements)
        {
            string? selectedCatalogIdentity =
                (AssetList.SelectedItem as CatalogItem)?.PreviewCacheKey;
            PlacementSelectionKey? activeSelection = _activeSelectionKey;
            _catalogPreviewCache.Clear();
            ApplyCatalogFilter();
            CatalogItem? restoredCatalogItem = selectedCatalogIdentity is null
                ? null
                : AssetList.Items.OfType<CatalogItem>().FirstOrDefault(item =>
                    item.PreviewCacheKey == selectedCatalogIdentity);
            if (restoredCatalogItem is not null)
            {
                _syncingSelection = true;
                AssetList.SelectedItem = restoredCatalogItem;
                AssetList.ScrollIntoView(restoredCatalogItem);
                _syncingSelection = false;
                if (CatalogPlaceButton is not null)
                {
                    CatalogPlaceButton.IsEnabled =
                        _catalogSection == CatalogSection.Models;
                }
                return;
            }
            if (activeSelection is PlacementSelectionKey active)
            {
                EditorAssetItem? activeItem = _allAssets.FirstOrDefault(item =>
                    item.Asset?.ObjectIndex == active.MeshObjectIndex);
                if (activeItem is not null)
                    SelectCatalogItemForPlacement(activeItem, _activePlacementIndex);
            }
            return;
        }

        if (_catalogSection != CatalogSection.Textures)
            return;

        foreach (CatalogItem item in _catalogItems)
        {
            if (item.Texture is not SmoLevelTexture texture)
                continue;

            bool replaced = _document.TextureReplacements.ContainsKey(
                texture.ObjectIndex);
            _catalogPreviewCache.Remove(item.PreviewCacheKey);
            item.Preview = replaced && _textureReplacementPreviews.TryGetValue(
                    texture.ObjectIndex,
                    out ImageSource? preview)
                ? preview
                : null;
            item.Status = replaced
                ? LevelText.Label("Catalog.ReplacedRgbOriginalAlpha")
                : texture.SourceLayout;
        }

        _catalogPreviewGeneration++;
        Dispatcher.BeginInvoke(
            QueueVisibleCatalogPreviews,
            DispatcherPriority.Background);
    }

    private void ApplyPendingModelCatalogState()
    {
        if (_document is null || _workspace is null ||
            _document.ModelReplacements.Count == 0)
            return;

        var replacedMeshIndices = new Dictionary<int, SmoLevelModelReplacement>();
        foreach (SmoLevelModelReplacement replacement in
                 _document.ModelReplacements.Values)
        {
            SmoLevelModelGraphPlan plan = SmoLevelModelGraphReplacer.ResolvePlan(
                _workspace.Document,
                replacement.MeshObjectIndex);
            foreach (SmoLevelModelGraphComponent component in plan.Components)
                replacedMeshIndices[component.MeshObjectIndex] = replacement;
        }

        foreach (CatalogItem item in _catalogItems)
        {
            SmoLevelModelReplacement? replacement = item.VisualParts
                .Select(part => part.AssetItem.Asset?.ObjectIndex)
                .Where(index => index.HasValue)
                .Select(index => replacedMeshIndices.GetValueOrDefault(index!.Value))
                .FirstOrDefault(value => value is not null);
            if (replacement is null)
                continue;
            int textureCount = replacement.ImportedScene.Meshes
                .Select(mesh => mesh.MaterialIndex)
                .Where(index => index >= 0 &&
                    index < replacement.ImportedScene.Materials.Count)
                .Select(index => replacement.ImportedScene.Materials[index]
                    .BaseColorTextureIndex)
                .Where(index => index >= 0)
                .Distinct()
                .Count();
            int alphaMaterialCount = replacement.ImportedScene.Materials.Count(
                material => material.UsesTextureAlpha);
            string replacementName = Path.GetFileNameWithoutExtension(replacement.SourcePath);
            int meshCount = replacement.ImportedScene.Meshes.Count;
            item.Status = alphaMaterialCount > 0
                ? LevelText.Label("Catalog.ModelReplacementWithAlphaStatus", replacementName,
                    meshCount, textureCount, alphaMaterialCount)
                : LevelText.Label("Catalog.ModelReplacementStatus", replacementName,
                    meshCount, textureCount);
        }
    }

    private static string CatalogSectionTitle(CatalogSection section) => section switch
    {
        CatalogSection.Models => LevelText.Get("Catalog.ModelsSection"),
        CatalogSection.Placements => LevelText.Get("Catalog.PlacementsSection"),
        CatalogSection.Textures => LevelText.Get("Catalog.TexturesSection"),
        _ => LevelText.Get("Catalog.UnknownSection")
    };

    private enum CatalogSection
    {
        Models,
        Placements,
        Textures,
        Unknown
    }

    private sealed record CatalogVisualPart(
        EditorAssetItem AssetItem,
        int PlacementIndex,
        SmoSceneMesh? PreviewMesh);

    private sealed record CatalogPlacementPart(
        int MeshObjectIndex,
        Matrix4x4 SourceWorldTransform,
        string Name);

    private sealed record CatalogModelDrag(
        Guid? ExternalModelId,
        CatalogPlacementPart[] Parts,
        string Name,
        Vector3 SourceAnchor,
        float InitialScale)
    {
        public const string Format = "SmoLVLcreator.CatalogModel";
    }

    private sealed class CatalogItem : INotifyPropertyChanged
    {
        private ImageSource? _preview;
        private bool _previewLoading;
        private string? _status;
        private bool _isHidden;
        private readonly string _name = string.Empty;
        private readonly string _badge = string.Empty;
        private readonly string _kind = string.Empty;
        private readonly string _details = string.Empty;
        private readonly string _path = string.Empty;
        private readonly string _identity = string.Empty;
        private readonly string _message = string.Empty;

        private CatalogItem()
        {
        }

        public string Name { get => LevelText.Display(_name); private init => _name = value; }
        public string Badge { get => LevelText.Display(_badge); private init => _badge = value; }
        public string Kind
        {
            get => _kind.Length == 0 && UsesAssetDescriptions && AssetItem is { } asset
                ? asset.Kind
                : LevelText.Display(_kind);
            private init => _kind = value;
        }
        public string Icon { get; private init; } = "◇";
        public string Details
        {
            get => UsesAssetDescriptions && AssetItem is { } asset
                ? asset.Details
                : LevelText.Display(_details);
            private init => _details = value;
        }
        public string Status
        {
            get => _status is null && UsesAssetDescriptions && AssetItem is { } asset
                ? asset.Status
                : LevelText.Error(LevelText.Display(_status));
            set
            {
                if (_status == value)
                    return;
                _status = value;
                OnPropertyChanged();
            }
        }
        public Brush StatusColor { get; private init; } = HealthyBrush;
        public bool IsHidden
        {
            get => _isHidden;
            set
            {
                if (_isHidden == value)
                    return;
                _isHidden = value;
                OnPropertyChanged();
            }
        }
        public string SearchText { get; private init; } = string.Empty;
        public string Path { get => LevelText.Display(_path); private init => _path = value; }
        public string Identity { get => LevelText.Display(_identity); private init => _identity = value; }
        public string ObjectIdText { get; private init; } = "—";
        public string Message
        {
            get => _message.Length == 0 && UsesAssetDescriptions && AssetItem is { } asset
                ? asset.Status
                : LevelText.Error(LevelText.Display(_message));
            private init => _message = value;
        }
        public int? RawObjectIndex { get; private init; }
        public EditorAssetItem? AssetItem { get; private init; }
        private bool UsesAssetDescriptions { get; init; }
        public int PlacementIndex { get; private init; }
        public SmoLevelTexture? Texture { get; private init; }
        public Guid? ExternalModelId { get; private init; }
        public SmoSceneMesh? PreviewMesh { get; private init; }
        public SmoSceneMesh[] PreviewMeshes { get; private init; } = [];
        public CatalogVisualPart[] VisualParts { get; private init; } = [];
        public SmoCompositeModel[] CompositeModels { get; private init; } = [];
        public EditorAssetItem[] RelatedAssets { get; private init; } = [];
        public string PreviewError { get; set; } = string.Empty;
        public string PreviewCacheKey => ExternalModelId is Guid externalId
            ? $"E:{externalId}"
            : Texture is not null
            ? $"T:{Texture.ObjectIndex}"
            : PreviewMeshes.Length > 0
                ? "M:" + string.Join(',', PreviewMeshes
                    .Select(mesh => mesh.Mesh.ObjectIndex)
                    .Order())
                : $"N:{_kind}:{_name}";
        public ImageSource? Preview
        {
            get => _preview;
            set
            {
                if (ReferenceEquals(_preview, value))
                    return;
                _preview = value;
                OnPropertyChanged();
            }
        }

        public event PropertyChangedEventHandler? PropertyChanged;

        public void RefreshLanguage() => OnPropertyChanged(string.Empty);

        public bool TryBeginPreviewLoad()
        {
            if (_previewLoading)
                return false;
            _previewLoading = true;
            return true;
        }

        public void CompletePreviewLoad() => _previewLoading = false;

        public bool ContainsVisualPart(
            EditorAssetItem assetItem,
            int placementIndex,
            bool matchPlacement) =>
            VisualParts.Length > 0
                ? VisualParts.Any(part =>
                    ReferenceEquals(part.AssetItem, assetItem) &&
                    (!matchPlacement || part.PlacementIndex == placementIndex))
                : ReferenceEquals(AssetItem, assetItem) &&
                  (!matchPlacement || PlacementIndex == placementIndex);

        private void OnPropertyChanged([CallerMemberName] string? propertyName = null) =>
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));

        public static CatalogItem FromModel(
            EditorAssetItem item,
            SmoSceneMesh? previewMesh = null) => new()
        {
            Name = item.Name,
            Badge = item.PlacementBadge,
            Icon = "◇",
            StatusColor = item.StatusColor,
            SearchText = $"{item.Name} {item.Status} {item.Asset?.FullPath}",
            AssetItem = item,
            UsesAssetDescriptions = true,
            PlacementIndex = 0,
            PreviewMesh = previewMesh,
            PreviewMeshes = previewMesh is null ? [] : [previewMesh],
            VisualParts = [new CatalogVisualPart(item, 0, previewMesh)]
        };

        public static CatalogItem FromExternalModel(
            SmoLevelExternalModel model,
            SmoSceneMesh[] previewMeshes,
            int placementCount) => new()
        {
            Name = model.Name,
            Badge = placementCount > 0 ? $"×{placementCount}" : LevelText.Label("Catalog.NewBadge"),
            Kind = LevelText.Label("Catalog.ExternalModelKind"),
            Icon = "◇",
            Details = LevelText.Label("Catalog.MeshDetails",
                model.ImportedScene.Meshes.Sum(mesh => mesh.Positions.Length),
                model.ImportedScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3)),
            Status = LevelText.Label("Catalog.ExternalModelStatus", model.ImportedScene.Meshes.Count),
            StatusColor = NoticeBrush,
            SearchText = $"{model.Name} {model.SourcePath} external imported",
            ExternalModelId = model.Id,
            PreviewMesh = previewMeshes.FirstOrDefault(),
            PreviewMeshes = previewMeshes
        };

        public static CatalogItem FromCompositeModel(
            SmoCompositeModel model,
            CatalogVisualPart[] parts,
            SmoCompositeModel[] instances)
        {
            EditorAssetItem[] assets = parts
                .Select(part => part.AssetItem)
                .Distinct()
                .ToArray();
            SmoMesh[] previewMeshes = parts
                .Select(part => part.PreviewMesh?.Mesh)
                .Where(mesh => mesh is not null)
                .Select(mesh => mesh!)
                .GroupBy(mesh => mesh.ObjectIndex)
                .Select(group => group.First())
                .ToArray();
            int vertices = previewMeshes.Length > 0
                ? previewMeshes.Sum(mesh => mesh.VertexCount)
                : assets.Sum(asset => asset.VertexCount);
            int triangles = previewMeshes.Length > 0
                ? previewMeshes.Sum(mesh => mesh.TriangleCount)
                : assets.Sum(asset => asset.TriangleCount);
            bool hasIssue = assets.Any(asset => asset.HasIssue);
            return new CatalogItem
            {
                Name = model.Name,
                Badge = $"×{instances.Length}",
                Kind = LevelText.Label("Catalog.CompositeModelKind"),
                Icon = "◇",
                Details = LevelText.Label("Catalog.MeshDetails", vertices, triangles),
                Status = LevelText.Label("Catalog.CompositeModelStatus", parts.Length),
                StatusColor = hasIssue ? ErrorBrush : HealthyBrush,
                SearchText = $"{model.Name} composite {instances.Length} placements " +
                    string.Join(' ', assets.Select(asset =>
                        $"{asset.Name} {asset.Asset?.FullPath}")),
                AssetItem = parts[0].AssetItem,
                PlacementIndex = parts[0].PlacementIndex,
                PreviewMesh = parts[0].PreviewMesh,
                PreviewMeshes = parts.Select(part => part.PreviewMesh)
                    .Where(mesh => mesh is not null)
                    .Select(mesh => mesh!)
                    .ToArray(),
                VisualParts = parts,
                CompositeModels = instances,
                RelatedAssets = assets
            };
        }

        public static CatalogItem FromPlacement(
            EditorAssetItem item,
            int index,
            Matrix4x4 transform,
            SmoSceneMesh? previewMesh)
        {
            SmoLevelPlacement placement = item.Asset!.Placements[index];
            Vector3 position = new(transform.M41, transform.M42, transform.M43);
            string placementName = placement.Name.TrimEnd('\0');
            if (string.IsNullOrWhiteSpace(placementName))
                placementName = item.Name;
            string coordinates = string.Format(
                CultureInfo.CurrentCulture,
                "X {0:0.##} · Y {1:0.##} · Z {2:0.##}",
                position.X,
                position.Y,
                position.Z);
            return new CatalogItem
            {
                Name = placementName,
                Badge = $"#{index + 1}",
                Kind = LevelText.Label(placement.IsSharedInstance
                    ? "Catalog.SharedPlacementKind"
                    : "Catalog.EmbeddedPlacementKind"),
                Icon = "⌖",
                Details = coordinates,
                Status = item.Name,
                StatusColor = HealthyBrush,
                SearchText = $"{placementName} {item.Name} {coordinates} {item.Asset.FullPath}",
                AssetItem = item,
                PlacementIndex = index,
                PreviewMesh = previewMesh,
                PreviewMeshes = previewMesh is null ? [] : [previewMesh],
                VisualParts = [new CatalogVisualPart(item, index, previewMesh)]
            };
        }

        public static CatalogItem FromTexture(
            SmoLevelTexture texture,
            EditorAssetItem[] users) => new()
        {
            Name = texture.Name.TrimEnd('\0'),
            Badge = $"×{users.Length}",
            Kind = LevelText.Label("Catalog.TextureKind"),
            Icon = "▧",
            Details = $"{texture.Width:N0} × {texture.Height:N0} · 0x{texture.FormatCode:X4}",
            Status = texture.SourceLayout,
            StatusColor = HealthyBrush,
            SearchText = $"{texture.Name} {texture.SourceLayout} 0x{texture.FormatCode:X4} " +
                         string.Join(' ', users.Select(user => user.Name)),
            Texture = texture,
            RelatedAssets = users
        };

        public static CatalogItem FromUnknownObject(SmoObjectEntry entry)
        {
            string name = string.IsNullOrWhiteSpace(entry.Name)
                ? $"Object_{entry.Index}"
                : entry.Name.TrimEnd('\0');
            return new CatalogItem
            {
                Name = name,
                Badge = $"[{entry.Index}]",
                Kind = LevelText.Label("Catalog.UnknownClassKind"),
                Icon = "?",
                Details = LevelText.Label("Catalog.UnknownObjectDetails", entry.TypeHash, entry.SerializedSize),
                Status = LevelText.Label("Catalog.UnregisteredClass"),
                StatusColor = NoticeBrush,
                SearchText = $"{name} {entry.Index} 0x{entry.TypeHash:X8}",
                Path = LevelText.Label("Catalog.ObjectDirectoryPath", entry.Index),
                Identity = LevelText.Label("Catalog.UnknownObjectIdentity", entry.TypeHash, entry.Index),
                ObjectIdText = $"0x{entry.Id:X8}",
                RawObjectIndex = entry.Index,
                Message = LevelText.Label("Catalog.UnknownClassDiagnostic")
            };
        }

        public static CatalogItem FromDiagnostic(SmoDiagnostic diagnostic) => new()
        {
            Name = diagnostic.Code,
            Badge = diagnostic.ObjectIndex is int index ? $"[{index}]" : DiagnosticSeverity(diagnostic.Severity),
            Kind = DiagnosticSeverity(diagnostic.Severity),
            Icon = diagnostic.Severity == SmoDiagnosticSeverity.Error ? "!" : "?",
            Details = diagnostic.Offset is long offset
                ? LevelText.Label("Catalog.DiagnosticOffset", offset)
                : LevelText.Label("Catalog.ParserDiagnostic"),
            Status = diagnostic.Message,
            StatusColor = diagnostic.Severity == SmoDiagnosticSeverity.Error ? ErrorBrush : NoticeBrush,
            SearchText = $"{diagnostic.Code} {diagnostic.Message} {diagnostic.ObjectIndex}",
            Path = diagnostic.ObjectIndex is int objectIndex
                ? LevelText.Label("Catalog.ObjectDirectoryPath", objectIndex)
                : LevelText.Label("Catalog.SmoParserPath"),
            Identity = diagnostic.Code,
            RawObjectIndex = diagnostic.ObjectIndex,
            Message = diagnostic.Message
        };

        public static CatalogItem FromIssue(string category, string issue) => new()
        {
            Name = category,
            Badge = LevelText.Label("Catalog.IssueBadge"),
            Kind = category,
            Icon = "!",
            Details = LevelText.Label("Catalog.DecodeDiagnostic"),
            Status = issue,
            StatusColor = NoticeBrush,
            SearchText = $"{LevelText.Display(category)} {issue}",
            Path = LevelText.Label("Catalog.DecodePath"),
            Identity = category,
            Message = issue
        };

        public static CatalogItem FromModelIssue(EditorAssetItem item) => new()
        {
            Name = item.Name,
            Badge = LevelText.Label("Catalog.IssueBadge"),
            Kind = LevelText.Label("Catalog.ModelKind"),
            Icon = "!",
            StatusColor = item.StatusColor,
            SearchText = $"{item.Name} {item.Status}",
            Path = item.Asset?.FullPath ?? LevelText.Label("Catalog.MockupPath"),
            Identity = "spMeshData",
            RawObjectIndex = item.Asset?.ObjectIndex,
            AssetItem = item,
            UsesAssetDescriptions = true
        };

        private static string DiagnosticSeverity(SmoDiagnosticSeverity severity) => LevelText.Label(severity switch
        {
            SmoDiagnosticSeverity.Error => "Catalog.DiagnosticError",
            SmoDiagnosticSeverity.Warning => "Catalog.DiagnosticWarning",
            _ => "Catalog.DiagnosticInfo"
        });
    }
}
