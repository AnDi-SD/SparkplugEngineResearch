#pragma once
// Own bounded regression using the unchanged renderer's actual CPU/GPU header.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <array>
#include <stdexcept>
#include <cmath>
#include "rtx/pass/gpu_skinning_binding_indices.h"
#include "rtx/pass/skinning.h"
#include "source_expressions.h"
#include "Sparkplug/Analysis/PC/spFixedShaderSkinning.h"
// Own test logging sink. Every production math diagnostic fails the fixture;
// no algorithm or success path is replaced, and no renderer log backend links.
void dxvk::Logger::err(const std::string& message){throw std::runtime_error("renderer math diagnostic: "+message);}
