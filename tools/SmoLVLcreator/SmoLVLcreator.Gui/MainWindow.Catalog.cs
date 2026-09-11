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
            StatusText.Text = $"Каталог: {CatalogSectionTitle(section)}";
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
        AssetCountText.Text = $"{visible.Length:N0} {CatalogSectionCountNoun(_catalogSection)}";
        if (AssetFilterBox is not null)
        {
            AssetFilterBox.ToolTip =
                $"Поиск: {CatalogSectionTitle(_catalogSection).ToLowerInvariant()}";
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
            _catalogItems.Add(CatalogItem.FromIssue("DECODE", issue));
        foreach (string issue in _workspace.PreparedScene.TextureIssues)
            _catalogItems.Add(CatalogItem.FromIssue("TEXTURE", issue));
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
                ? $"Выбрано в каталоге: {selection[0].Name} · двойной клик — показать в сцене"
                : $"Выбрано в каталоге: {selection.Length:N0}";
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
            item.PreviewError = exception.Message;
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
            ? $"Выбрано размещение · {active.Name} #{activeIndex + 1}"
            : $"Выбрано объектов: {_selectedEntities.Count:N0} · Ctrl+клик меняет набор";
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
        SelectedPathText.Text = $"Texture catalog / [{texture.ObjectIndex}]";
        ObjectIdentityText.Text = $"spTextureData · [{texture.ObjectIndex}]";
        ObjectIdText.Text = entry is null ? "—" : $"0x{entry.Id:X8}";
        SetPositionEditor(null);
        SetRotationScaleEditor(null, null);
        PlacementCountText.Text = $"{item.RelatedAssets.Length:N0} моделей используют текстуру";
        GeometryCountText.Text = $"{texture.Width:N0} × {texture.Height:N0}";
        VertexLayoutText.Text = $"format 0x{texture.FormatCode:X4}";
        ChannelsText.Text = texture.SourceLayout;
        TextureNameText.Text = texture.Name.TrimEnd('\0');
        MaterialText.Text = item.RelatedAssets.Length == 0
            ? "Нет подтверждённой привязки к модели"
            : string.Join(" · ", item.RelatedAssets.Take(3).Select(asset => asset.Name));
        BindingsText.Text = item.RelatedAssets.Length == 0
            ? "spTextureData · отдельный ресурс уровня"
            : $"spTextureData → {item.RelatedAssets.Length:N0} model binding(s)";
        DiagnosticsText.Text = "Текстура декодирована общим SmoViewer.Core.";
        DiagnosticsText.Foreground = HealthyBrush;
        ViewportSelectionText.Text = $"RESOURCE · TEXTURE · {texture.Name.TrimEnd('\0')}";
        StatusText.Text = $"Текстура · {texture.Width} × {texture.Height} · 0x{texture.FormatCode:X4}";
        UpdateRawData(("texture resource", texture.ObjectIndex));
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
        ViewportSelectionText.Text = $"RESOURCE · {item.Kind} · {item.Name}";
        StatusText.Text = $"{item.Kind} · {item.Name}";
        UpdateRawData(("catalog object", item.RawObjectIndex));
    }

    private void FocusCatalogSelection()
    {
        if (AssetList.SelectedItem is not CatalogItem item ||
            item.AssetItem?.Asset is not SmoLevelAsset asset)
        {
            StatusText.Text = "Этот ресурс не имеет размещения в сцене";
            return;
        }
        if (item.VisualParts.Length > 1)
        {
            FrameMeshes(item.VisualParts
                .Select(part => part.PreviewMesh)
                .Where(mesh => mesh is not null)
                .Select(mesh => mesh!));
            StatusText.Text = $"Фокус · {item.Name} · {item.VisualParts.Length} частей";
            return;
        }
        int index = Math.Clamp(item.PlacementIndex, 0, asset.Placements.Count - 1);
        FocusPlacement(asset, index);
        StatusText.Text = $"Фокус · {item.AssetItem.Name} #{index + 1}";
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
            Title = "Добавить внешнюю модель в каталог уровня",
            Filter = "3D-модели (*.glb;*.fbx;*.obj)|*.glb;*.fbx;*.obj|" +
                     "glTF Binary (*.glb)|*.glb|FBX (*.fbx)|*.fbx|OBJ (*.obj)|*.obj",
            CheckFileExists = true,
            Multiselect = false
        };
        if (dialog.ShowDialog(this) != true)
            return;
        try
        {
            SetBusy(true, $"Чтение {Path.GetFileName(dialog.FileName)}…");
            ImportedScene imported = await Task.Run(() =>
                SmoLevelEmbeddedTextureBudget.Prepare(
                    ImportedModelReader.Read(dialog.FileName)));
            SetBusy(false, "Проверка модели завершена");
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
            SetBusy(true, "Добавление модели в каталог…");
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
                $"{Path.GetFileName(dialog.FileName)} добавлен в каталог · перетащите его на сцену");
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось добавить внешнюю модель");
            MessageBox.Show(this, exception.Message,
                "SmoLVLcreator — добавление модели",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private async void CatalogPlace_Click(object sender, RoutedEventArgs e)
    {
        CatalogItem? item = AssetList.SelectedItem as CatalogItem ?? _catalogDragItem;
        if (item is null)
        {
            StatusText.Text = "Сначала выберите модель в нижнем каталоге";
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
        StatusText.Text = $"Размещаю {item.Name}…";
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
            error = $"{item.Name}: нет размещаемой геометрии";
            return false;
        }
        foreach (CatalogPlacementPart part in parts)
        {
            if (!HasProject && !SmoSharedPlacementCloner.CanClone(
                    _workspace!.Document,
                    part.MeshObjectIndex))
            {
                error = $"{part.Name}: в SMO нет ссылочного шаблона размещения";
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
                "Transient external-preview object ID space is exhausted.");
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
                "Для модели будет открываться отдельное окно подгонки. " +
                "Сейчас подключён первый общий ресурсный путь — замена текстур; " +
                "редактор геометрии идёт следующим этапом.",
                "SmoLVLcreator — замена модели",
                MessageBoxButton.OK,
                MessageBoxImage.Information);
            return;
        }

        var dialog = new OpenFileDialog
        {
            Title = $"Заменить текстуру {texture.Name.TrimEnd('\0')}",
            Filter = "Изображения (*.png;*.jpg;*.jpeg;*.bmp;*.tga)|" +
                     "*.png;*.jpg;*.jpeg;*.bmp;*.tga|" +
                     "PNG (*.png)|*.png|JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg|" +
                     "BMP (*.bmp)|*.bmp|TGA (*.tga)|*.tga",
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
                "Как поступить с альфа-каналом?\n\n" +
                "Да — перенести RGB и alpha из нового изображения.\n" +
                "Нет — заменить RGB, но сохранить исходную alpha из SMO.\n" +
                "Отмена — ничего не менять.",
                "Замена текстуры — альфа-канал",
                MessageBoxButton.YesNoCancel,
                MessageBoxImage.Question);
            if (alphaChoice == MessageBoxResult.Cancel)
                return;
            bool replaceAlpha = alphaChoice == MessageBoxResult.Yes;
            ImageSource preview = DecodeReplacementPreview(encoded);
            if (HasProject)
            {
                uint textureObjectId = _workspace.Document.Objects[texture.ObjectIndex].Id;
                SetBusy(true, $"Замена текстуры {texture.Name.TrimEnd('\0')} в проекте…");
                bool changed = await Task.Run(() =>
                    _projectSession!.Execute(
                        $"Заменить текстуру {texture.Name.TrimEnd('\0')}",
                        project => SmoProjectTextureReplacement.Replace(
                            project,
                            textureObjectId,
                            encoded,
                            replaceAlpha)));
                if (!changed)
                {
                    SetBusy(false, "Текстура не изменилась");
                    return;
                }
                _textureReplacementPreviews[texture.ObjectIndex] = preview;
                _catalogPreviewCache.Remove($"T:{texture.ObjectIndex}");
                await RefreshProjectPreviewAsync(
                    $"Текстура {texture.Name.TrimEnd('\0')} заменена · " +
                    (replaceAlpha ? "RGB + alpha" : "RGB · исходная alpha сохранена"));
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
                ? "ЗАМЕНЕНА · RGB + ALPHA"
                : "ЗАМЕНЕНА · RGB · исходная alpha сохранена";
            StatusText.Text =
                $"Текстура {texture.Name.TrimEnd('\0')} заменена · " +
                "размер будет приведён к слоту SMO при сохранении";
        }
        catch (Exception exception)
        {
            if (_isBusy)
                SetBusy(false, "Не удалось заменить текстуру");
            MessageBox.Show(
                this,
                exception.Message,
                "SmoLVLcreator — ошибка замены текстуры",
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
                "У выбранной карточки нет доступной геометрии для замены.",
                "SmoLVLcreator — замена модели",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
            return;
        }

        var dialog = new OpenFileDialog
        {
            Title = $"Заменить модель {item.Name}",
            Filter = "3D-модели (*.glb;*.fbx;*.obj)|*.glb;*.fbx;*.obj|" +
                     "glTF Binary (*.glb)|*.glb|FBX (*.fbx)|*.fbx|OBJ (*.obj)|*.obj",
            CheckFileExists = true,
            Multiselect = false
        };
        if (dialog.ShowDialog(this) != true)
            return;

        try
        {
            SetBusy(true, $"Чтение {Path.GetFileName(dialog.FileName)}…");
            ImportedScene imported = await Task.Run(() =>
                SmoLevelEmbeddedTextureBudget.Prepare(
                    ImportedModelReader.Read(dialog.FileName)));

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
                SetBusy(false, "Открыто окно подгонки полной замены модели");
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
                    $"Карточка «{item.Name}» содержит SMO-ресурсы на " +
                    $"{string.Join("/", componentCounts.Order())} мешей, " +
                    $"а внешняя — из {imported.Meshes.Count}. " +
                    "Для полной замены число мешей одной ресурсной модели должно совпадать.");
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
            SetBusy(false, "Открыто окно подгонки модели");
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
                item.Status = "ЗАМЕНЕНА · ресурс SMO · ожидает сохранения";
                _catalogPreviewCache.Remove(item.PreviewCacheKey);
                StatusText.Text =
                    $"Геометрия {asset.DisplayName} заменена · " +
                    $"все {asset.Placements.Count:N0} размещений используют новый ресурс";
            }
        }
        catch (Exception exception)
        {
            SetBusy(false, "Не удалось импортировать модель");
            MessageBox.Show(
                this,
                exception.Message,
                "SmoLVLcreator — импорт модели",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
        finally
        {
            if (_isBusy)
                SetBusy(false, "Импорт модели завершён");
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
            report.Warnings.Take(10));
        if (report.Warnings.Count > 10)
            warnings += $"\n• …ещё {report.Warnings.Count - 10:N0}";
        return MessageBox.Show(
                   this,
                   "Модель совместима, но есть предупреждения:\n\n• " +
                   warnings +
                   "\n\nПродолжить импорт?",
                   "SmoLVLcreator — проверка импорта",
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
                "У составной модели нет доступной геометрии для окна подгонки.");

        Matrix4x4 referenceWorld = reference.Entities[0].WorldTransform;
        SetBusy(false, "Открыто окно подгонки составной модели");
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
                $"ЗАМЕНЕНА ЦЕЛИКОМ · {item.CompositeModels.Length:N0} размещений · " +
                "ожидает сохранения";
            AssetList.SelectedItem = replacementItem;
            AssetList.ScrollIntoView(replacementItem);
        }
        StatusText.Text =
            $"Составная модель {item.Name} заменена целиком · " +
            $"{item.CompositeModels.Length:N0} размещений используют новый ресурс {modelId}";
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
                ? "ЗАМЕНЕНА · RGB · исходная альфа сохранена"
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
            item.Status =
                $"ЗАМЕНЕНА · {Path.GetFileNameWithoutExtension(replacement.SourcePath)} · " +
                $"{replacement.ImportedScene.Meshes.Count:N0} мешей · " +
                $"{textureCount:N0} текстур" +
                (alphaMaterialCount > 0
                    ? $" · alpha {alphaMaterialCount:N0}"
                    : string.Empty);
        }
    }

    private static string CatalogSectionTitle(CatalogSection section) => section switch
    {
        CatalogSection.Models => "Модели",
        CatalogSection.Placements => "Размещения",
        CatalogSection.Textures => "Текстуры",
        _ => "Неопознанное и диагностика"
    };

    private static string CatalogSectionCountNoun(CatalogSection section) => section switch
    {
        CatalogSection.Models => "моделей",
        CatalogSection.Placements => "размещений",
        CatalogSection.Textures => "текстур",
        _ => "записей"
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
        private string _status = string.Empty;
        private bool _isHidden;

        private CatalogItem()
        {
        }

        public string Name { get; private init; } = string.Empty;
        public string Badge { get; private init; } = string.Empty;
        public string Kind { get; private init; } = string.Empty;
        public string Icon { get; private init; } = "◇";
        public string Details { get; private init; } = string.Empty;
        public string Status
        {
            get => _status;
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
        public string Path { get; private init; } = string.Empty;
        public string Identity { get; private init; } = string.Empty;
        public string ObjectIdText { get; private init; } = "—";
        public string Message { get; private init; } = string.Empty;
        public int? RawObjectIndex { get; private init; }
        public EditorAssetItem? AssetItem { get; private init; }
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
                : $"N:{Kind}:{Name}";
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
            Kind = item.Kind,
            Icon = "◇",
            Details = item.Details,
            Status = item.Status,
            StatusColor = item.StatusColor,
            SearchText = $"{item.Name} {item.Status} {item.Asset?.FullPath}",
            AssetItem = item,
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
            Badge = placementCount > 0 ? $"×{placementCount}" : "NEW",
            Kind = "EXTERNAL",
            Icon = "◇",
            Details = $"{model.ImportedScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} verts · " +
                      $"{model.ImportedScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} tris",
            Status = $"Внешняя модель · {model.ImportedScene.Meshes.Count:N0} мешей · " +
                     "будет упакована при сохранении",
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
                Kind = "COMPOSITE",
                Icon = "◇",
                Details = $"{vertices:N0} verts · {triangles:N0} tris",
                Status = $"Составная модель · {parts.Length:N0} мешей",
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
                Kind = placement.IsSharedInstance ? "SHARED" : "EMBEDDED",
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
            Kind = "TEXTURE",
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
                Kind = "UNKNOWN CLASS",
                Icon = "?",
                Details = $"0x{entry.TypeHash:X8} · {entry.SerializedSize:N0} bytes",
                Status = "Класс пока не зарегистрирован",
                StatusColor = NoticeBrush,
                SearchText = $"{name} {entry.Index} 0x{entry.TypeHash:X8}",
                Path = $"Object directory / [{entry.Index}]",
                Identity = $"unknown 0x{entry.TypeHash:X8} · [{entry.Index}]",
                ObjectIdText = $"0x{entry.Id:X8}",
                RawObjectIndex = entry.Index,
                Message = "Объект сохранён в исходном SMO, но его класс пока не описан в SmoClassRegistry."
            };
        }

        public static CatalogItem FromDiagnostic(SmoDiagnostic diagnostic) => new()
        {
            Name = diagnostic.Code,
            Badge = diagnostic.ObjectIndex is int index ? $"[{index}]" : diagnostic.Severity.ToString().ToUpperInvariant(),
            Kind = diagnostic.Severity.ToString().ToUpperInvariant(),
            Icon = diagnostic.Severity == SmoDiagnosticSeverity.Error ? "!" : "?",
            Details = diagnostic.Offset is long offset ? $"offset 0x{offset:X}" : "parser diagnostic",
            Status = diagnostic.Message,
            StatusColor = diagnostic.Severity == SmoDiagnosticSeverity.Error ? ErrorBrush : NoticeBrush,
            SearchText = $"{diagnostic.Code} {diagnostic.Message} {diagnostic.ObjectIndex}",
            Path = diagnostic.ObjectIndex is int objectIndex ? $"Object directory / [{objectIndex}]" : "SMO parser",
            Identity = diagnostic.Code,
            RawObjectIndex = diagnostic.ObjectIndex,
            Message = diagnostic.Message
        };

        public static CatalogItem FromIssue(string category, string issue) => new()
        {
            Name = category,
            Badge = "ISSUE",
            Kind = category,
            Icon = "!",
            Details = "Диагностика декодирования",
            Status = issue,
            StatusColor = NoticeBrush,
            SearchText = $"{category} {issue}",
            Path = "Scene decoding",
            Identity = category,
            Message = issue
        };

        public static CatalogItem FromModelIssue(EditorAssetItem item) => new()
        {
            Name = item.Name,
            Badge = "ISSUE",
            Kind = "MODEL",
            Icon = "!",
            Details = item.Details,
            Status = item.Status,
            StatusColor = item.StatusColor,
            SearchText = $"{item.Name} {item.Status}",
            Path = item.Asset?.FullPath ?? "Mockup",
            Identity = "spMeshData",
            RawObjectIndex = item.Asset?.ObjectIndex,
            Message = item.Status,
            AssetItem = item
        };
    }
}
