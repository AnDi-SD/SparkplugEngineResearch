using System.Numerics;
using OpenTK.Graphics.OpenGL4;

namespace SmoViewer.Rendering.Wpf;

public sealed partial class SmoGpuSceneRenderer
{
    private const int MaximumPickingBufferBytes = 16 * 1024 * 1024;
    private int _pickingProgram;
    private int _pickingBuffer;
    private int _pickingBufferBytes;
    private int _pickingModelLocation;
    private int _pickingViewLocation;
    private int _pickingProjectionLocation;
    private int _pickingSkinnedLocation;
    private int _pickingBonesLocation;

    /// <summary>
    /// Reads every uploaded vertex in local space through the production vertex
    /// shader and the selected placement's current bone palette. Call after
    /// Render on the same current GL context, outside another feedback capture.
    /// Model, camera, turntable and editor coordinate mapping are not applied.
    /// </summary>
    public Vector3[] ReadPositionsForPicking(SmoRenderObjectKey key, int meshObjectIndex)
    {
        GpuRenderItem? selected = null;
        foreach (GpuRenderItem item in _items)
        {
            if (item.Key != key || item.GeometryKey.ObjectIndex != meshObjectIndex)
                continue;
            if (selected is not null)
                throw new InvalidOperationException(
                    $"Picking geometry is ambiguous for {key}, mesh {meshObjectIndex}.");
            selected = item;
        }
        if (selected is null)
            throw new KeyNotFoundException(
                $"Picking geometry is missing for {key}, mesh {meshObjectIndex}.");
        if (!_initialized || _resetRequested ||
            !_geometry.TryGetValue(selected.GeometryKey, out GpuGeometry? geometry))
            throw new InvalidOperationException("Picking requires the scene's completed Render/upload.");

        long requestedBytes = (long)geometry.VertexCount * 4 * sizeof(float);
        if (requestedBytes > MaximumPickingBufferBytes)
            throw new InvalidOperationException("Picking vertex capture exceeds the 16 MiB buffer limit.");
        if (geometry.VertexCount == 0)
            return [];
        int byteCount = checked((int)requestedBytes);

        GL.GetInteger(GetPName.CurrentProgram, out int previousProgram);
        GL.GetInteger(GetPName.VertexArrayBinding, out int previousVertexArray);
        GL.GetInteger(GetPName.TransformFeedbackBufferBinding, out int previousBuffer);
        GL.GetInteger(GetIndexedPName.TransformFeedbackBufferBinding, 0, out int previousIndexedBuffer);
        GL.GetInteger64(GetIndexedPName.TransformFeedbackBufferStart, 0, out long previousStart);
        GL.GetInteger64(GetIndexedPName.TransformFeedbackBufferSize, 0, out long previousSize);
        bool previousDiscard = GL.IsEnabled(EnableCap.RasterizerDiscard);
        bool capturing = false;
        try
        {
            EnsurePickingProgram();
            if (_pickingBuffer == 0)
                _pickingBuffer = GL.GenBuffer();
            GL.BindBuffer(BufferTarget.TransformFeedbackBuffer, _pickingBuffer);
            if (_pickingBufferBytes < byteCount)
            {
                GL.BufferData(BufferTarget.TransformFeedbackBuffer, byteCount,
                    IntPtr.Zero, BufferUsageHint.StreamRead);
                GL.GetBufferParameter(BufferTarget.TransformFeedbackBuffer,
                    BufferParameterName.BufferSize, out int allocatedBytes);
                if (allocatedBytes != byteCount)
                    throw new InvalidOperationException("OpenGL could not allocate the picking capture buffer.");
                _pickingBufferBytes = byteCount;
            }
            GL.BindBufferBase(BufferRangeTarget.TransformFeedbackBuffer, 0, _pickingBuffer);
            GL.UseProgram(_pickingProgram);
            SetMatrix(_pickingModelLocation, Matrix4x4.Identity);
            SetMatrix(_pickingViewLocation, Matrix4x4.Identity);
            SetMatrix(_pickingProjectionLocation, Matrix4x4.Identity);
            bool skinned = selected.BoneMatrices.Length > 0;
            GL.Uniform1(_pickingSkinnedLocation, skinned ? 1 : 0);
            if (skinned)
                SetBoneMatrices(_pickingBonesLocation, selected.BoneMatrices);
            GL.BindVertexArray(geometry.VertexArray);
            GL.Enable(EnableCap.RasterizerDiscard);
            GL.BeginTransformFeedback(TransformFeedbackPrimitiveType.Points);
            capturing = true;
            GL.DrawArrays(PrimitiveType.Points, 0, geometry.VertexCount);
            GL.EndTransformFeedback();
            capturing = false;

            var captured = new float[geometry.VertexCount * 4];
            GL.GetBufferSubData(BufferTarget.TransformFeedbackBuffer, IntPtr.Zero, byteCount, captured);
            var positions = new Vector3[geometry.VertexCount];
            for (int index = 0; index < positions.Length; ++index)
                positions[index] = new Vector3(captured[4 * index], captured[4 * index + 1], captured[4 * index + 2]);
            return positions;
        }
        finally
        {
            if (capturing)
                GL.EndTransformFeedback();
            if (previousIndexedBuffer != 0 && previousSize > 0)
                GL.BindBufferRange(BufferRangeTarget.TransformFeedbackBuffer, 0, previousIndexedBuffer,
                    new IntPtr(previousStart), new IntPtr(previousSize));
            else
                GL.BindBufferBase(BufferRangeTarget.TransformFeedbackBuffer, 0, previousIndexedBuffer);
            // Indexed binding calls also change the generic target binding.
            GL.BindBuffer(BufferTarget.TransformFeedbackBuffer, previousBuffer);
            GL.BindVertexArray(previousVertexArray);
            GL.UseProgram(previousProgram);
            if (!previousDiscard)
                GL.Disable(EnableCap.RasterizerDiscard);
        }
    }

    private void EnsurePickingProgram()
    {
        if (_pickingProgram != 0)
            return;
        int shader = CompileShader(ShaderType.VertexShader, VertexShaderSource);
        int program = GL.CreateProgram();
        try
        {
            GL.AttachShader(program, shader);
            GL.TransformFeedbackVaryings(program, 1, ["gl_Position"], TransformFeedbackMode.InterleavedAttribs);
            GL.LinkProgram(program);
            GL.GetProgram(program, GetProgramParameterName.LinkStatus, out int linked);
            if (linked == 0)
                throw new InvalidOperationException("OpenGL picking program link failed: " + GL.GetProgramInfoLog(program));
            _pickingModelLocation = GL.GetUniformLocation(program, "uModel");
            _pickingViewLocation = GL.GetUniformLocation(program, "uView");
            _pickingProjectionLocation = GL.GetUniformLocation(program, "uProjection");
            _pickingSkinnedLocation = GL.GetUniformLocation(program, "uSkinned");
            _pickingBonesLocation = GL.GetUniformLocation(program, "uBones[0]");
            _pickingProgram = program;
        }
        finally
        {
            GL.DetachShader(program, shader);
            GL.DeleteShader(shader);
            if (_pickingProgram == 0)
                GL.DeleteProgram(program);
        }
    }

    private void DeletePickingBuffer()
    {
        if (_pickingBuffer != 0)
            GL.DeleteBuffer(_pickingBuffer);
        _pickingBuffer = 0;
        _pickingBufferBytes = 0;
        // The linked program lives with the renderer/context, like _program.
        // Clear may run without a current context; deletion is deferred by Render.
    }
}
