"""Freeze the already built, unchanged Remix compute shader and Vulkan SDK inputs."""
from pathlib import Path
import json,re,shutil,struct,sys
from prepare_cpu import ROOT,PACKAGE,UPSTREAM,prepare,digest

def prepare_gpu(destination):
    renderer=ROOT/'local-data/rtx-remix/renderer-skinning-build/source'
    shaders=renderer/'_Comp64Release/src/dxvk/rtx_shaders'
    dependencies=['src/dxvk/shaders/rtx/pass/gpu_skinning.comp.slang',
        'src/dxvk/shaders/rtx/pass/skinning.h',
        'src/dxvk/shaders/rtx/pass/gpu_skinning_binding_indices.h',
        'src/dxvk/shaders/rtx/utility/shader_types.h',
        'src/dxvk/shaders/rtx/utility/cpu_gpu_compat.h',
        'src/dxvk/shaders/rtx/utility/cpu_gpu_compat_undef.h']
    for relative in dependencies:
        assert (renderer/relative).read_bytes()==(UPSTREAM/relative).read_bytes(),relative
    binary=(shaders/'gpu_skinning.spv').read_bytes()
    embedded=(shaders/'gpu_skinning.h').read_text()
    words=[int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{8})',embedded)]
    assert struct.pack('<'+'I'*len(words),*words)==binary,'Embedded SPIR-V differs'
    assert 20<=len(binary)<=65536
    prepare(destination)
    source=destination/'source'
    for relative in dependencies:
        target=source/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(renderer/relative,target)
    for original in (renderer/'include/vulkan/include').rglob('*.h'):
        relative=original.relative_to(renderer/'include/vulkan/include')
        target=source/'vulkan-headers'/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(original,target)
    for name in ['gpu_skinning.spv','gpu_skinning.h','gpu_skinning.spv.d']:
        original=shaders/name
        if original.exists():shutil.copy2(original,destination/name)
    shutil.copy2(renderer/'lib/vulkan-1.lib',destination/'vulkan-1.lib')
    for name in ['test_gpu_skinning.cpp','vulkan_skin_dispatch.h','prepare_gpu.py','prepare_cpu.py','Test-GPU.ps1']:
        shutil.copy2(PACKAGE/name,destination/name)
    metadata={'schema':1,'shaderSha256':digest(binary),
        'embeddedShaderRoundtrip':True,'shaderSourcesMatchPinnedUpstream':True,
        'scope':'Direct Vulkan compute dispatch of the unchanged embedded Remix SPIR-V. Actual shared CPU header and original Fixed semantic references. No full renderer, bridge, game, or draw validation.',
        'files':{str(p.relative_to(destination)).replace('\\','/'):digest(p.read_bytes())
                 for p in sorted(destination.rglob('*')) if p.is_file()}}
    (destination/'gpu-source.json').write_text(json.dumps(metadata,indent=2)+'\n')

if __name__=='__main__':prepare_gpu(Path(sys.argv[1]).resolve())
