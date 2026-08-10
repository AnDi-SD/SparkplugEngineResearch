using System.Globalization;
using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using Microsoft.Win32;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Gui;

public partial class MainWindow : Window
{
    private string? _sourcePath;
    private SmoDocument? _document;
    private SmoExportScene? _sourceScene;
    private ImportedScene? _replacementScene;
    private MeshSplitPlan? _plan;
    private string? _texturePath;

    public MainWindow() => InitializeComponent();

    private void SelectSource_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog { Filter = "Sparkplug model (*.smo)|*.smo", CheckFileExists = true };
        if (dialog.ShowDialog(this) != true) return;
        try
        {
            _sourcePath = dialog.FileName;
            _document = SmoDocument.Load(_sourcePath);
            _sourceScene = SmoSceneBuilder.Build(_document);
            _plan = null;
            SourcePathText.Text = _sourcePath;
            SourceSummaryText.Text = $"{_sourceScene.Meshes.Count} mesh slots; {_sourceScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; {_sourceScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles.";
            BoneCombo.ItemsSource = SmoWholeModelReplacer.GetRigidBoneChoices(_document)
                .Select(bone => new BoneItem(bone.Slot, $"[{bone.Slot}] {bone.Name}"))
                .ToArray();
            BoneCombo.SelectedIndex = BoneCombo.Items.Count > 0 ? 0 : -1;
            StatusText.Text = "Шаблон SMO загружен.";
            RefreshState();
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private void SelectReplacement_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "Модель замены (*.glb;*.obj)|*.glb;*.obj|GLB (*.glb)|*.glb|OBJ (*.obj)|*.obj",
            CheckFileExists = true
        };
        if (dialog.ShowDialog(this) != true) return;
        try
        {
            _replacementScene = ImportedModelReader.Read(dialog.FileName);
            _plan = null;
            ReplacementPathText.Text = dialog.FileName;
            ReplacementSummaryText.Text = $"{_replacementScene.Meshes.Count} source meshes; {_replacementScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; {_replacementScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles.";
            EmbeddedTextureCombo.ItemsSource = new[]
                {
                    new TextureItem(-1, "Не менять текстуру исходного SMO")
                }
                .Concat(_replacementScene.Textures.Select((texture, index) => new TextureItem(index,
                    $"{texture.Name} — {texture.Width}×{texture.Height}, {texture.MimeType}")))
                .ToArray();
            EmbeddedTextureCombo.SelectedIndex = 0;
            _texturePath = null;
            TexturePathText.Text = _replacementScene.Textures.Count > 0
                ? $"Встроенных base-color текстур: {_replacementScene.Textures.Count}; выберите нужную или оставьте исходную."
                : "Встроенная base-color текстура не найдена.";
            StatusText.Text = "Модель замены загружена. Постройте план нарезки.";
            RefreshState();
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private void Plan_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null || _replacementScene is null) return;
        try
        {
            int slots = _document.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData);
            ImportedMesh combined = ImportedMeshCombiner.Combine(_replacementScene);
            _plan = MeshSplitter.Split(_replacementScene);
            if (_plan.Chunks.Count != 1)
                throw new InvalidOperationException("Модель превышает 65 535 уникальных вершин и пока требует умного пространственного разбиения.");
            PlanSummaryText.Text = $"{combined.Positions.Length:N0} vertices и {combined.TriangleIndices.Length / 3:N0} triangles → " +
                $"1 цельный rigid body-slot; ещё {slots - 1} slots получат невидимый degenerate triangle.";
            StatusText.Text = "План проверен. Можно создать экспериментальный SMO.";
            RefreshState();
        }
        catch (Exception exception) { _plan = null; RefreshState(); ShowError(exception); }
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null || _sourcePath is null || _replacementScene is null || _plan is null) return;
        var dialog = new SaveFileDialog
        {
            Filter = "Sparkplug model (*.smo)|*.smo",
            FileName = Path.GetFileNameWithoutExtension(_sourcePath) + "_whole_replaced.smo",
            InitialDirectory = Path.GetDirectoryName(_sourcePath)
        };
        if (dialog.ShowDialog(this) != true) return;
        try
        {
            WholeModelReplacementResult result = SmoWholeModelReplacer.Replace(
                _document, _replacementScene, ReadTransform(), dialog.FileName,
                BoneCombo.SelectedItem is BoneItem bone ? bone.Slot : 0,
                texturePath: _texturePath,
                embeddedTexture: EmbeddedTextureCombo.SelectedItem is TextureItem texture && texture.Index >= 0
                    ? _replacementScene.Textures[texture.Index]
                    : null);
            StatusText.Text = $"Готово: {result.MeshCount} meshes, {result.VertexCount:N0} vertices, {result.TriangleCount:N0} triangles.";
            MessageBox.Show(this,
                $"Новая копия сохранена и проверена strict parser:\n{result.OutputPath}\n\nТекстура записана безопасно: RGB заменён, исходный Alpha и структура SMO сохранены.",
                "Экспериментальный SMO готов", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private void SelectTexture_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "Изображение (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg|PNG (*.png)|*.png|JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg",
            CheckFileExists = true
        };
        if (dialog.ShowDialog(this) != true) return;
        _texturePath = dialog.FileName;
        EmbeddedTextureCombo.SelectedIndex = -1;
        TexturePathText.Text = _texturePath;
        StatusText.Text = "Текстура будет записана в основной character atlas при сохранении.";
    }

    private void EmbeddedTexture_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (EmbeddedTextureCombo.SelectedItem is not TextureItem texture || _replacementScene is null)
            return;
        _texturePath = null;
        TexturePathText.Text = texture.Index < 0
            ? "Текстура исходного SMO останется без изменений."
            : $"Встроена в GLB: {_replacementScene.Textures[texture.Index].Name}";
    }

    private void AutoFit_Click(object sender, RoutedEventArgs e)
    {
        if (_sourceScene is null || _replacementScene is null) return;
        try
        {
            Vector3[] source = _sourceScene.Meshes.SelectMany(mesh => mesh.Positions).ToArray();
            Vector3[] replacement = _replacementScene.Meshes.SelectMany(mesh => mesh.Positions).ToArray();
            if (source.Length == 0 || replacement.Length == 0)
                throw new InvalidOperationException("Одна из моделей не содержит вершин.");

            (Vector3 sourceMin, Vector3 sourceMax) = Bounds(source);
            (Vector3 replacementMin, Vector3 replacementMax) = Bounds(replacement);
            Vector3 sourceSize = sourceMax - sourceMin;
            Vector3 replacementSize = replacementMax - replacementMin;
            float sourceHeight = sourceSize.Y > 0.000001f ? sourceSize.Y : sourceSize.Length();
            float replacementHeight = replacementSize.Y > 0.000001f ? replacementSize.Y : replacementSize.Length();
            if (replacementHeight <= 0.000001f)
                throw new InvalidOperationException("Невозможно определить размер модели замены.");

            float scale = sourceHeight / replacementHeight;
            Vector3 sourceCenter = (sourceMin + sourceMax) * 0.5f;
            Vector3 replacementCenter = (replacementMin + replacementMax) * 0.5f;
            Vector3 translation = sourceCenter - replacementCenter * scale;
            ScaleBox.Text = scale.ToString("G9", CultureInfo.InvariantCulture);
            RotXBox.Text = RotYBox.Text = RotZBox.Text = "0";
            MoveXBox.Text = translation.X.ToString("G9", CultureInfo.InvariantCulture);
            MoveYBox.Text = translation.Y.ToString("G9", CultureInfo.InvariantCulture);
            MoveZBox.Text = translation.Z.ToString("G9", CultureInfo.InvariantCulture);
            StatusText.Text = $"Автоподгонка: scale {scale:G5}; центры моделей совмещены.";
            RefreshPreview();
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private void Transform_Changed(object sender, TextChangedEventArgs e) => RefreshPreview();

    private void Reset_Click(object sender, RoutedEventArgs e)
    {
        _sourcePath = null; _document = null; _sourceScene = null; _replacementScene = null; _plan = null; _texturePath = null;
        SourcePathText.Text = "Не выбран"; SourceSummaryText.Text = "—";
        ReplacementPathText.Text = "Не выбрана"; ReplacementSummaryText.Text = "—";
        TexturePathText.Text = "Остаётся текстура исходного SMO"; BoneCombo.ItemsSource = null;
        EmbeddedTextureCombo.ItemsSource = null;
        PlanSummaryText.Text = "План ещё не построен.";
        ScaleBox.Text = "1"; RotXBox.Text = RotYBox.Text = RotZBox.Text = "0";
        MoveXBox.Text = MoveYBox.Text = MoveZBox.Text = "0";
        StatusText.Text = "Выберите исходный SMO.";
        RefreshState();
    }

    private void RefreshState()
    {
        PlanButton.IsEnabled = _document is not null && _replacementScene is not null;
        AutoFitButton.IsEnabled = _sourceScene is not null && _replacementScene is not null;
        SaveButton.IsEnabled = _plan is not null;
        RefreshPreview();
    }

    private void RefreshPreview()
    {
        if (SceneVisual is null) return;
        var group = new Model3DGroup();
        var all = new List<Point3D>();
        if (_sourceScene is not null)
            foreach (SmoExportMesh mesh in _sourceScene.Meshes)
                AddModel(group, mesh.Positions, mesh.TriangleIndices, Color.FromArgb(75, 170, 180, 190), all);
        if (_replacementScene is not null)
        {
            Matrix4x4 transform = ReadTransform(false).Matrix;
            foreach (ImportedMesh mesh in _replacementScene.Meshes)
                AddModel(group, mesh.Positions.Select(value => Vector3.Transform(value, transform)).ToArray(),
                    mesh.TriangleIndices, Color.FromArgb(190, 255, 125, 35), all);
        }
        SceneVisual.Content = group;
        if (all.Count > 0) Frame(all);
    }

    private static void AddModel(Model3DGroup group, Vector3[] positions, uint[] indices, Color color, List<Point3D> all)
    {
        var points = new Point3DCollection(positions.Length);
        foreach (Vector3 value in positions) { var point = new Point3D(value.X, value.Y, value.Z); points.Add(point); all.Add(point); }
        var triangles = new Int32Collection(indices.Length);
        foreach (uint index in indices) triangles.Add(checked((int)index));
        var geometry = new MeshGeometry3D { Positions = points, TriangleIndices = triangles };
        var material = new DiffuseMaterial(new SolidColorBrush(color));
        group.Children.Add(new GeometryModel3D(geometry, material) { BackMaterial = material });
    }

    private void Frame(IReadOnlyList<Point3D> points)
    {
        double minX=points.Min(p=>p.X), maxX=points.Max(p=>p.X), minY=points.Min(p=>p.Y), maxY=points.Max(p=>p.Y), minZ=points.Min(p=>p.Z), maxZ=points.Max(p=>p.Z);
        var center = new Point3D((minX+maxX)/2,(minY+maxY)/2,(minZ+maxZ)/2);
        double size = Math.Max(1, Math.Max(maxX-minX, Math.Max(maxY-minY,maxZ-minZ)));
        Camera.Position = new Point3D(center.X, center.Y, center.Z + size * 2.5);
        Camera.LookDirection = new Vector3D(0, 0, -size * 2.5);
    }

    private static (Vector3 Min, Vector3 Max) Bounds(IEnumerable<Vector3> positions)
    {
        using IEnumerator<Vector3> iterator = positions.GetEnumerator();
        if (!iterator.MoveNext()) throw new InvalidOperationException("Модель не содержит вершин.");
        Vector3 min = iterator.Current, max = iterator.Current;
        while (iterator.MoveNext())
        {
            min = Vector3.Min(min, iterator.Current);
            max = Vector3.Max(max, iterator.Current);
        }
        return (min, max);
    }

    private ReplacementTransform ReadTransform(bool strict = true)
    {
        bool Try(TextBox box, out float value) => float.TryParse(box.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out value) || float.TryParse(box.Text, out value);
        float s=0,rx=0,ry=0,rz=0,x=0,y=0,z=0;
        bool valid = Try(ScaleBox,out s)&Try(RotXBox,out rx)&Try(RotYBox,out ry)&Try(RotZBox,out rz)&Try(MoveXBox,out x)&Try(MoveYBox,out y)&Try(MoveZBox,out z)&&s>0;
        if (!valid && strict) throw new InvalidOperationException("Проверьте transform: scale должен быть больше нуля, остальные поля должны содержать числа.");
        return valid ? new ReplacementTransform(s,new Vector3(rx,ry,rz),new Vector3(x,y,z)) : ReplacementTransform.Identity;
    }

    private void ShowError(Exception exception)
    {
        StatusText.Text = "Ошибка: " + exception.Message;
        MessageBox.Show(this, exception.Message, "Ошибка", MessageBoxButton.OK, MessageBoxImage.Error);
    }

    private sealed record BoneItem(int Slot, string Display);
    private sealed record TextureItem(int Index, string Display);
}
