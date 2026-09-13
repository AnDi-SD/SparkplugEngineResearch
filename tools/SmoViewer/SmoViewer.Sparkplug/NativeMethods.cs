using System.Numerics;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    private const string Library = "SparkplugViewerNative";
    static NativeMethods()
    {
        if (!OperatingSystem.IsWindows() || RuntimeInformation.ProcessArchitecture != Architecture.X64)
            throw new PlatformNotSupportedException("The Sparkplug viewing core requires Windows x64.");
        NativeLibrary.SetDllImportResolver(typeof(NativeMethods).Assembly, (name, assembly, flags) =>
            name == Library ? NativeLibrary.Load(Path.Combine(AppContext.BaseDirectory, Library + ".dll")) : IntPtr.Zero);
        if (spv_abi_version() != 2) throw new InvalidOperationException("Incompatible Sparkplug viewer bridge ABI (expected 2).");
    }
    [StructLayout(LayoutKind.Sequential)] internal struct Node
    {
        public int Parent;
        public Vector3 Position;
        public Quaternion Rotation;
        public Vector3 Scale;
        public uint Billboard;
    }
    [StructLayout(LayoutKind.Sequential)] internal struct Bone { public int Node; public Matrix4x4 InverseBind; }
    [StructLayout(LayoutKind.Sequential)] internal struct TrackInfo { public uint PositionKeys, RotationKeys, ScaleKeys; }
    [StructLayout(LayoutKind.Sequential)] internal struct Sample { public Vector3 Position; public Quaternion Rotation; public Vector3 Scale; public uint ValidRoles; }
    [StructLayout(LayoutKind.Sequential)] internal struct ChannelInfo { public uint SourceKeys, Axes, UniqueTimes; public fixed uint Representations[3]; }
    [StructLayout(LayoutKind.Sequential)] internal struct LinearChannel { public float* Times; public float* Values; public uint Count; }
    [StructLayout(LayoutKind.Sequential)] internal struct FieldHeader { public uint Field, PayloadSize, HeaderSize; }
    [StructLayout(LayoutKind.Sequential)] internal struct ReferencePrefix { public uint Id, InlineSize, Encoding, ClassId; }
    [StructLayout(LayoutKind.Sequential)] internal struct NodeField { public uint Field, Offset, Size; }
    [StructLayout(LayoutKind.Sequential)] internal struct StaticMatrices
    { public Matrix4x4 World, Inverse; public uint FieldMask; }
    [StructLayout(LayoutKind.Sequential)] internal struct MaterialReference { public uint Offset, Size; }
    [StructLayout(LayoutKind.Sequential)] internal struct MaterialInfo
    {
        public fixed uint States[11]; public uint VertexAlpha, HasColor;
        public fixed uint Colors[4]; public float Power;
        public MaterialReference ColorController; public uint Passes, Layers;
    }
    [StructLayout(LayoutKind.Sequential)] internal struct MaterialLayer
    {
        public uint Pass, Index, ClassId, Blend; public int StatesField;
        public fixed uint States[9]; public uint HasUv, UvEnabled; public fixed float UvMatrix[9];
        public MaterialReference Texture, Animation, UvController;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_material_read(byte* bytes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_material_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_material_info(MaterialHandle handle, out MaterialInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_material_layers(MaterialHandle handle, MaterialLayer* layers, uint count);
    [StructLayout(LayoutKind.Sequential)] internal struct MaterialPass { public uint Blend, Layers; }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_material_passes(MaterialHandle handle, MaterialPass* passes, uint count);
    [StructLayout(LayoutKind.Sequential)] internal struct ModelInfo
    {
        public uint Alpha, Priority, Projection, Weights, RenderableMask, ModelMask, SkinMask, Bones;
        public MaterialReference Material, Fog, Mesh;
    }
    [StructLayout(LayoutKind.Sequential)] internal struct SkinBone
    {
        public MaterialReference Reference; public uint Id, InlineSize; public Matrix4x4 InverseBind;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_model_read(byte* bytes, uint count, uint kind);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_model_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_model_info(ModelHandle handle, out ModelInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_model_bones(ModelHandle handle, SkinBone* bones, uint count);
    [StructLayout(LayoutKind.Sequential)] internal struct AnimTextureInfo { public uint Frames, HasTrack; public float Duration; }
    [StructLayout(LayoutKind.Sequential)] internal struct AnimTextureFrame { public float Time; public MaterialReference Reference; }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_anim_texture_read(byte* bytes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_anim_texture_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_anim_texture_info(AnimTextureHandle handle, out AnimTextureInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_anim_texture_frames(AnimTextureHandle handle, AnimTextureFrame* frames, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_anim_texture_index(AnimTextureHandle handle, float time, out int index);
    [StructLayout(LayoutKind.Sequential)] internal struct FunctionInfo
    { public uint Type; public float Frequency, Amplitude, XOffset, YOffset, Pitch; }
    [StructLayout(LayoutKind.Sequential)] internal struct ColorFunctionInfo
    { public uint First, Second; public FunctionInfo Function; }
    [StructLayout(LayoutKind.Sequential)] internal struct UvFunctions
    { public FunctionInfo Tx, Ty, Tz, Sx, Sy, Sz, Rotation; public Vector3 Pivot, Axis; }
    [StructLayout(LayoutKind.Sequential)] internal struct ColorFunctions
    { public ColorFunctionInfo Ambient, Diffuse, Specular, Emissive; public FunctionInfo Alpha; }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_uv_functions_read(byte* bytes, uint count, out UvFunctions values);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_color_functions_read(byte* bytes, uint count, out ColorFunctions values);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_static_matrices(
        byte* bytes, uint count, NodeField* fields, uint fieldCount, out StaticMatrices values);
    [StructLayout(LayoutKind.Sequential)] internal struct CollisionInfoValues
    {
        public Vector3 Position; public Quaternion Rotation; public Vector3 Scale;
        public fixed float Orientation[9]; public uint Group, FieldMask;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_collision_info_values(
        byte* bytes, uint count, NodeField* fields, uint fieldCount, out CollisionInfoValues values);
    [StructLayout(LayoutKind.Sequential)] internal struct NodeValues
    {
        public Vector3 Position; public Quaternion Rotation; public Vector3 Scale;
        public fixed float Orientation[9]; public uint Flags, Billboard, Bone, Static, Animated;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_node_values(byte* bytes, uint count, NodeField* fields, uint fieldCount, out NodeValues values);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_reference_prefix(byte* bytes, uint count, uint payloadSize, uint kind, out ReferencePrefix prefix);
    [StructLayout(LayoutKind.Sequential)] internal struct ContainerInfo
    { public uint Signature, Version, ExportTag, FileSize, PlatformMask, DataOffset, DataSize, ObjectCount, HeaderStatus; }
    [StructLayout(LayoutKind.Sequential)] internal struct ContainerEntry
    { public uint TableOffset, Id, NameOffset, NameBytes, ClassId, Offset, Size, SignatureClassId, SignatureFlags; }
    [StructLayout(LayoutKind.Sequential)] internal struct MeshBVInfo
    { public uint PrimitiveType, Vertices, Indices, Faces, HasFaces, FieldMask, VertexPayloadOffset, FaceClassId; }
    [StructLayout(LayoutKind.Sequential)] internal struct FaceData
    { public uint SurfaceType, Flags, SurfaceId, FieldMask; }
    [StructLayout(LayoutKind.Sequential)] internal struct VertexLayout
    { public uint Stride; public int Normal, Color, Uv0, Uv1, Weights, Bones; }
    [StructLayout(LayoutKind.Sequential)] internal struct MeshInfo
    {
        public uint FieldId, FieldPayloadOffset, IndexPayloadOffset, VertexPayloadOffset;
        public uint PrimitiveType, Primitives, Vertices, Indices, IndexElementSize;
        public uint SerializedStride, RuntimeStride, RuntimeVbSize, ComponentFlags, Attributes;
        public fixed uint PlanningWords[4]; public uint PlanningByte;
    }
    [StructLayout(LayoutKind.Sequential)] internal struct MeshVertex
    { public Vector3 Position, Normal; public Vector2 Uv0, Uv1; public Vector4 Weights; public uint Color, Bones; }
    [StructLayout(LayoutKind.Sequential)] internal struct Ps2MeshHeader
    { public Vector4 Sphere; public uint Primitives, Vertices, ComponentFlags, PacketQwords, AdditionalUvCount, WeightCount; }
    [StructLayout(LayoutKind.Sequential)] internal struct MeshBounds { public Vector3 Minimum, Maximum; }
    [StructLayout(LayoutKind.Sequential)] internal struct TextureSectionInfo
    { public uint Kind, Width, Height, Format, Auxiliary, BitsPerPixel, PixelDataPresent, Mips; }
    [StructLayout(LayoutKind.Sequential)] internal struct TextureMip
    { public uint Width, Height, Descriptor0, Descriptor1, Descriptor2, PixelOffset, PixelSize; }
    [StructLayout(LayoutKind.Sequential)] internal struct TextureSourceInfo
    { public uint Fields, Representations, Mips, FinalPosition, RuntimeWidth, RuntimeHeight, RuntimeMips; }
    [StructLayout(LayoutKind.Sequential)] internal struct TextureSourceField
    {
        public uint Scope, Outcome, FrameOffset, FrameSize, Depth, FieldId, HeaderOffset, PayloadOffset, PayloadSize;
        public uint PlatformBefore, PlatformAfter, InputStream, Complete, HandledBefore, HandledAfter;
    }
    [StructLayout(LayoutKind.Sequential)] internal struct TextureSourceRepresentation
    { public uint Kind, FieldIndex, Width, Height, Format, Auxiliary, BitsPerPixel, NativeFlag, Field1C, FirstMip, Mips; }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_texture_source_read(byte* bytes, uint count, uint platformMask);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_texture_source_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_texture_source_info(TextureSourceHandle handle, out TextureSourceInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_texture_source_fields(TextureSourceHandle handle, TextureSourceField* fields, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_texture_source_representations(TextureSourceHandle handle, TextureSourceRepresentation* representations, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_texture_source_mips(TextureSourceHandle handle, TextureMip* mips, uint count);
    [StructLayout(LayoutKind.Sequential)] internal struct Ps2TextureInfo
    { public uint Fields, Images, Mips, FinalPosition; }
    [StructLayout(LayoutKind.Sequential)] internal struct Ps2TextureImage
    { public uint FieldIndex, NativeFlag, Format, Width, Height, Auxiliary, PaletteOffset, PaletteSize, FirstMip, Mips; }
    [StructLayout(LayoutKind.Sequential)] internal struct Ps2TextureMip
    { public uint Descriptor0, Descriptor1, Descriptor2, DataSize, DescriptorOffset, DataOffset; }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_ps2_texture_inspect(byte* bytes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_ps2_texture_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_ps2_texture_info(Ps2TextureHandle handle, out Ps2TextureInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_ps2_texture_images(Ps2TextureHandle handle, Ps2TextureImage* images, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_ps2_texture_mips(Ps2TextureHandle handle, Ps2TextureMip* mips, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_texture_section_read(byte* bytes, uint count, uint kind);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_texture_section_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_texture_section_info(TextureSectionHandle handle, out TextureSectionInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_texture_section_mips(TextureSectionHandle handle, TextureMip* mips, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_texture_section_bgra(TextureSectionHandle handle, byte* output, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_texture_xrgb_bgra(byte* input, uint count, byte* output, uint outputCount);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_texture_section_field1c(TextureSectionHandle handle, out uint value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_texture_write_bgra(byte* pixels, uint count, uint width, uint height, uint field1C, uint kind);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_mesh_write_triangles(MeshVertex* vertices, uint vertexCount, uint* indices, uint indexCount, uint componentFlags, uint kind);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_mesh_bv_write_triangles(MeshVertex* vertices, uint vertexCount, uint* indices, uint indexCount);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_serialized_bytes_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_serialized_bytes_size(SerializedBytesHandle handle, out uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_serialized_bytes_copy(SerializedBytesHandle handle, byte* output, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_ps2_mesh_header(byte* bytes, uint count, out Ps2MeshHeader header);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_mesh_bounds(byte* bytes, uint count, out MeshBounds bounds);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_mesh_read(byte* bytes, uint count, uint kind, uint platformMask);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_mesh_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_mesh_info(MeshHandle handle, out MeshInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_mesh_vertices(MeshHandle handle, MeshVertex* vertices, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_mesh_indices(MeshHandle handle, uint* indices, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_mesh_triangles(MeshHandle handle, uint* indices, uint capacity, out uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_vertex_layout(uint flags, out VertexLayout layout);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_mesh_bv_read(byte* bytes, uint count, uint kind);
    [StructLayout(LayoutKind.Sequential)] internal struct IndexBufferInfo
    { public uint PrimitiveType, PrimitiveCount, IndexCount, FormatFlags, ElementSize, FinalPosition; }
    [StructLayout(LayoutKind.Sequential)] internal struct VertexBufferInfo
    { public uint ComponentFlags, VertexCount, Flags, Stride, ComponentCount, ByteCount, FinalPosition; }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_index_buffer_read(byte* bytes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_index_buffer_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_index_buffer_info(IndexBufferHandle handle, out IndexBufferInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_index_buffer_indices(IndexBufferHandle handle, uint* indices, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_vertex_buffer_read(byte* bytes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_vertex_buffer_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_vertex_buffer_info(VertexBufferHandle handle, out VertexBufferInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_vertex_buffer_positions(VertexBufferHandle handle, Vector3* positions, uint floats);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_mesh_bv_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_mesh_bv_info(MeshBVHandle handle, out MeshBVInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_mesh_bv_geometry(MeshBVHandle handle, Vector3* positions, uint floats, int* indices, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_mesh_bv_faces(MeshBVHandle handle, FaceData* faces, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_container_inspect(byte* bytes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_container_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_container_info(ContainerHandle handle, out ContainerInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_container_entries(ContainerHandle handle, ContainerEntry* entries, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_read_field(byte* bytes, uint count, out FieldHeader header);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_write_field_header(uint field, uint payloadSize, uint preferredCode,
        uint preferExtended, byte* output, uint capacity, out uint size);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_node_local(in Node node, out Matrix4x4 matrix, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_skin_matrix(in Matrix4x4 inverseBind, in Matrix4x4 world, out Matrix4x4 matrix, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern uint spv_abi_version();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern IntPtr spv_last_error();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_scene_create(Node* nodes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_scene_destroy(IntPtr scene);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_clip_load(byte* bytes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_clip_destroy(IntPtr clip);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_clip_info(ClipHandle clip, out float duration, out uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_clip_track(ClipHandle clip, uint index, byte* name, uint capacity, out TrackInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_clip_sample(ClipHandle clip, uint index, float time, out Sample sample);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_clip_channel(ClipHandle clip, uint index, uint role, out ChannelInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_clip_times(ClipHandle clip, uint index, uint role, float* output, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_clip_create_linear(LinearChannel* channels, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_bind(SceneHandle scene, ClipHandle clip, int* roles, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_sample(SceneHandle scene, float time, Matrix4x4* worlds, uint floats);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_palette(SceneHandle scene, Bone* bones, uint count, Matrix4x4* matrices, uint floats);
    internal static void Check(int success) { if (success == 0) throw new InvalidDataException(Marshal.PtrToStringUTF8(spv_last_error()) ?? "Sparkplug runtime failure."); }
    internal static IntPtr Check(IntPtr handle) { Check(handle == IntPtr.Zero ? 0 : 1); return handle; }
}
internal sealed class SceneHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal SceneHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_scene_destroy(handle); return true; }
}
internal sealed class MaterialHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal MaterialHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_material_destroy(handle); return true; }
}
internal sealed class ModelHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal ModelHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_model_destroy(handle); return true; }
}
internal sealed class AnimTextureHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal AnimTextureHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_anim_texture_destroy(handle); return true; }
}
internal sealed class ClipHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal ClipHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_clip_destroy(handle); return true; }
}
internal sealed class ContainerHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal ContainerHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_container_destroy(handle); return true; }
}
internal sealed class MeshBVHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal MeshBVHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_mesh_bv_destroy(handle); return true; }
}
internal sealed class SerializedBytesHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal SerializedBytesHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_serialized_bytes_destroy(handle); return true; }
}
internal sealed class TextureSectionHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal TextureSectionHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_texture_section_destroy(handle); return true; }
}
internal sealed class IndexBufferHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal IndexBufferHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_index_buffer_destroy(handle); return true; }
}
internal sealed class VertexBufferHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal VertexBufferHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_vertex_buffer_destroy(handle); return true; }
}
internal sealed class TextureSourceHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal TextureSourceHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_texture_source_destroy(handle); return true; }
}
internal sealed class Ps2TextureHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal Ps2TextureHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_ps2_texture_destroy(handle); return true; }
}
internal sealed class MeshHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal MeshHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_mesh_destroy(handle); return true; }
}
