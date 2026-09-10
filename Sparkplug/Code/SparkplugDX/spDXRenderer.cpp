#include "spDXRenderer.h"
#include "spDXMaterial.h"
#include "spDXTexture.h"
#include "spDXLight.h"
#include "spDXIndexBuffer.h"
#include "spDXVertexBuffer.h"
#include "../Sparkplug/spMaterialPassLayer.h"
#include "../Sparkplug/spMaterialTextureLayer.h"
#include "../Sparkplug/spMaterialTexture.h"
#include "../Sparkplug/spFog.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "Analysis/PC/spColorMath.h"
#include "Analysis/PC/spNodeTransformMath.h"
#include <cstring>
#include "../SparkplugPC/spPCVertexDeclaration.h"
#include "../SparkplugPC/spPCShaderManager.h"

namespace sparkplug::reconstruction
{
    spDXRenderer::spDXRenderer() : spRenderer(TextureStateCacheCount)
    {
    }

    spDXRenderer::~spDXRenderer() = default;

    bool spDXRenderer::InitializePCSubmissionCachesForAnalysis(SubmissionStateForAnalysis& state,
        RenderStateSubmitForAnalysis submit,void* context) noexcept
    {
        if(!submit)return false; // host guard; original requires a device
        constexpr auto invalid=std::numeric_limits<std::uint32_t>::max();
        InvalidateCacheWordsForAnalysis(state.raw.data(),state.raw.size());
        for(auto& stage:state.textures.cache)
        {
            InvalidateCacheWordsForAnalysis(stage.raw.data(),stage.raw.size());
            stage.coordinateIndex=stage.transformFlags=invalid;
        }
        // The first9 shared PC texture defaults equal constructor C29C[8][9].
        const spMaterialTexture defaults;
        for(auto& stage:state.textures.desired)
            std::copy_n(defaults.GetTextureStatesForAnalysis().begin(),stage.size(),stage.begin());
        state.frame=1;
        state.textures.dirty.fill(0); // constructor C1B8; identities remain caller input
        state.textures.palette=invalid;
        InvalidateCacheWordsForAnalysis(state.deviceStates.data(),state.deviceStates.size());
        constexpr std::array<std::array<std::uint32_t,2>,5> desired{{{143,1},{27,1},{15,1},{24,192},{25,7}}};
        for(const auto& entry:desired)
            (void)ApplyRenderStateCacheEntryForAnalysis(state.deviceStates[entry[0]],entry[0],entry[1],submit,context);
        state.textures.boundTextures.fill(invalid); // native32-bit invalid identity token
        state.lighting.diffuseSource=11;
        state.lighting.ambientSource=10;
        return true;
    }

    bool spDXRenderer::ApplyFogForAnalysis(FogStateForAnalysis& state,const spFog* input,
        std::array<std::uint32_t,256>& cache,RenderStateSubmitForAnalysis submit,void* context) noexcept
    {
        const auto* selected=input?input:state.fallback;
        if(selected==state.current)return true;
        // Native would publish NULL then dereference it. Keep this invalid
        // missing-fallback transition an explicit host-only guard.
        if(!selected)return false;
        state.current=selected;
        const auto kind=static_cast<std::uint32_t>(selected->GetTypeForAnalysis());
        const auto send=[&](unsigned index,unsigned value)
        {return ApplyRenderStateCacheEntryForAnalysis(cache[index],index,value,submit,context);};
        const auto bits=[](float value){std::uint32_t word;std::memcpy(&word,&value,4);return word;};
        if(kind==0)return send(0x1c,0);
        if(kind>3)return false;
        if(!send(0x1c,1)||!send(0x23,kind))return false;
        if(kind==3)
        {if(!send(0x24,bits(selected->GetStartForAnalysis()))||!send(0x25,bits(selected->GetEndForAnalysis())))return false;}
        else if(!send(0x26,bits(selected->GetDensityForAnalysis())))return false;
        return send(0x22,selected->GetColorARGBForAnalysis());
    }

    bool spDXRenderer::RefreshMatricesForAnalysis(MatrixStateForAnalysis& state) noexcept
    {
        using namespace sparkplug::evidence::pc::node_math;
        std::array<Matrix4,3> inputs;
        for(unsigned m=0;m<3;++m)for(unsigned i=0;i<16;++i)
        {
            std::memcpy(&inputs[m][i],&state.inputs[m][i],4);
            if(!std::isfinite(inputs[m][i]))return false; // explicit host boundary
        }
        const auto vp=Multiply4ForAnalysis(inputs[1],inputs[2]);
        const auto view=Multiply4ForAnalysis(inputs[0],inputs[1]);
        const auto wvp=Multiply4ForAnalysis(inputs[0],vp);
        for(unsigned i=0;i<16;++i)
        {
            std::memcpy(&state.cached[0][i],&view[i],4);
            std::memcpy(&state.cached[1][i],&vp[i],4);
            std::memcpy(&state.cached[2][i],&wvp[i],4);
        }
        state.dirty=false;return true;
    }
    bool spDXRenderer::SetInputMatrixForAnalysis(MatrixStateForAnalysis& state,unsigned index,
        const MatrixStateForAnalysis::RawMatrix& matrix,std::uint32_t,
        MatrixInputSubmitForAnalysis submit,void* context) noexcept
    {
        if(index>=3||!submit)return false;
        state.inputs[index]=matrix;state.dirty=true;
        constexpr std::array<unsigned,3> transforms{256,2,3};
        (void)submit(context,transforms[index],state.inputs[index]);return true;
    }
    const spDXRenderer::MatrixStateForAnalysis::RawMatrix* spDXRenderer::GetInputMatrixForAnalysis(
        const MatrixStateForAnalysis& state,unsigned index) noexcept
    {return index<3?&state.inputs[index]:nullptr;}
    const spDXRenderer::MatrixStateForAnalysis::RawMatrix* spDXRenderer::GetCachedMatrixForAnalysis(
        MatrixStateForAnalysis& state,unsigned index) noexcept
    {
        if(index>=3||(state.dirty&&!RefreshMatricesForAnalysis(state)))return nullptr;
        return &state.cached[index];
    }

    bool spDXRenderer::ResolveLightCacheForAnalysis(const spLightManager::CacheForAnalysis& cache,
        std::array<DeviceLightInputForAnalysis,9>& storage,LightListInputForAnalysis& output,
        const std::uint32_t unknownDeviceWord,LightIdentityForAnalysis identity)
    {
        const auto resolve=[&](const spLight* input,std::size_t index)->bool
        {
            const auto* light=dynamic_cast<const spDXLight*>(input);if(!light)return false;
            auto& item=storage[index];item.identity=identity?identity(*light):reinterpret_cast<std::uintptr_t>(light);
            item.type=static_cast<std::uint32_t>(light->GetTypeForAnalysis());item.enabled=light->IsLightEnabledForAnalysis();
            item.color=light->GetColorForAnalysis();item.sourceObject=light;
            for(std::size_t i=0;i<item.deviceWords.size();++i)item.deviceWords[i]=light->GetDevicePayloadForAnalysis()[i].value_or(unknownDeviceWord);
            return true;
        };
        output.lights.clear();output.ambient=nullptr;
        for(std::size_t i=0;i<cache.GetCount();++i)
        {if(!resolve(cache.Get(i),i))return false;output.lights.push_back(&storage[i]);}
        if(cache.GetAmbient())
        {if(!resolve(cache.GetAmbient(),8))return false;output.ambient=&storage[8];}
        return true;
    }

    bool spDXRenderer::SubmitLightsForAnalysis(LightSubmissionStateForAnalysis& state,
        const LightListInputForAnalysis* input,std::uint32_t fallbackARGB,DeviceLightSubmitForAnalysis submit,
        DeviceLightEnableForAnalysis enable,RenderStateSubmitForAnalysis render,void* context) noexcept
    {
        if(!enable||!render||state.previousCount>8)return false;
        if(!input)
        {
            for(unsigned i=0;i<8;++i)(void)enable(context,i,false);
            state.ambient.fill(0);
            if(!ApplyRenderStateCacheEntryForAnalysis(state.deviceAmbient,139,0,render,context))return false;
            state.borrowedList=0;return true;
        }
        if(input->lights.size()>8)return false;
        for(const auto* light:input->lights)
            if(!light||(light->enabled&&(!submit||light->type>=3)))return false;
        auto ambient=input->ambient&&input->ambient->enabled?input->ambient->color:PCARGBToRGBAForAnalysis(fallbackARGB);
        for(unsigned i=0;i<4;++i)
            if(!std::isfinite(ambient[i])||std::abs(ambient[i])>4096||!std::isfinite(state.ambient[i]))return false;
        state.grouped.fill(0);std::array<unsigned,3> counts{};
        unsigned i=0;
        for(const auto* light:input->lights)
        {
            if(light->enabled)
            {
                (void)submit(context,i,light->deviceWords);(void)enable(context,i,true);
                state.grouped[light->type*8+counts[light->type]++]=light->identity;
            }
            else (void)enable(context,i,false);
            ++i;
        }
        for(;i<state.previousCount;++i)(void)enable(context,i,false);
        state.previousCount=static_cast<std::uint32_t>(input->lights.size());
        constexpr float epsilon=0.0010000000474974513F;bool changed=false;
        for(unsigned channel=0;channel<4;++channel)
            if(std::abs(double(ambient[channel])-double(state.ambient[channel]))>double(epsilon))changed=true;
        if(changed)
        {
            state.ambient=ambient;
            const auto byte=[](float value){return static_cast<std::uint32_t>(static_cast<std::int32_t>(double(value)*255.0))&255u;};
            const std::uint32_t packed=(byte(ambient[3])<<24)|(byte(ambient[0])<<16)|(byte(ambient[1])<<8)|byte(ambient[2]);
            if(!ApplyRenderStateCacheEntryForAnalysis(state.deviceAmbient,139,packed,render,context))return false;
        }
        return true;
    }

    bool spDXRenderer::SubmitUnlitGeometryForAnalysis(SubmissionStateForAnalysis& state,
        const spPCVertexDeclaration* declaration,const std::shared_ptr<spDXIndexBuffer>& indices,
        const std::shared_ptr<spDXVertexBuffer>& vertices,const std::array<std::uintptr_t,3>& handles,
        std::uint32_t stride,const std::array<std::uint32_t,7>& args,spDXMaterial& material,
        spDXMaterial& fallback,spPCShaderManager& manager,const SubmissionDeviceForAnalysis& device,
        const spDXShader::ConstantInputsForAnalysis* constants,const spPCShaderGenerationForAnalysis* generation)
    {
        return material.GetRenderStateForAnalysis(8)==2 && SubmitGeometryForAnalysis(state,
            declaration,indices,vertices,handles,stride,args,material,fallback,manager,device,constants,generation);
    }

    bool spDXRenderer::SubmitGeometryForAnalysis(SubmissionStateForAnalysis& state,
        const spPCVertexDeclaration* declaration,const std::shared_ptr<spDXIndexBuffer>& indices,
        const std::shared_ptr<spDXVertexBuffer>& vertices,const std::array<std::uintptr_t,3>& handles,
        std::uint32_t stride,const std::array<std::uint32_t,7>& args,spDXMaterial& material,
        spDXMaterial& fallback,spPCShaderManager& manager,const SubmissionDeviceForAnalysis& device,
        const spDXShader::ConstantInputsForAnalysis* constants,const spPCShaderGenerationForAnalysis* generation,
        LightSubmissionForAnalysis* lights)
    {
        if((material.GetRenderStateForAnalysis(8)!=2&&!lights)||((args[5]&0x1e)&&!constants)||args[0]>4||
            (constants&&constants->material!=&material)||
            !material.HasInitializedSpecularPowerForAnalysis()||state.draw.vertex.top>=4||
            state.draw.vertex.entries[state.draw.vertex.top].identity)return false;
        auto& geometry=state.geometry;
        if(geometry.declaration!=declaration)
        {
            if(!declaration||!device.geometry)return false;
            // Actual PC4C9D00 ignores the bool argument supplied by4BC4A0.
            (void)device.geometry(device.context,0,handles[0],0);geometry.declaration=declaration;
        }
        if(geometry.indices!=indices)
        {
            if(!device.geometry)return false;
            (void)device.geometry(device.context,1,indices?handles[1]:0,0);geometry.indices=indices;
        }
        if(geometry.vertices!=vertices)
        {
            if(!vertices||!device.geometry)return false;
            (void)device.geometry(device.context,2,handles[2],stride);geometry.vertices=vertices;
        }
        // Stride alone does not invalidate the native buffer identity cache.
        if(!InstallMaterialForAnalysis(state.lighting,state.installedMaterial,&material,state.frame))return false;
        struct Bridge final
        {
            SubmissionStateForAnalysis& state;const SubmissionDeviceForAnalysis& device;
            static std::int32_t Render(void* ptr,std::uint32_t index,std::uint32_t value) noexcept
            {
                auto& b=*static_cast<Bridge*>(ptr);
                if(index>=b.state.deviceStates.size())return -1;
                return ApplyRenderStateCacheEntryForAnalysis(b.state.deviceStates[index],index,value,b.device.render,b.device.context)?0:-1;
            }
            static bool UV(void* ptr,std::uint32_t stage,const TextureMatrix3ForAnalysis& matrix)
            {
                auto& b=*static_cast<Bridge*>(ptr);return stage<8&&ApplyTextureTransform3ForAnalysis(
                    b.state.uv[stage],stage,matrix,b.device.transform,b.device.context);
            }
        } bridge{state,device};
        if(lights)state.lighting.globalBlackARGB=lights->fallbackARGB;
        if(!device.render||!ApplyMaterialStateSetForAnalysis(state.raw,material.GetRenderStatesForAnalysis(),nullptr,state.lighting,Bridge::Render,&bridge))return false;
        if(material.GetRenderStateForAnalysis(8)!=2)
        {
            lights->state.deviceAmbient=state.deviceStates[139];
            if(!SubmitLightsForAnalysis(lights->state,lights->input,lights->fallbackARGB,
                device.light,device.lightEnable,device.render,device.context))return false;
            state.deviceStates[139]=lights->state.deviceAmbient;
        }
        std::vector<std::uint32_t> lightTypes;
        const bool hasLights=lights&&lights->input;
        if(hasLights)
            for(const auto* light:lights->input->lights)
            {if(!light)return false;lightTypes.push_back(light->type);}
        std::optional<spDXShader::ConstantInputsForAnalysis> resolvedConstants;
        if(constants&&lights)
        {
            resolvedConstants=*constants;
            auto& resolved=*resolvedConstants;
            resolved.rendererAmbient=lights->state.ambient;
            if(resolved.rendererMatrices)resolved.viewMatrix=resolved.rendererMatrices->inputs[1];
            resolved.lightListPresent=hasLights;
            if(hasLights)
            {
                const auto read=[](const DeviceLightInputForAnalysis& input)->std::optional<spDXShader::LightForAnalysis>
                {
                    if(!input.sourceObject)return std::nullopt;
                    const auto& light=*input.sourceObject;spDXShader::LightForAnalysis result;
                    result.type=static_cast<std::uint32_t>(light.GetTypeForAnalysis());result.color=light.GetColorForAnalysis();
                    result.worldPosition=light.GetWorldPositionForAnalysis();
                    const auto& world=light.GetWorldOrientationForAnalysis();const auto& local=light.GetOrientationForAnalysis();
                    for(unsigned i=0;i<3;++i)
                    {
                        result.worldDirection[i]=world[6+i];result.localDirection[i]=local[6+i];
                        const auto value=light.GetDevicePayloadForAnalysis()[21+i];
                        if(value)std::memcpy(&result.attenuation[i],&*value,4);
                        else result.attenuation[i]=std::numeric_limits<float>::quiet_NaN(); // unknown: type22 guard refuses
                    }
                    result.attenuation[3]=light.GetRangeForAnalysis();
                    result.innerAngle=light.GetHotspotAngleForAnalysis();result.outerAngle=light.GetFalloffAngleForAnalysis();
                    return result;
                };
                resolved.lights.clear();resolved.ambientLight.reset();resolved.directionalLight.reset();
                for(const auto* input:lights->input->lights)
                {
                    const auto light=read(*input);if(!light)return false;
                    resolved.lights.push_back(*light);
                    if(input->identity==lights->state.grouped[0])resolved.directionalLight=*light;
                }
                if(lights->input->ambient)
                {resolved.ambientLight=read(*lights->input->ambient);if(!resolved.ambientLight)return false;}
            }
            constants=&resolved;
        }
        if(!device.material)return false;(void)device.material(device.context,state.lighting);
        for(std::size_t index=0;index<material.GetPassCountForAnalysis();++index)
        {
            auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(index));
            if(!pass||!pass->UpdateForRenderForAnalysis(0xffffffff,Bridge::UV,&bridge))return false;
            const auto* defaultPass=dynamic_cast<spMaterialPassLayer*>(fallback.GetPassForAnalysis(0));
            if(!defaultPass)return false;
            std::array<const spMaterialTexture*,2> defaults{};
            for(unsigned i=0;i<2;++i)
            {const auto& layer=defaultPass->GetLayerForAnalysis(i);if(!layer)return false;defaults[i]=layer->GetMaterialTextureForAnalysis().get();}
            std::array<ResolvedTextureBindingForAnalysis,8> bindings{};
            if(pass->GetLayerCountForAnalysis()>bindings.size())return false;
            for(std::size_t i=0;i<pass->GetLayerCountForAnalysis();++i)
            {
                const auto& layer=pass->GetLayerForAnalysis(i);
                if(!layer||!layer->GetMaterialTextureForAnalysis())return false;
                const auto* texture=layer->GetMaterialTextureForAnalysis()->GetTextureForAnalysis();
                if(!texture)continue;
                // Actual PC4BB650 DXTexture branch. Other RTTI resource families
                // remain unsupported at this whole-caller boundary.
                const auto* dx=dynamic_cast<const spDXTexture*>(texture);
                if(!dx||!device.textureHandle)return false;
                const auto handle=device.textureHandle(device.context,*dx);if(!handle)return false;
                bindings[i].identity=reinterpret_cast<std::uintptr_t>(texture);
                bindings[i].deviceTexture=*handle;
                if(const auto* palette=dx->GetPaletteForAnalysis())bindings[i].paletteIndex=palette->GetIndexForAnalysis();
            }
            if(!ApplyPassTextureStatesForAnalysis(state.textures,*pass,defaults,bindings,nullptr,false,false,
                device.texture,device.palette,device.textureState,device.context))return false;
            if(!ApplyPassBlendForAnalysis(state.raw,material,*pass,nullptr,Bridge::Render,&bridge))return false;
            std::array<std::uint32_t,8> uv{};for(unsigned i=0;i<8;++i)uv[i]=state.textures.cache[i].raw[8];
            const bool drawn=constants?
                DrawAutomaticForAnalysis(state.draw,args,manager,*constants,state.raw[8],hasLights?&lightTypes:nullptr,uv,
                    device.shader,device.constants,device.draw,device.context,generation):
                DrawWithoutBlendWeightsForAnalysis(state.draw,args,manager,material,state.raw[8],hasLights?&lightTypes:nullptr,uv,
                    device.shader,device.constants,device.draw,device.context);
            if(!drawn)return false;
        }
        return true;
    }

    std::optional<std::array<std::uint32_t,2>> spDXRenderer::BuildShaderKeyForAnalysis(
        std::uint32_t flags,std::uint32_t color,float power,const std::vector<std::uint32_t>* lights,
        const std::array<std::uint32_t,8>& uv) noexcept
    {
        if(lights&&lights->size()>8)return std::nullopt;
        std::uint32_t mask=color<<16,types=0;
        for(unsigned count=8;count>0;--count)if(flags&(0x400u<<count)){mask|=count<<4;break;}
        for(unsigned count=4;count>0;--count)if(flags&(1u<<count)){mask|=count;break;}
        if(lights)
        {
            mask|=static_cast<std::uint32_t>(lights->size())<<20;
            for(unsigned i=0;i<lights->size();++i)types|=(*lights)[i]<<(2*i);
        }
        if(power>0)mask|=0x1000000;
        for(unsigned i=0;i<8;++i)if(uv[i]&2)mask|=0x100u<<i;
        return std::array<std::uint32_t,2>{mask,types};
    }

    bool spDXRenderer::PushShaderForAnalysis(ShaderStackForAnalysis& stack,ShaderBindingForAnalysis shader) noexcept
    {if(stack.top>=stack.entries.size()-1)return false;stack.entries[++stack.top]=shader;return true;}
    bool spDXRenderer::PopShaderForAnalysis(ShaderStackForAnalysis& stack) noexcept
    {if(!stack.top||stack.top>=stack.entries.size())return false;--stack.top;return true;}
    bool spDXRenderer::DrawPreselectedForAnalysis(DrawStateForAnalysis& state,
        const std::array<std::uint32_t,7>& args,DeviceShaderBindForAnalysis shader,
        DeviceShaderConstantsForAnalysis constants,DevicePrimitiveDrawForAnalysis draw,void* context) noexcept
    {
        if(state.vertex.top>=4||!state.vertex.entries[state.vertex.top].identity)return false;
        return DrawResolvedForAnalysis(state,args,shader,constants,draw,context);
    }
    bool spDXRenderer::DrawWithoutBlendWeightsForAnalysis(DrawStateForAnalysis& state,
        const std::array<std::uint32_t,7>& args,spPCShaderManager& manager,const spMaterial& material,
        std::uint32_t color,const std::vector<std::uint32_t>* lights,const std::array<std::uint32_t,8>& uv,
        DeviceShaderBindForAnalysis shader,DeviceShaderConstantsForAnalysis constants,DevicePrimitiveDrawForAnalysis draw,void* context) noexcept
    {
        if(state.vertex.top>=4||state.vertex.entries[state.vertex.top].identity||(args[5]&0x1e)||!material.HasInitializedSpecularPowerForAnalysis())return false;
        const auto key=BuildShaderKeyForAnalysis(args[5],color,material.GetSpecularPowerForAnalysis(),lights,uv);
        if(!key)return false;const auto selected=manager.SelectCachedForAnalysis(*key);
        if(!selected.completed||selected.shader)return false;
        // Actual4BE2B0 sees NULL and leaves constant cache/count unchanged.
        return DrawResolvedForAnalysis(state,args,shader,constants,draw,context);
    }
    bool spDXRenderer::DrawResolvedForAnalysis(DrawStateForAnalysis& state,
        const std::array<std::uint32_t,7>& args,DeviceShaderBindForAnalysis shader,
        DeviceShaderConstantsForAnalysis constants,DevicePrimitiveDrawForAnalysis draw,void* context) noexcept
    {
        if(state.vertex.top>=4||state.pixel.top>=4||args[0]>4||!draw)return false;
        const auto& vertex=state.vertex.entries[state.vertex.top];
        if(vertex.identity!=state.boundVertex)
        {if(!shader)return false;state.boundVertex=vertex.identity;(void)shader(context,false,vertex.identity?vertex.deviceShader:0);}
        if(!state.vertexConstants.empty())
        {if(!constants)return false;(void)constants(context,false,state.vertexConstants.data(),state.vertexConstants.size());}
        if(state.pixelEnabled)
        {
            const auto& pixel=state.pixel.entries[state.pixel.top];
            if(pixel.identity!=state.boundPixel)
            {if(!shader)return false;state.boundPixel=pixel.identity;(void)shader(context,true,pixel.identity?pixel.deviceShader:0);}
            if(state.boundPixel&&!state.pixelConstants.empty())
            {if(!constants)return false;(void)constants(context,true,state.pixelConstants.data(),state.pixelConstants.size());}
        }
        if(args[0]==1)(void)draw(context,false,{1,args[3],args[2],0,0,0});
        else
        {
            constexpr std::array<std::uint32_t,5> types{1,1,4,5,2};
            (void)draw(context,true,{types[args[0]],args[3],0,args[4],args[1],args[2]});
        }
        return true;
    }
    bool spDXRenderer::DrawCachedAutomaticForAnalysis(DrawStateForAnalysis& state,
        const std::array<std::uint32_t,7>& args,spPCShaderManager& manager,
        const spDXShader::ConstantInputsForAnalysis& inputs,std::uint32_t color,
        const std::vector<std::uint32_t>* lightTypes,const std::array<std::uint32_t,8>& uv,
        DeviceShaderBindForAnalysis shader,DeviceShaderConstantsForAnalysis constants,
        DevicePrimitiveDrawForAnalysis draw,void* context)
    {
        return DrawAutomaticForAnalysis(state,args,manager,inputs,color,lightTypes,uv,shader,constants,draw,context);
    }
    bool spDXRenderer::DrawAutomaticForAnalysis(DrawStateForAnalysis& state,
        const std::array<std::uint32_t,7>& args,spPCShaderManager& manager,
        const spDXShader::ConstantInputsForAnalysis& inputs,std::uint32_t color,
        const std::vector<std::uint32_t>* lightTypes,const std::array<std::uint32_t,8>& uv,
        DeviceShaderBindForAnalysis shader,DeviceShaderConstantsForAnalysis constants,
        DevicePrimitiveDrawForAnalysis draw,void* context,const spPCShaderGenerationForAnalysis* generation)
    {
        if(state.vertex.top>=4)return false;
        if(state.vertex.entries[state.vertex.top].identity)
            return DrawPreselectedForAnalysis(state,args,shader,constants,draw,context);
        if(!inputs.material||!inputs.material->HasInitializedSpecularPowerForAnalysis())return false;
        const auto key=BuildShaderKeyForAnalysis(args[5],color,inputs.material->GetSpecularPowerForAnalysis(),lightTypes,uv);
        if(!key)return false;auto selected=manager.SelectCachedForAnalysis(*key);
        if(!selected.completed&&generation&&generation->compiler&&generation->state)
            selected=manager.SelectOrCreateForAnalysis(*key,*generation->compiler,*generation->state,generation->create,generation->release,generation->context);
        if(!selected.completed)return false;
        if(!selected.shader)return DrawResolvedForAnalysis(state,args,shader,constants,draw,context);
        std::vector<std::uint32_t> words;const auto rows=selected.shader->GetScalarWordsForAnalysis()[8];
        if(!selected.shader->BuildFullyWrittenConstantsForAnalysis(words,inputs,rows))return false;
        // Guard validation precedes mutation in the host. Actual original has
        // no safety check and copies all34-count rows from uninitialized stack.
        state.vertex.entries[state.vertex.top]={reinterpret_cast<std::uintptr_t>(selected.shader),selected.shader->GetDeviceShaderForAnalysis()};
        if(state.vertexConstants.size()<rows)state.vertexConstants.resize(rows);
        for(unsigned row=0;row<rows;++row)std::copy_n(words.begin()+row*4,4,state.vertexConstants[row].begin());
        //4BE210 retains the previous high-water row count/tail when rows shrink.
        const bool result=DrawResolvedForAnalysis(state,args,shader,constants,draw,context);
        if(state.vertex.top<4)state.vertex.entries[state.vertex.top]={};
        return result;
    }

    bool spDXRenderer::ApplyPassTextureStatesForAnalysis(PassTextureStateForAnalysis& state,
        const spMaterialPassLayer& pass,const std::array<const spMaterialTexture*,2>& fallbacks,
        const std::array<ResolvedTextureBindingForAnalysis,8>& bindings,const PassTextureOverridesForAnalysis* overrides,
        bool shaderActive,bool debugDisable,DeviceTextureBindForAnalysis bind,DevicePaletteSelectForAnalysis palette,
        TextureStateSubmitForAnalysis submit,void* context)
    {
        if(shaderActive)for(unsigned stage=0;stage<8;++stage)
        {
            if(state.cache[stage].transformFlags)state.cache[stage].raw[8]=0xffffffff;
            if(state.cache[stage].coordinateIndex!=stage)state.cache[stage].raw[7]=0xffffffff;
        }
        for(unsigned stage=0;stage<8;++stage)
        {
            const spMaterialTexture* texture=nullptr;std::uintptr_t identity=0;
            if(stage<pass.GetLayerCountForAnalysis())
            {
                const auto& layer=pass.GetLayerForAnalysis(stage);if(!layer||!layer->GetMaterialTextureForAnalysis())return false;
                texture=layer->GetMaterialTextureForAnalysis().get();identity=reinterpret_cast<std::uintptr_t>(texture->GetTextureForAnalysis());
                if(!layer->CopyTextureStatesForAnalysis(stage,state.desired[stage]))return false;
            }
            else
            {
                texture=fallbacks[stage?1:0];if(!texture)return false;
                const auto& values=texture->GetTextureStatesForAnalysis();std::copy_n(values.begin(),9,state.desired[stage].begin());
            }
            if(state.desiredTextures[stage]!=identity){state.desiredTextures[stage]=identity;state.dirty[stage]=1;}
        }
        for(unsigned stage=0;stage<8;++stage)if(state.dirty[stage])
        {
            if(bindings[stage].identity!=state.desiredTextures[stage])return false;
            if(!BindResolvedTextureForAnalysis(state.boundTextures[stage],state.palette,stage,bindings[stage],debugDisable,bind,palette,context))return false;
            state.dirty[stage]=0;
        }
        for(unsigned stage=0;stage<8;++stage)for(unsigned index=1;index<9;++index)
        {
            const auto* source=&state.desired;
            if(overrides)
            {
                const auto slot=overrides->selectors[stage][index];
                if(slot){if(!overrides->sources||slot>=overrides->count)return false;source=&overrides->sources[slot];}
            }
            const auto value=(*source)[stage][index];
            if(state.cache[stage].raw[index]!=value&&!ApplyTextureStateForAnalysis(state.cache[stage],stage,index,value,shaderActive,submit,context))return false;
        }
        return true;
    }
    bool spDXRenderer::ApplyPassBlendForAnalysis(MaterialStateWordsForAnalysis& raw,spDXMaterial& material,
        const spMaterialPassLayer& pass,const MaterialStateOverridesForAnalysis* overrides,RenderStateSubmitForAnalysis dispatch,void* context) noexcept
    {
        const auto blend=pass.GetFinalBlendOperationForAnalysis();(void)material.SetRenderStateForAnalysis(7,blend);
        std::uint32_t desired=blend;
        if(overrides)
        {
            const auto slot=overrides->selectors[6];
            if(slot){if(!overrides->sources||slot>=overrides->count||!overrides->sources[slot])return false;desired=(*overrides->sources[slot])[7];}
        }
        return raw[7]==desired||ApplyMaterialRenderStateForAnalysis(raw[7],7,desired,dispatch,context);
    }

    bool spDXRenderer::SelectPaletteForAnalysis(std::uint32_t& cachedIndex,
        std::optional<std::uint32_t> index,DevicePaletteSelectForAnalysis call,void* context) noexcept
    {
        if(!index||cachedIndex==*index)return true;
        if(!call)return false;
        cachedIndex=*index;(void)call(context,*index);return true;
    }
    bool spDXRenderer::BindResolvedTextureForAnalysis(std::uintptr_t& cachedIdentity,
        std::uint32_t& cachedPalette,std::uint32_t stage,const ResolvedTextureBindingForAnalysis& input,
        bool debugDisable,DeviceTextureBindForAnalysis bind,DevicePaletteSelectForAnalysis palette,void* context) noexcept
    {
        const auto identity=debugDisable?0:input.identity;
        if(cachedIdentity==identity)return true;
        if(!bind)return false;
        (void)bind(context,stage,identity?input.deviceTexture:0);
        if(identity&&!SelectPaletteForAnalysis(cachedPalette,input.paletteIndex,palette,context))return false;
        cachedIdentity=identity;return true;
    }

    bool spDXRenderer::InstallMaterialForAnalysis(LightingStateForAnalysis& state,
        spDXMaterial*& borrowedOwner,spDXMaterial* material,std::uint32_t frame,bool* evaluated)
    {
        if(evaluated)*evaluated=false;
        if(!material||!material->HasInitializedSpecularPowerForAnalysis())return false;
        borrowedOwner=material;
        state.diffuse=material->GetDiffuseColorForAnalysis();state.ambient=material->GetAmbientColorForAnalysis();
        state.specular=material->GetSpecularColorForAnalysis();state.emissive=material->GetEmissiveColorForAnalysis();
        state.specularPower=material->GetSpecularPowerForAnalysis();
        return material->UpdateColorForFrameForAnalysis(frame,false,evaluated);
    }
    bool spDXRenderer::ApplyMaterialStateSetForAnalysis(MaterialStateWordsForAnalysis& raw,
        const MaterialStateWordsForAnalysis& material,const MaterialStateOverridesForAnalysis* overrides,
        LightingStateForAnalysis& lighting,RenderStateSubmitForAnalysis dispatch,void* context) noexcept
    {
        raw[8]=255;
        for(std::uint32_t index=1;index<11;++index)
        {
            const auto* source=&material;
            if(overrides)
            {
                const auto slot=overrides->selectors[index-1];
                if(slot)
                {
                    if(!overrides->sources||slot>=overrides->count||!overrides->sources[slot])return false;
                    source=overrides->sources[slot];
                }
            }
            const auto value=(*source)[index];
            if(raw[index]!=value&&!ApplyMaterialRenderStateForAnalysis(raw[index],index,value,dispatch,context,&lighting))return false;
        }
        return true;
    }

    bool spDXRenderer::BeginSceneForAnalysis(SceneStateForAnalysis& state,DeviceSceneCallForAnalysis call,void* context) noexcept
    {
        if(!call)return false; // native unconditionally requires a device
        (void)call(context,true);state.activeCBC0=1;state.resetWord4C=0;return true;
    }
    bool spDXRenderer::EndSceneForAnalysis(SceneStateForAnalysis& state,DeviceSceneCallForAnalysis call,void* context) noexcept
    {
        if(!call)return false;
        (void)call(context,false);++state.counter40;state.activeCBC0=0;return true;
    }
    bool spDXRenderer::ClearForAnalysis(std::uint32_t flags,std::uint32_t argb,std::uint32_t stencil,DeviceClearForAnalysis call,void* context) noexcept
    {
        if(!call)return false;
        (void)call(context,flags&7u,argb,1.F,stencil);return true; // zero rectangles, depth1, HRESULT ignored
    }
    spDXRenderer::PresentBoundaryForAnalysis spDXRenderer::PresentBeforeResetForAnalysis(
        std::uint32_t inhibit,const std::array<std::uint32_t,8>& parameters,DevicePresentForAnalysis call,void* context) noexcept
    {
        if(inhibit)return {true,false,false,{}};
        if(!call)return {};
        if(static_cast<std::uint32_t>(call(context,false))==0x88760868u
            &&static_cast<std::uint32_t>(call(context,true))==0x88760869u)return {false,false,true,parameters};
        return {true,true,false,{}}; // native ignores other HRESULTs
    }

    std::shared_ptr<spPCVertexDeclaration> spDXRenderer::GetVertexDeclarationForAnalysis(
        const std::uint32_t componentFlags)
    {
        const auto found=vertexDeclarations_.find(componentFlags);
        if(found!=vertexDeclarations_.end())return found->second;
        try
        {
            auto declaration=std::make_shared<spPCVertexDeclaration>();
            if(!declaration->InitializeForAnalysis(componentFlags))return nullptr;
            vertexDeclarations_.emplace(componentFlags,declaration);
            return declaration;
        }
        catch(...){return nullptr;} // explicit host allocation guard
    }

    std::unique_ptr<spBaseObject> spDXRenderer::vfunc_10(spCloneManager&) const
    {
        // Native spDXRenderer remains abstract and has no RTTI factory.
        return nullptr;
    }

    const spRTTIRecord& spDXRenderer::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }

    bool spDXRenderer::ApplyTextureTransform4ForAnalysis(
        TextureMatrix4ForAnalysis& cache,const std::uint32_t stage,const TextureMatrix4ForAnalysis& matrix,
        const TextureTransformSubmitForAnalysis submit,void* context) noexcept
    {
        if(!submit)return false; // host-only rejection, native requires device
        cache=matrix;
        (void)submit(context,stage+16u,matrix); // native uint32 wrap, HRESULT ignored
        return true;
    }

    bool spDXRenderer::ApplyTextureTransform3ForAnalysis(
        TextureMatrix4ForAnalysis& cache,const std::uint32_t stage,const TextureMatrix3ForAnalysis& matrix,
        const TextureTransformSubmitForAnalysis submit,void* context) noexcept
    {
        const TextureMatrix4ForAnalysis expanded{
            matrix[0],matrix[1],matrix[2],0,matrix[3],matrix[4],matrix[5],0,
            matrix[6],matrix[7],matrix[8],0,0,0,0,1};
        return ApplyTextureTransform4ForAnalysis(cache,stage,expanded,submit,context);
    }

    bool spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(
        std::uint32_t& cachedEntry, const std::uint32_t index, const std::uint32_t value,
        const RenderStateSubmitForAnalysis submit, void* context) noexcept
    {
        if (cachedEntry == value)
            return true;
        if (!submit)
            return false; // host-only; native dereferences device unconditionally
        (void)submit(context, index, value);
        cachedEntry = value;
        return true;
    }

    bool spDXRenderer::ApplyMaterialRenderStateForAnalysis(std::uint32_t& cached,
        std::uint32_t index,std::uint32_t value,RenderStateSubmitForAnalysis dispatch,void* context,
        LightingStateForAnalysis* lighting) noexcept
    {
        cached=value; // actual store precedes dispatch/index rejection
        if(index<1||index>10)return false;
        if(index==8)return lighting&&ApplyMaterialLightingForAnalysis(*lighting,value,dispatch,context);
        if((index==2&&value>1)||(index==3&&value>2)||((index==6||index==10)&&value>7))return false;
        if(index==7&&(value==5||value>6))return true; // real no-op modes
        if(!dispatch)return false; // host guard
        auto emit=[&](std::uint32_t state,std::uint32_t mapped){(void)dispatch(context,state,mapped);};
        switch(index)
        {
        case 1:emit(8,value==1?2:3);break;
        case 2:emit(9,value+1);break;
        case 3:emit(22,value+1);break;
        case 4:emit(7,value==1?1:0);break;
        case 5:emit(14,value==1?1:0);break;
        case 6:emit(23,value+1);break;
        case 7:
        {
            constexpr std::array<std::array<std::uint32_t,2>,7> blend{{{2,1},{9,1},{5,6},{1,4},{2,2},{0,0},{5,2}}};
            emit(19,blend[value][0]);emit(20,blend[value][1]);break;
        }
        case 9:emit(24,value);break;
        case 10:emit(25,value+1);break;
        default:return false;
        }
        return true; // original ignores lower-level HRESULT/result
    }

    bool spDXRenderer::ApplyMaterialColorSourceForAnalysis(std::uint32_t& rawSource,
        bool ambient,std::uint32_t value,RenderStateSubmitForAnalysis dispatch,void* context) noexcept
    {
        if(rawSource==value)return true;
        rawSource=value;
        if(value<10||value>12)return true;
        if(!dispatch)return false;
        (void)dispatch(context,ambient?148u:145u,value-10);return true;
    }

    bool spDXRenderer::ApplyMaterialLightingForAnalysis(LightingStateForAnalysis& state,
        std::uint32_t mode,RenderStateSubmitForAnalysis dispatch,void* context) noexcept
    {
        if(!dispatch)return false;
        (void)dispatch(context,29,mode>=3&&mode<=5&&state.specularPower>0.F?1u:0u);
        if(mode>7)return true;
        const auto black=PCARGBToRGBAForAnalysis(state.globalBlackARGB);
        if(mode==0)state.emissive={1,1,1,1};
        else if(mode==1)
        {
            state.emissive=state.diffuse;const float alpha=state.diffuse[3];
            state.diffuse=state.ambient=state.specular=black;state.diffuse[3]=alpha;
        }
        else if(mode==6)
        {state.emissive=PCARGBToRGBAForAnalysis(state.packedColorC194);state.diffuse=state.ambient=state.specular=black;}
        (void)dispatch(context,137,mode==2?0u:1u);
        (void)ApplyMaterialColorSourceForAnalysis(state.diffuseSource,false,mode==2||mode==5?11u:10u,dispatch,context);
        (void)ApplyMaterialColorSourceForAnalysis(state.ambientSource,true,mode==4?11u:10u,dispatch,context);
        return true;
    }

    bool spDXRenderer::ApplyTextureStateForAnalysis(TextureStageCacheForAnalysis& cache,
        std::uint32_t stage,std::uint32_t index,std::uint32_t value,bool overrideCoordinates,
        TextureStateSubmitForAnalysis submit,void* context) noexcept
    {
        if(stage>=8||index>=cache.raw.size())return false; // host bounds
        cache.raw[index]=value;
        if(index==0)return true;
        if((index==1||index==2)&&value>=16)return false; // unbounded native table guarded
        if(!submit)return false;
        auto emit=[&](bool sampler,std::uint32_t state,std::uint32_t mapped){(void)submit(context,sampler,stage,state,mapped);};
        switch(index)
        {
        case 1:case 2:
        {
            constexpr std::array<std::uint32_t,16> operation{1,2,3,4,5,6,7,10,13,12,15,16,18,19,20,21};
            emit(false,index==1?1:4,operation[value]);break;
        }
        case 3:case 4:emit(true,index-2,value==0?1:value==1?2:3);break;
        case 5:emit(true,4,value);break;
        case 6:
        {
            const auto filter=value<=3?value:0u;
            emit(true,7,filter==3?2:filter);emit(true,5,filter?filter:1);emit(true,6,filter?filter:1);break;
        }
        case 7:
        {
            constexpr std::array<std::uint32_t,13> generation{0,1,2,3,4,5,6,7,0x10000,0x30000,0x20000,0x30000,0x40000};
            auto mapped=value<generation.size()?generation[value]:0u;
            if(overrideCoordinates)cache.raw[index]=mapped=stage;
            if(cache.coordinateIndex!=mapped){emit(false,11,mapped);cache.coordinateIndex=mapped;}break;
        }
        case 8:
        {
            const auto low=value&7u;auto mapped=low<=4?low:stage;
            if(value&8u)mapped|=0x100u;
            if(overrideCoordinates)mapped=0;
            if(cache.transformFlags!=mapped){emit(false,24,mapped);cache.transformFlags=mapped;}break;
        }
        default:return false;
        }
        return true;
    }
} // namespace sparkplug::reconstruction
