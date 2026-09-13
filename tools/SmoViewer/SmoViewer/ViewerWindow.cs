using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.GraphicsLibraryFramework;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;

public class ViewerWindow : GameWindow
{
    private int _vao;
    private int _vbo;
    private int _ebo;
    private int _shaderProgram;

    private ModelData? _model;

    private List<MeshPart> _meshParts = new();
    private bool[] _meshEnabled = Array.Empty<bool>();
    private int _selectedMeshIndex = 0;

    private float _yaw = 0.6f;
    private float _pitch = 0.3f;
    private float _distance = 3.0f;
    private Vector3 _target = Vector3.Zero;

    private Vector2 _lastMousePosition;
    private bool _isLeftMouseDown;
    private bool _isMiddleMouseDown;
    private bool _isRightMouseDown;

    public ViewerWindow(GameWindowSettings gameSettings, NativeWindowSettings nativeSettings)
        : base(gameSettings, nativeSettings)
    {
    }

    protected override void OnLoad()
    {
        base.OnLoad();

        GL.ClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        GL.Enable(EnableCap.DepthTest);

        _vao = GL.GenVertexArray();
        _vbo = GL.GenBuffer();
        _ebo = GL.GenBuffer();

        GL.BindVertexArray(_vao);
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vbo);
        GL.BindBuffer(BufferTarget.ElementArrayBuffer, _ebo);

        GL.VertexAttribPointer(0, 3, VertexAttribPointerType.Float, false, 3 * sizeof(float), 0);
        GL.EnableVertexAttribArray(0);

        string vertexShaderSource = @"
#version 330 core
layout (location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}";
        string fragmentShaderSource = @"
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(0.8, 0.85, 1.0, 1.0);
}";
        _shaderProgram = ShaderHelper.CreateProgram(vertexShaderSource, fragmentShaderSource);

        string smoPath = Path.Combine(AppContext.BaseDirectory, "Assets", "bloom_ball.smo");

        _meshParts = SmoLoader.LoadMeshPartsFromFile(smoPath);

        _meshEnabled = new bool[_meshParts.Count];
        if (_meshParts.Count > 0)
        {
            _selectedMeshIndex = 0;
            _meshEnabled[0] = true; // по умолчанию показываем только первый меш
        }

        PrintMeshListToConsole();
        RebuildVisibleModel();
        UpdateWindowTitle();
    }

    protected override void OnUpdateFrame(FrameEventArgs args)
    {
        base.OnUpdateFrame(args);

        if (!IsFocused)
            return;

        if (KeyboardState.IsKeyDown(Keys.Escape))
            Close();

        HandleMeshHotkeys();
    }

    private void HandleMeshHotkeys()
    {
        if (_meshParts.Count == 0)
            return;

        bool changedSelection = false;
        bool changedVisible = false;

        if (KeyboardState.IsKeyPressed(Keys.Down))
        {
            _selectedMeshIndex++;
            if (_selectedMeshIndex >= _meshParts.Count)
                _selectedMeshIndex = 0;
            changedSelection = true;
        }

        if (KeyboardState.IsKeyPressed(Keys.Up))
        {
            _selectedMeshIndex--;
            if (_selectedMeshIndex < 0)
                _selectedMeshIndex = _meshParts.Count - 1;
            changedSelection = true;
        }

        if (KeyboardState.IsKeyPressed(Keys.Space))
        {
            _meshEnabled[_selectedMeshIndex] = !_meshEnabled[_selectedMeshIndex];
            changedVisible = true;
        }

        if (KeyboardState.IsKeyPressed(Keys.Enter))
        {
            for (int i = 0; i < _meshEnabled.Length; i++)
                _meshEnabled[i] = false;

            _meshEnabled[_selectedMeshIndex] = true;
            changedVisible = true;
        }

        if (KeyboardState.IsKeyPressed(Keys.A))
        {
            for (int i = 0; i < _meshEnabled.Length; i++)
                _meshEnabled[i] = true;

            changedVisible = true;
        }

        if (KeyboardState.IsKeyPressed(Keys.C))
        {
            for (int i = 0; i < _meshEnabled.Length; i++)
                _meshEnabled[i] = false;

            changedVisible = true;
        }

        if (changedSelection || changedVisible)
        {
            PrintMeshListToConsole();

            if (changedVisible)
                RebuildVisibleModel();

            UpdateWindowTitle();
        }
    }

    private void RebuildVisibleModel()
    {
        List<MeshPart> selected = new();

        for (int i = 0; i < _meshParts.Count; i++)
        {
            if (_meshEnabled[i])
                selected.Add(_meshParts[i]);
        }

        if (selected.Count == 0)
        {
            _model = new ModelData
            {
                Vertices = Array.Empty<float>(),
                Indices = Array.Empty<uint>()
            };
        }
        else
        {
            _model = SmoLoader.BuildCombinedModel(selected);
        }

        UploadModelToGpu();
    }

    private void UploadModelToGpu()
    {
        if (_model == null)
            return;

        GL.BindVertexArray(_vao);

        GL.BindBuffer(BufferTarget.ArrayBuffer, _vbo);
        GL.BufferData(
            BufferTarget.ArrayBuffer,
            _model.Vertices.Length * sizeof(float),
            _model.Vertices,
            BufferUsageHint.StaticDraw);

        GL.BindBuffer(BufferTarget.ElementArrayBuffer, _ebo);
        GL.BufferData(
            BufferTarget.ElementArrayBuffer,
            _model.Indices.Length * sizeof(uint),
            _model.Indices,
            BufferUsageHint.StaticDraw);
    }

    private void PrintMeshListToConsole()
    {
        Console.Clear();
        Console.WriteLine("========================================");
        Console.WriteLine("MESH LIST");
        Console.WriteLine("========================================");
        Console.WriteLine("Up/Down  - выбрать меш");
        Console.WriteLine("Space    - включить/выключить текущий");
        Console.WriteLine("Enter    - оставить только текущий");
        Console.WriteLine("A        - включить все");
        Console.WriteLine("C        - выключить все");
        Console.WriteLine("Esc      - выход");
        Console.WriteLine();

        for (int i = 0; i < _meshParts.Count; i++)
        {
            string cursor = (i == _selectedMeshIndex) ? ">" : " ";
            string flag = _meshEnabled[i] ? "[x]" : "[ ]";

            Console.WriteLine($"{cursor} {flag} {i:D2}  {_meshParts[i].Name}  (V:{_meshParts[i].VertexCount}, I:{_meshParts[i].IndexCount})");
        }

        Console.WriteLine();
    }

    private void UpdateWindowTitle()
    {
        int enabledCount = _meshEnabled.Count(x => x);
        string currentName = _meshParts.Count > 0 ? _meshParts[_selectedMeshIndex].Name : "none";

        Title = $"SMO Viewer | Selected: {_selectedMeshIndex + 1}/{_meshParts.Count} | Enabled: {enabledCount} | {currentName}";
    }

    protected override void OnMouseDown(MouseButtonEventArgs e)
    {
        base.OnMouseDown(e);

        if (e.Button == MouseButton.Left) _isLeftMouseDown = true;
        if (e.Button == MouseButton.Middle) _isMiddleMouseDown = true;
        if (e.Button == MouseButton.Right) _isRightMouseDown = true;

        _lastMousePosition = MouseState.Position;
    }

    protected override void OnMouseUp(MouseButtonEventArgs e)
    {
        base.OnMouseUp(e);

        if (e.Button == MouseButton.Left) _isLeftMouseDown = false;
        if (e.Button == MouseButton.Middle) _isMiddleMouseDown = false;
        if (e.Button == MouseButton.Right) _isRightMouseDown = false;
    }

    protected override void OnMouseMove(MouseMoveEventArgs e)
    {
        base.OnMouseMove(e);

        Vector2 currentPosition = e.Position;
        Vector2 delta = currentPosition - _lastMousePosition;
        _lastMousePosition = currentPosition;

        const float rotationSensitivity = 0.01f;
        const float zoomDragSensitivity = 0.01f;
        const float panSensitivity = 0.005f;

        if (_isLeftMouseDown)
        {
            _yaw -= delta.X * rotationSensitivity;
            _pitch += delta.Y * rotationSensitivity;
        }

        if (_isMiddleMouseDown)
        {
            _distance += delta.Y * zoomDragSensitivity;
        }

        if (_isRightMouseDown)
        {
            Vector3 forward = Vector3.Normalize(_target - GetCameraPosition());
            Vector3 right = Vector3.Normalize(Vector3.Cross(forward, Vector3.UnitY));
            Vector3 up = Vector3.Normalize(Vector3.Cross(right, forward));

            _target -= right * delta.X * panSensitivity * _distance;
            _target += up * delta.Y * panSensitivity * _distance;
        }

        _pitch = MathHelper.Clamp(_pitch, -1.5f, 1.5f);
        _distance = MathHelper.Clamp(_distance, 0.5f, 50.0f);
    }

    protected override void OnMouseWheel(MouseWheelEventArgs e)
    {
        base.OnMouseWheel(e);

        const float wheelZoomSensitivity = 0.25f;
        _distance -= e.OffsetY * wheelZoomSensitivity;
        _distance = MathHelper.Clamp(_distance, 0.5f, 50.0f);
    }

    protected override void OnRenderFrame(FrameEventArgs args)
    {
        base.OnRenderFrame(args);

        GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);

        if (_model == null || _model.Indices.Length == 0)
        {
            SwapBuffers();
            return;
        }

        GL.UseProgram(_shaderProgram);
        GL.BindVertexArray(_vao);

        Matrix4 model = Matrix4.Identity;
        Vector3 cameraPosition = GetCameraPosition();
        Matrix4 view = Matrix4.LookAt(cameraPosition, _target, Vector3.UnitY);
        Matrix4 projection = Matrix4.CreatePerspectiveFieldOfView(
            MathHelper.DegreesToRadians(60f),
            Size.X / (float)Size.Y,
            0.1f,
            100f);

        int modelLoc = GL.GetUniformLocation(_shaderProgram, "uModel");
        int viewLoc = GL.GetUniformLocation(_shaderProgram, "uView");
        int projLoc = GL.GetUniformLocation(_shaderProgram, "uProjection");

        GL.UniformMatrix4(modelLoc, false, ref model);
        GL.UniformMatrix4(viewLoc, false, ref view);
        GL.UniformMatrix4(projLoc, false, ref projection);

        GL.DrawElements(
            PrimitiveType.Triangles,
            _model.Indices.Length,
            DrawElementsType.UnsignedInt,
            0);

        SwapBuffers();
    }

    protected override void OnUnload()
    {
        base.OnUnload();

        GL.DeleteBuffer(_vbo);
        GL.DeleteBuffer(_ebo);
        GL.DeleteVertexArray(_vao);
        GL.DeleteProgram(_shaderProgram);
    }

    private Vector3 GetCameraPosition()
    {
        Vector3 orbit = new Vector3(
            MathF.Cos(_pitch) * MathF.Sin(_yaw),
            MathF.Sin(_pitch),
            MathF.Cos(_pitch) * MathF.Cos(_yaw)
        ) * _distance;

        return _target + orbit;
    }
}