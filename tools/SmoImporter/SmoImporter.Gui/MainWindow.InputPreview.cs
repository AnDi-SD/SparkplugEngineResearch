using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private readonly record struct PreviewAlpha(bool Blend, uint Test = 0, byte Reference = 0);
    private readonly record struct PreviewImageKey(int Texture, PreviewAlpha Alpha, string? Uri = null);
    private sealed record NativePreviewSurface(
        SmoLoadedTexture? Reference, SmoMaterialTextureStage? Stage, PreviewAlpha Alpha, Color Color, bool Layered);

    private IReadOnlyList<SmoModelReader.NativePreviewMaterial> _nativeDonorPreviewMaterials = [];
    private SmoExportScene? _shownInputSource;
    private ImportedScene? _shownInputDonor;
    private ImportedScene? _shownDonorAppearance;
    private IReadOnlyList<SmoModelReader.NativePreviewMaterial>? _shownDonorMaterials;
    private Model3DGroup? _texturedDonorPreview;
    private Model3DGroup? _placementDonorPreview;
    private SmoExportScene? _placementSourceScene;
    private Model3DGroup? _placementSourcePreview;
    private bool _placementSourceBuilt;
    private string? _sourcePreviewNote;
    private string? _donorPreviewNote;

    private static readonly Color PlacementSourceColor = Color.FromArgb(86, 153, 174, 197);
    private static readonly Color PlacementDonorColor = Color.FromRgb(98, 207, 183);

    private void ReleaseSourceInputPreview()
    {
        SourceSelectionVisual.Content = null;
        _sourcePreviewNote = null;
    }

    private void ReleaseDonorInputPreview()
    {
        DonorSelectionVisual.Content = null;
        _shownInputDonor = null;
        _shownDonorAppearance = null;
        _shownDonorMaterials = null;
        _texturedDonorPreview = null;
        _placementDonorPreview = null;
        _fullDonorPreview = null;
        _donorMeshVisuals = [];
        _donorPreviewNote = null;
        RefreshTextureFocusPreview();
    }

    private void RefreshSourceInputPreview()
    {
        bool sameSource = ReferenceEquals(_shownInputSource, _sourceScene);
        if (!sameSource || (_sourceScene is not null && SourceSelectionVisual.Content is null && _sourcePreviewNote is null))
        {
            _shownInputSource = _sourceScene;
            _sourcePreviewNote = SetInputPreview(SourceSelectionVisual,
                _sourceScene is { } source && _document is { } document
                    ? () => BuildSourceInputPreview(source, document) : null,
                preserveOrientation: sameSource);
        }

        SourcePreviewEmptyText.Visibility = SourceSelectionVisual.Content is null ? Visibility.Visible : Visibility.Collapsed;
        SourcePreviewEmptyText.Text = HasInputSource ? "Не удалось показать модель" : "Перетащите сюда исходный SMO";
        if (_sourcePreviewNote is { } note && SourceSelectionVisual.Content is not null)
        {
            SourceSelectionText.Text += $"  ·  {note}";
            SourceSelectionText.ToolTip = $"{_sourcePath}\n{note}";
        }
    }

    private void RefreshDonorInputPreview()
    {
        ImportedScene? appearance = _replacementScene;
        var materials = _editedNativeDonorMaterials ?? _nativeDonorPreviewMaterials;
        bool sameDonor = ReferenceEquals(_shownInputDonor, _baseReplacementScene);
        if (!sameDonor || !ReferenceEquals(_shownDonorAppearance, appearance) ||
            !ReferenceEquals(_shownDonorMaterials, materials))
        {
            bool reframe = !sameDonor || _texturedDonorPreview is null;
            _shownInputDonor = _baseReplacementScene;
            _shownDonorAppearance = appearance;
            _shownDonorMaterials = materials;
            GeometryModel3D?[] meshVisuals = [];
            _donorPreviewNote = SetInputPreview(DonorSelectionVisual,
                appearance is null ? null : () => BuildDonorInputPreview(appearance, materials, out meshVisuals),
                preserveOrientation: sameDonor, reframe: reframe);
            _texturedDonorPreview = DonorSelectionVisual.Content as Model3DGroup;
            _placementDonorPreview = null;
            _donorMeshVisuals = meshVisuals;
        }

        if (WizardShowsComparison && _texturedDonorPreview is not null)
            _placementDonorPreview ??= BuildPlacementDonorPreview(_texturedDonorPreview);
        _fullDonorPreview = WizardShowsComparison ? _placementDonorPreview : _texturedDonorPreview;
        RefreshPlacementSourcePreview();

        DonorPreviewEmptyText.Visibility = _fullDonorPreview is null ? Visibility.Visible : Visibility.Collapsed;
        DonorPreviewEmptyText.Text = _replacementLoadRunning ? "Загружается модель…" :
            HasInputDonor ? "Не удалось показать модель" : "Перетащите сюда SMO, FBX, GLB или OBJ";
        if (!WizardShowsComparison && _donorPreviewNote is { } note && _fullDonorPreview is not null)
        {
            DonorSelectionText.Text += $"  ·  {note}";
            DonorSelectionText.ToolTip = $"{_replacementPath}\n{note}";
        }
        RefreshTextureFocusPreview();
    }

    private void RefreshPlacementSourcePreview()
    {
        if (!ReferenceEquals(_placementSourceScene, _sourceScene))
        {
            _placementSourceScene = _sourceScene;
            _placementSourcePreview = null;
            _placementSourceBuilt = false;
        }

        if (!WizardShowsSourceComparison || _sourceScene is not { } source || _document is not { } document)
        {
            PlacementSourceVisual.Content = null;
            ClearWizardRigPreview();
            return;
        }
        if (TryShowWizardPosedSource(source, document))
            return;

        if (!_placementSourceBuilt)
        {
            SetInputPreview(PlacementSourceVisual,
                () => BuildSourceInputPreview(source, document, PlacementSourceColor),
                preserveOrientation: true, reframe: false);
            _placementSourcePreview = PlacementSourceVisual.Content as Model3DGroup;
            _placementSourceBuilt = true;
        }
        else
        {
            PlacementSourceVisual.Content = _placementSourcePreview;
        }
    }

    private static Model3DGroup BuildPlacementDonorPreview(Model3DGroup textured)
    {
        var material = new DiffuseMaterial(new SolidColorBrush(PlacementDonorColor));
        var group = new Model3DGroup();
        foreach (GeometryModel3D mesh in textured.Children.OfType<GeometryModel3D>())
        {
            // Share the frozen geometry with the preparation preview, changing only its material.
            group.Children.Add(new GeometryModel3D(mesh.Geometry, material)
            {
                BackMaterial = material,
                Transform = mesh.Transform
            });
        }
        group.Freeze();
        return group;
    }

    private string? SetInputPreview(ModelVisual3D visual,
        Func<(Model3DGroup Group, string? Note)>? build, bool preserveOrientation, bool reframe = true)
    {
        visual.Content = null;
        if (build is null)
            return null;
        try
        {
            var (group, note) = build();
            Rect3D bounds = group.Bounds;
            if (bounds.IsEmpty)
                return "Нет видимой геометрии";
            if (reframe)
                FrameInputPreview(visual, bounds, preserveOrientation);
            group.Freeze();
            visual.Content = group;
            return note;
        }
        catch (Exception exception) when (exception is InvalidDataException or InvalidOperationException or ArgumentException or OverflowException)
        {
            SessionLog.Error("INPUT_PREVIEW", exception);
            return "Предпросмотр недоступен";
        }
    }

    private static (Model3DGroup, string?) BuildSourceInputPreview(SmoExportScene scene, SmoDocument document,
        Color? plainColor = null,
        Action<SmoExportMeshPlacement, SmoExportMesh, GeometryModel3D>? captureModel = null)
    {
        var group = new Model3DGroup();
        var images = new Dictionary<PreviewImageKey, Material?>();
        var draws = plainColor.HasValue ? null : SmoLoadedResources.Get(document).PreviewMaterialDraws;
        var meshes = scene.Meshes.ToDictionary(mesh => mesh.VariantKey);
        bool missing = false, layered = false;
        foreach (var placement in scene.MeshPlacements)
        {
            var mesh = meshes[placement.EffectiveMeshKey];
            GeometryModel3D? model;
            if (plainColor is Color color)
            {
                model = AddInputMesh(group, mesh.Positions, mesh.Normals, mesh.TriangleIndices, [], null, null, color);
            }
            else
            {
                var draw = mesh.LoadedMaterial is { } material ? draws?.GetValueOrDefault(material.ObjectIndex) : null;
                var surface = ResolveNativePreviewSurface(mesh.LoadedMaterial, draw, mesh.MaterialColor, mesh.UsesAlphaBlend);
                var uv = surface.Stage?.Coordinates == 1 ? mesh.TextureCoordinates1 : mesh.TextureCoordinates0;
                Material? image = ResolveNativePreviewImage(surface, mesh.Texture, document, images);
                bool textured = image is not null && uv.Length == mesh.Positions.Length;
                missing |= !textured && (surface.Reference is not null || mesh.Texture is not null);
                layered |= surface.Layered;
                model = AddInputMesh(group, mesh.Positions, mesh.Normals, mesh.TriangleIndices, uv,
                    surface.Stage, textured ? image : null, surface.Color);
            }
            if (model is not null && mesh.SkinObjectIndex is null && placement.WorldMatrix != Matrix4x4.Identity)
            {
                Matrix4x4 world = placement.WorldMatrix;
                model.Transform = new MatrixTransform3D(new Matrix3D(
                    world.M11, world.M12, world.M13, world.M14, world.M21, world.M22, world.M23, world.M24,
                    world.M31, world.M32, world.M33, world.M34, world.M41, world.M42, world.M43, world.M44));
            }
            if (model is not null)
                captureModel?.Invoke(placement, mesh, model);
        }
        return (group, PreviewTextureNote(missing, layered));
    }

    private (Model3DGroup, string?) BuildDonorInputPreview(ImportedScene scene,
        IReadOnlyList<SmoModelReader.NativePreviewMaterial> nativeMaterials, out GeometryModel3D?[] meshVisuals)
    {
        var group = new Model3DGroup();
        meshVisuals = new GeometryModel3D?[scene.Meshes.Count];
        var images = new Dictionary<PreviewImageKey, Material?>();
        bool missing = false, layered = false, externalImageShown = false;
        for (int index = 0; index < scene.Meshes.Count; index++)
        {
            ImportedMesh mesh = scene.Meshes[index];
            Material? image;
            Vector2[] uv = mesh.TextureCoordinates;
            SmoMaterialTextureStage? stage = null;
            Color color = Color.FromRgb(177, 183, 191);
            if (index < nativeMaterials.Count)
            {
                var material = nativeMaterials[index];
                var surface = ResolveNativePreviewSurface(material.Material, material.Draw, Vector4.One, false);
                stage = surface.Stage;
                uv = stage?.Coordinates == 1 ? mesh.SecondaryTextureCoordinates : uv;
                color = surface.Color;
                image = ResolveNativePreviewImage(surface, null, null, images);
                missing |= surface.Reference is not null && (image is null || uv.Length != mesh.Positions.Length);
                layered |= surface.Layered;
            }
            else
            {
                var material = (uint)mesh.MaterialIndex < (uint)scene.Materials.Count
                    ? scene.Materials[mesh.MaterialIndex] : null;
                image = ResolveExternalPreviewImage(scene, material, _replacementPath, images);
                externalImageShown |= image is not null && uv.Length == mesh.Positions.Length &&
                    material is { PreviewTextureUri: not null, BaseColorTextureIndex: < 0 };
                missing |= (material is { BaseColorTextureIndex: >= 0 } ||
                    !string.IsNullOrWhiteSpace(material?.BaseColorTextureName)) &&
                    (image is null || uv.Length != mesh.Positions.Length);
                uint[] colors = mesh.DiffuseColors;
                if (colors.Length > 0 && colors.All(value => value == colors[0]))
                {
                    uint value = colors[0];
                    color = Color.FromArgb((byte)(value >> 24), (byte)(value >> 16), (byte)(value >> 8), (byte)value);
                }
            }
            meshVisuals[index] = AddInputMesh(group, mesh.Positions, mesh.Normals,
                mesh.TriangleIndices, uv, stage, image, color);
        }
        return (group, PreviewTextureNote(missing, layered) ??
            (externalImageShown ? "Внешние текстуры — проверьте импорт" : null));
    }

    private static string? PreviewTextureNote(bool missing, bool layered) =>
        missing ? "Есть недоступные текстуры" : layered ? "Показаны основные текстуры" : null;

    private static NativePreviewSurface ResolveNativePreviewSurface(
        SmoLoadedMaterial? material, SmoMaterialDraw? draw, Vector4 color, bool blend)
    {
        SmoMaterialDrawPass? pass = draw?.Passes.FirstOrDefault(value =>
            value.Stages.Any(stage => stage.Texture is not null && stage.ColorOperation != 1 && stage.Coordinates <= 1));
        SmoMaterialTextureStage? stage = pass?.Stages.FirstOrDefault(value =>
            value.Texture is not null && value.ColorOperation != 1 && value.Coordinates <= 1);
        var layers = material?.Passes.SelectMany(value => value.Layers).ToArray() ?? [];
        SmoLoadedTexture? reference = stage?.Texture ?? layers.Select(layer =>
            layer.Texture ?? layer.Animation?.Keys.Select(key => key.Texture).FirstOrDefault(texture => texture is not null))
            .FirstOrDefault(texture => texture is not null);
        var raster = (pass ?? draw?.Passes.FirstOrDefault())?.Raster;
        var alpha = new PreviewAlpha(raster is null ? blend : raster.BlendEnable != 0,
            raster is { AlphaTest: not 0 } ? raster.AlphaFunction : 0,
            (byte)(raster?.AlphaReference ?? 0));
        if (material?.Colors.Count > 1 && color == Vector4.One)
            color = material.Colors[1];
        static byte Channel(float value) => float.IsFinite(value)
            ? (byte)MathF.Round(Math.Clamp(value, 0, 1) * 255) : (byte)255;
        return new NativePreviewSurface(reference, stage, alpha,
            Color.FromArgb(Channel(color.W), Channel(color.X), Channel(color.Y), Channel(color.Z)),
            layers.Count(layer => layer.Texture is not null || layer.Animation is not null) > 1);
    }

    private static Material? ResolveNativePreviewImage(NativePreviewSurface surface, SmoExportTexture? exported,
        SmoDocument? sourceDocument, Dictionary<PreviewImageKey, Material?> images)
    {
        int? index = surface.Reference?.ObjectIndex ?? exported?.ObjectIndex;
        if (index is null)
            return null;
        var key = new PreviewImageKey(index.Value, surface.Alpha);
        return GetInputImage(images, key, () =>
        {
            SmoTexture? texture = surface.Reference?.Texture;
            if (texture is not null)
                return SmoThumbnailRenderer.CreateTextureThumbnail(texture.Width, texture.Height,
                    texture.Bgra32Pixels, MaximumPreviewTextureSide, MaximumPreviewTextureSide);
            if (exported?.ObjectIndex == index && !exported.Bgra32Pixels.IsEmpty)
                return SmoThumbnailRenderer.CreateTextureThumbnail(exported.Width, exported.Height,
                    exported.Bgra32Pixels, MaximumPreviewTextureSide, MaximumPreviewTextureSide);
            if (sourceDocument is null || (uint)index.Value >= (uint)sourceDocument.Objects.Count)
                return null;

            // Read only the texture already bound to this material. Never choose
            // another container or bind a different image based on a similar name.
            var entry = sourceDocument.Objects[index.Value];
            if (SmoTextureDecoder.TryDecode(sourceDocument, entry, out texture, out string issue))
                return SmoThumbnailRenderer.CreateTextureThumbnail(texture.Width, texture.Height,
                    texture.Bgra32Pixels, MaximumPreviewTextureSide, MaximumPreviewTextureSide);
            SessionLog.Info("INPUT_TEXTURE", issue);
            return TryReadAdjacentInputImage(sourceDocument.SourcePath, entry.Name);
        });
    }

    private static BitmapSource? TryReadAdjacentInputImage(string? modelPath, string name)
    {
        string? directory = Path.GetDirectoryName(modelPath);
        string fileName = Path.GetFileName(name.Replace('\\', '/'));
        if (directory is null || string.IsNullOrWhiteSpace(fileName))
            return null;
        string[] extensions = [".png", ".tga", ".bmp", ".jpg", ".jpeg"];
        string extension = Path.GetExtension(fileName);
        IEnumerable<string> names = extension.Length == 0 ? extensions.Select(value => fileName + value) :
            extensions.Contains(extension, StringComparer.OrdinalIgnoreCase) ? [fileName] : [];
        string[] candidates = names.SelectMany(value => new[]
            { Path.Combine(directory, value), Path.Combine(directory, "textures", value) })
            .Where(File.Exists).Distinct(StringComparer.OrdinalIgnoreCase).Take(2).ToArray();
        return candidates.Length == 1 ? ReadPreviewBitmap(ImportedTextureFileReader.Read(candidates[0])) : null;
    }

    private static Material? ResolveExternalPreviewImage(ImportedScene scene, ImportedMaterial? material,
        string? donorPath, Dictionary<PreviewImageKey, Material?> images)
    {
        if (material is null)
            return null;
        int index = material.BaseColorTextureIndex;
        byte cutoff = (byte)MathF.Ceiling(Math.Clamp(material.AlphaCutoff, 0, 1) * 255);
        var alpha = material.AlphaMode switch
        {
            ImportedMaterialAlphaMode.Mask => new PreviewAlpha(false, 7, cutoff),
            ImportedMaterialAlphaMode.Blend => new PreviewAlpha(true),
            _ => default
        };
        if ((uint)index < (uint)scene.Textures.Count)
            return GetInputImage(images, new PreviewImageKey(index, alpha), () => ReadPreviewBitmap(scene.Textures[index]));
        return material.PreviewTextureUri is { Length: > 0 } uri
            ? GetInputImage(images, new PreviewImageKey(-1, alpha, uri), () => TryReadLinkedInputImage(donorPath, uri))
            : null;
    }

    private static BitmapSource? TryReadLinkedInputImage(string? donorPath, string uri)
    {
        string? directory = Path.GetDirectoryName(donorPath);
        if (directory is null || uri.IndexOfAny([':', '?', '#', '\\']) >= 0 ||
            !Uri.IsWellFormedUriString(uri, UriKind.Relative))
            return null;

        string relative = Uri.UnescapeDataString(uri);
        if (Path.IsPathRooted(relative) || relative.Contains(':'))
            return null;
        string path = Path.GetFullPath(Path.Combine(directory, relative));
        string prefix = Path.TrimEndingDirectorySeparator(directory) + Path.DirectorySeparatorChar;
        return path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) && File.Exists(path)
            ? ReadPreviewBitmap(ImportedTextureFileReader.Read(path)) : null;
    }

    private static Material? GetInputImage(Dictionary<PreviewImageKey, Material?> images, PreviewImageKey key,
        Func<BitmapSource?> create)
    {
        if (images.TryGetValue(key, out Material? material))
            return material;
        try
        {
            if (create() is { } bitmap)
                material = CreatePreviewImageMaterial(ApplyInputAlpha(bitmap, key.Alpha));
        }
        catch (Exception exception) when (exception is IOException or InvalidDataException or UnauthorizedAccessException or
            ArgumentException or InvalidOperationException or NotSupportedException or OverflowException)
        {
            SessionLog.Error("INPUT_TEXTURE", exception);
        }
        images.Add(key, material);
        return material;
    }

    private static BitmapSource ApplyInputAlpha(BitmapSource bitmap, PreviewAlpha alpha)
    {
        if (alpha.Blend && alpha.Test == 0)
            return bitmap;
        int stride = checked(bitmap.PixelWidth * 4);
        byte[] pixels = new byte[checked(stride * bitmap.PixelHeight)];
        bitmap.CopyPixels(pixels, stride, 0);
        for (int i = 3; i < pixels.Length; i += 4)
        {
            byte value = pixels[i];
            bool visible = alpha.Test switch
            {
                1 => false, 2 => value < alpha.Reference, 3 => value == alpha.Reference,
                4 => value <= alpha.Reference, 5 => value > alpha.Reference,
                6 => value != alpha.Reference, 7 => value >= alpha.Reference, _ => true
            };
            pixels[i] = visible ? alpha.Blend ? value : (byte)255 : (byte)0;
        }
        var adjusted = BitmapSource.Create(bitmap.PixelWidth, bitmap.PixelHeight, 96, 96,
            PixelFormats.Bgra32, null, pixels, stride);
        adjusted.Freeze();
        return adjusted;
    }

    private static GeometryModel3D? AddInputMesh(Model3DGroup group, Vector3[] positions, Vector3[] normals,
        uint[] indices, Vector2[] uv, SmoMaterialTextureStage? stage, Material? image, Color color)
    {
        if (positions.Length == 0 || indices.Length < 3)
            return null;
        var model = AddModel(group, positions, indices, color, all: null);
        var geometry = (MeshGeometry3D)model.Geometry;
        if (normals.Length == positions.Length)
            geometry.Normals = new Vector3DCollection(normals.Select(value => new Vector3D(value.X, value.Y, value.Z)));
        if (image is not null && uv.Length == positions.Length)
        {
            geometry.TextureCoordinates = new PointCollection(uv.Select(value => ApplyInputUv(value, stage)));
            model.Material = model.BackMaterial = image;
        }
        return model;
    }

    private static Point ApplyInputUv(Vector2 uv, SmoMaterialTextureStage? stage)
    {
        if (stage is not { TransformFlags: not 0, UVTransform: Matrix4x4 transform })
            return new Point(uv.X, uv.Y);
        Vector4 value = Vector4.Transform(new Vector4(uv, 1, 0), transform);
        float divisor = (stage.TransformFlags & 0x100) == 0 ? 1 :
            (stage.TransformFlags & 7) switch { 2 => value.Y, 3 => value.Z, _ => value.W };
        return Math.Abs(divisor) > 1e-6f && float.IsFinite(value.X / divisor) && float.IsFinite(value.Y / divisor)
            ? new Point(value.X / divisor, value.Y / divisor) : new Point(uv.X, uv.Y);
    }
}
