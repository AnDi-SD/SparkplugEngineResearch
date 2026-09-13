"""Freeze unchanged renderer headers; expose exact source RHS/packing for CPU tests."""
from pathlib import Path
import difflib,hashlib,json,re,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[3]
PACKAGE=Path(__file__).resolve().parent
UPSTREAM=ROOT/'local-data/rtx-remix/upstream/dxvk-remix'
PIN='b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4'
REL='src/dxvk/rtx_render/rtx_remix_api.cpp'
SHA='7596974C635FCA923EB42BFAE6D32E0BBA855C52935C298FC3AA0C5E34DF4743'
def digest(data):return hashlib.sha256(data).hexdigest().upper()
def prepare(destination):
    assert subprocess.check_output(['git','-C',str(UPSTREAM),'rev-parse','HEAD'],text=True).strip()==PIN
    old=(UPSTREAM/REL).read_bytes()
    assert digest(old)==SHA, 'Exact original renderer source changed'
    text=old.decode();new=text.replace('blendWeightsSlice, 0, sizeof(float), VK_FORMAT_R32_SFLOAT };;',
        'blendWeightsSlice, 0, sizeof(float) * src.skinning_value.bonesPerVertex, VK_FORMAT_R32_SFLOAT };')
    new=new.replace('blendIndicesSlice, 0, sizeof(uint32_t), VK_FORMAT_R8G8B8A8_USCALED',
        'blendIndicesSlice, 0, sizeof(uint32_t) * dxvk::divCeil(src.skinning_value.bonesPerVertex, 4u), VK_FORMAT_R8G8B8A8_USCALED')
    assert new!=text
    patch=''.join(difflib.unified_diff(text.splitlines(True),new.splitlines(True),fromfile='a/'+REL,tofile='b/'+REL))
    assert patch==(PACKAGE/'skin-strides-v1.patch').read_bytes().decode(), 'Reviewed patch drift'
    destination.mkdir(parents=True,exist_ok=False)
    source=destination/'source';shutil.copytree(UPSTREAM/'src/util',source/'src/util')
    shutil.copytree(UPSTREAM/'include/MathLib',source/'include/MathLib')
    dependencies=['src/dxvk/shaders/rtx/pass/skinning.h','src/dxvk/shaders/rtx/pass/gpu_skinning_binding_indices.h',
        'src/dxvk/shaders/rtx/utility/shader_types.h','src/dxvk/shaders/rtx/utility/cpu_gpu_compat.h',
        'src/dxvk/shaders/rtx/utility/cpu_gpu_compat_undef.h','src/dxvk/rtx_render/rtx_geometry_utils.cpp',
        'public/include/remix/remix_c.h',REL]
    for relative in dependencies:
        target=source/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(UPSTREAM/relative,target)
    patched=destination/'patched'/REL;patched.parent.mkdir(parents=True);patched.write_bytes(new.encode())
    # Vulkan-Headers is an uninitialized submodule. This test-only POD is used
    # solely to parse an unused util_matrix constructor; no Vulkan API executes.
    shim=source/'test-platform/vulkan/vulkan_core.h';shim.parent.mkdir(parents=True)
    shim.write_text('#pragma once\nstruct VkTransformMatrixKHR { float matrix[3][4]; };\n')
    geometry=(UPSTREAM/'src/dxvk/rtx_render/rtx_geometry_utils.cpp').read_text()
    for channel in ['blendWeight','blendIndices']:
        assert f'params.{channel}Stride = drawCallState.getGeometryData().{channel}Buffer.stride();' in geometry
    pieces=['// Generated verbatim expressions/loop from the two frozen source variants.\n',
        'struct ApiSource { size_t vertices_count; struct { uint32_t bonesPerVertex; const uint32_t* blendIndices_values; } skinning_value; };\n']
    for tag,body in [('Stock',text),('Fixed',new)]:
        for channel in ['Weight','Indices']:
            pattern=rf'dst.blend{channel}Buffer = dxvk::RasterBuffer \{{ blend(?:Weights|Indices)Slice, 0, (.*?), VK_FORMAT_'
            expression=re.search(pattern,body).group(1)
            pieces.append(f'inline uint32_t {tag}{channel}Stride(const ApiSource& src) {{ return uint32_t({expression}); }}\n')
    start=text.index('        auto compressedBlendIndices = std::vector<uint32_t> {};')
    end=text.index('        assert(sizeInBytes_indices',start)
    pieces.append('inline std::vector<uint32_t> Pack(const ApiSource& src) {\n size_t wordsPerCompressedTuple = dxvk::divCeil(src.skinning_value.bonesPerVertex, 4u);\n'+text[start:end].replace('\r\n','\n')+'\nreturn compressedBlendIndices;\n}\n')
    (destination/'source_expressions.h').write_text(''.join(pieces))
    shutil.copy2(PACKAGE/'test_skin_strides.cpp',destination/'test_skin_strides.cpp')
    metadata={'schema':1,'pin':PIN,'originalSourceSha256':digest(old),'patchedSourceSha256':digest(patched.read_bytes()),
        'patchSha256':digest((PACKAGE/'skin-strides-v1.patch').read_bytes()),'scope':'Actual unchanged CPU skinning header/args/math; exact source-derived RasterBuffer strides and index packing. No full renderer/GPU/COM. Test-only unused VkTransformMatrixKHR POD.'}
    (destination/'source.json').write_text(json.dumps(metadata,indent=2)+'\n')
if __name__=='__main__':prepare(Path(sys.argv[1]).resolve())
