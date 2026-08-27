using SmoLVLcreator.Core;
using System.Globalization;
using System.Numerics;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;

namespace SmoLVLcreator.Gui;

public sealed record CollisionPreviewMesh(
    IReadOnlyList<Vector3> Positions,
    IReadOnlyList<int> TriangleIndices);

public partial class CollisionGenerationWindow : Window
{
    private readonly Vector3[] _sourcePoints;
    private readonly CollisionPreviewMesh[] _sourceMeshes;
    private Point3D _center;
    private double _distance = 10;
    private double _yaw = 0.65;
    private double _pitch = -0.35;
    private Point _lastMouse;
    private bool _orbiting;

    public CollisionGenerationWindow(
        IEnumerable<Vector3> sourcePoints,
        IEnumerable<CollisionPreviewMesh> sourceMeshes,
        int triangleBudget = SmoCollisionHullGenerator.DefaultTriangleBudget,
        float? padding = null)
    {
        _sourcePoints = sourcePoints.ToArray();
        _sourceMeshes = sourceMeshes.ToArray();
        InitializeComponent();
        TriangleBudgetBox.Text = triangleBudget.ToString(CultureInfo.CurrentCulture);
        PaddingBox.Text = (padding ?? CalculateDefaultPadding(_sourcePoints))
            .ToString("0.###", CultureInfo.CurrentCulture);
        RefreshPreview();
    }

    public SmoGeneratedCollisionMesh? Result { get; private set; }
    public int TriangleBudget { get; private set; }
    public float CollisionPadding { get; private set; }

    private void RefreshPreview_Click(object sender, RoutedEventArgs e) =>
        RefreshPreview();

    private void ParameterBox_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.Enter)
            return;
        RefreshPreview();
        e.Handled = true;
    }

    private bool RefreshPreview()
    {
        ValidationText.Text = string.Empty;
        if (!int.TryParse(
                TriangleBudgetBox.Text,
                NumberStyles.Integer,
                CultureInfo.CurrentCulture,
                out int budget) || budget is < 12 or > 256)
        {
            ValidationText.Text = "Лимит должен быть целым числом от 12 до 256.";
            ConfirmButton.IsEnabled = false;
            return false;
        }
        if (!TryParseFloat(PaddingBox.Text, out float padding) || padding < 0)
        {
            ValidationText.Text = "Отступ должен быть конечным неотрицательным числом.";
            ConfirmButton.IsEnabled = false;
            return false;
        }
        try
        {
            SmoGeneratedCollisionMesh generated = SmoCollisionHullGenerator.Generate(
                _sourcePoints,
                budget,
                padding);
            TriangleBudget = budget;
            CollisionPadding = padding;
            Result = generated;
            BuildPreview(generated);
            ResultStatsText.Text =
                $"Источник: {_sourcePoints.Length:N0} вершин\n" +
                $"Коллизия: {generated.Positions.Count:N0} вершин · " +
                $"{generated.TriangleCount:N0}/{budget:N0} треугольников\n" +
                $"Отступ: {padding:0.###}";
            ConfirmButton.IsEnabled = true;
            return true;
        }
        catch (Exception exception)
        {
            Result = null;
            ValidationText.Text = exception.Message;
            ConfirmButton.IsEnabled = false;
            return false;
        }
    }

    private void BuildPreview(SmoGeneratedCollisionMesh generated)
    {
        var group = new Model3DGroup();
        group.Children.Add(new AmbientLight(Color.FromRgb(115, 125, 138)));
        group.Children.Add(new DirectionalLight(
            Color.FromRgb(235, 238, 242),
            new Vector3D(-0.35, -0.75, -0.45)));

        var sourceMaterial = new DiffuseMaterial(
            new SolidColorBrush(Color.FromArgb(105, 86, 151, 205)));
        foreach (CollisionPreviewMesh source in _sourceMeshes)
            group.Children.Add(CreateGeometry(source, sourceMaterial));

        var collisionMaterial = new DiffuseMaterial(
            new SolidColorBrush(Color.FromArgb(145, 238, 139, 47)));
        group.Children.Add(CreateGeometry(
            new CollisionPreviewMesh(
                generated.Positions,
                generated.TriangleIndices),
            collisionMaterial));
        PreviewVisual.Content = group;
        FramePreview(generated.Positions.Concat(_sourcePoints));
    }

    private static GeometryModel3D CreateGeometry(
        CollisionPreviewMesh source,
        Material material)
    {
        var geometry = new MeshGeometry3D
        {
            Positions = new Point3DCollection(source.Positions.Select(position =>
                new Point3D(position.X, position.Y, position.Z))),
            TriangleIndices = new Int32Collection(source.TriangleIndices)
        };
        return new GeometryModel3D(geometry, material)
        {
            BackMaterial = material
        };
    }

    private void FramePreview(IEnumerable<Vector3> points)
    {
        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);
        bool found = false;
        foreach (Vector3 point in points)
        {
            minimum = Vector3.Min(minimum, point);
            maximum = Vector3.Max(maximum, point);
            found = true;
        }
        if (!found)
            return;
        Vector3 center = (minimum + maximum) * 0.5f;
        _center = new Point3D(center.X, center.Y, center.Z);
        _distance = Math.Max(Vector3.Distance(minimum, maximum) * 1.45, 0.1);
        UpdateCamera();
    }

    private void UpdateCamera()
    {
        double horizontal = Math.Cos(_pitch) * _distance;
        var offset = new Vector3D(
            Math.Sin(_yaw) * horizontal,
            Math.Sin(_pitch) * _distance,
            Math.Cos(_yaw) * horizontal);
        PreviewCamera.Position = _center + offset;
        PreviewCamera.LookDirection = _center - PreviewCamera.Position;
        PreviewCamera.UpDirection = new Vector3D(0, 1, 0);
        PreviewCamera.NearPlaneDistance = Math.Max(_distance / 10000, 0.001);
        PreviewCamera.FarPlaneDistance = Math.Max(_distance * 20, 100);
    }

    private void PreviewViewport_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton != MouseButton.Right)
            return;
        _orbiting = true;
        _lastMouse = e.GetPosition(PreviewViewport);
        PreviewViewport.CaptureMouse();
    }

    private void PreviewViewport_MouseMove(object sender, MouseEventArgs e)
    {
        if (!_orbiting || e.RightButton != MouseButtonState.Pressed)
            return;
        Point current = e.GetPosition(PreviewViewport);
        System.Windows.Vector delta = current - _lastMouse;
        _lastMouse = current;
        _yaw -= delta.X * 0.008;
        _pitch = Math.Clamp(_pitch + delta.Y * 0.008, -1.5, 1.5);
        UpdateCamera();
    }

    private void PreviewViewport_MouseUp(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton != MouseButton.Right)
            return;
        _orbiting = false;
        PreviewViewport.ReleaseMouseCapture();
    }

    private void PreviewViewport_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        _distance = Math.Clamp(
            _distance * Math.Exp(-e.Delta / 120.0 * 0.12),
            0.001,
            1_000_000);
        UpdateCamera();
        e.Handled = true;
    }

    private void Confirm_Click(object sender, RoutedEventArgs e)
    {
        if (!RefreshPreview() || Result is null)
            return;
        DialogResult = true;
    }

    private static float CalculateDefaultPadding(IReadOnlyList<Vector3> points)
    {
        if (points.Count == 0)
            return 0.05f;
        Vector3 minimum = points.Aggregate(Vector3.Min);
        Vector3 maximum = points.Aggregate(Vector3.Max);
        return MathF.Max(Vector3.Distance(minimum, maximum) * 0.005f, 0.05f);
    }

    private static bool TryParseFloat(string text, out float value) =>
        (float.TryParse(
             text,
             NumberStyles.Float,
             CultureInfo.CurrentCulture,
             out value) ||
         float.TryParse(
             text,
             NumberStyles.Float,
             CultureInfo.InvariantCulture,
             out value)) &&
        float.IsFinite(value);
}
