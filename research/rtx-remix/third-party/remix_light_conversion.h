/*
* Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

// Adapted from NVIDIA dxvk-remix b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4.
// CPU-only subset: explicit gain, original default least-squares branch, no GPU
// dependencies. Formula is Remix policy, not reconstructed Winx/Sparkplug code.
#pragma once
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <d3d9.h>
namespace remix_light_conversion {
constexpr float kLegacyLightEndValue=1.0f/255.0f;
constexpr float kNewLightEndValue=0.01f;
constexpr float kPi=3.14159265358979323846f;
static float leastSquareIntensity(float intensity, float attenuation2, float attenuation1, float attenuation0, float range) {
  // Calculate the distance where light intensity attenuates to 10%
  constexpr float kEpsilon = 0.000001f;
  float lowRange = 0.0f;
  const float lowThreshold = 0.1f;
  if (attenuation2 < kEpsilon) {
    if (attenuation1 > kEpsilon) {
      lowRange = (1.0f / lowThreshold - attenuation0) / attenuation1;
    }
  } else {
    float a = attenuation2;
    float b = attenuation1;
    float c = attenuation0 - 1.0f / lowThreshold;
    float discriminant = static_cast<float>(b * b - 4.0 * a * c);
    if (discriminant >= 0) {
      const float sqRoot = std::sqrt(discriminant);
      const float root1 = (-b + sqRoot) / (2 * a);
      const float root2 = (-b - sqRoot) / (2 * a);
      if (root1 > 0) {
        lowRange = root1;
      }
      if (root2 > 0) {
        lowRange = root2;
      }
    }
  }

  // Calculate the sample range
  if (lowRange > 0) {
    range = (std::min)(range, lowRange);
  }

  // Place 5 samples between [0, range]
  // Find newIntensity to minimize the error = Sigma((intensity / (a2*xi*xi + a1*xi + a0) - newIntensity / (xi * xi))^2)
  const int kSamples = 5;
  float numerator = 0;
  float denominator = 0;

  for (int i = 0; i < kSamples; i++) {
    float xi = float(i + 1) / kSamples * range;
    float xi2 = xi * xi;
    float xi4 = xi2 * xi2;
    float Ii = intensity / (attenuation2 * xi2 + attenuation1 * xi + attenuation0);
    numerator += Ii / xi2;
    denominator += 1 / xi4;
  }

  float newIntensity = numerator / denominator;
  return newIntensity;
}

static float intensityToEndDistance(float intensity) {
  float endDistanceSq = intensity / kLegacyLightEndValue;
  return static_cast<float>(sqrt(endDistanceSq));
}

float solveQuadraticEndDistance(float originalBrightness, float attenuation2, float attenuation1, float attenuation0, float range) {
  const float a = attenuation2;
  const float b = attenuation1;
  const float c = attenuation0;
  float endDistance = 0.0f;

  // Solve for kLegacyLightEndValue using quadratic equation.
  // originalBrightness/(a*d*d+b*d+c) = kLegacyLightEndValue
  // a*d*d+b*d+c-originalBrightness/kLegacyLightEndValue = 0
  const float newC = c - originalBrightness / kLegacyLightEndValue;
  const float discriminant = b * b - 4 * a * newC;

  if (discriminant < 0) {
    // Attenuation never reaches kLegacyLightEndValue.  Just use range.
    endDistance = range;
  } else if (discriminant == 0) {
    const float root = -b / (2 * a);
    if (root > 0) {
      endDistance = root;
    }
  } else {
    // Two roots, use the smaller positive root.
    const float sqRoot = std::sqrt(discriminant);
    const float root1 = (-b + sqRoot) / (2 * a);
    const float root2 = (-b - sqRoot) / (2 * a);
    if (root1 > 0) {
      endDistance = root1;
    }
    if (root2 > 0) {
      endDistance = root2;
    }
  }

  return endDistance;
}

static float calculateIntensity(const D3DLIGHT9& light, const float radius, float gain) {
  constexpr float kEpsilon = 0.000001f;

  // Calculate max distance based on attenuation.  We're looking to find when the light's attenuation is kLegacyLightEndValue.
  // Attenuation in D3D9 for lights is calculated as 1/(light.Attenuation2*d*d + light.Attenuation1*d + light.Attenuation).
  // This is calculated with respect to the max component of the light's 3 color components, and then is translated to RGB with the normalized color later.
  // Note that the calculated max distance may be greater than the Light's original "Range" value. This is because often times in older games the
  // Range was merely used in conjunction with a custom large color value and attenuation curve as an optimization to keep very bright lights from extending
  // across the entire level when only needed in a small area, but physical lights must reflect the "intended" full max distance as calculated by the attenuation.
  const float a = light.Attenuation2;
  const float b = light.Attenuation1;
  const float c = light.Attenuation0;

  const float originalBrightness = (std::max)(light.Diffuse.r, (std::max)(light.Diffuse.g, light.Diffuse.b));

  float endDistance = light.Range;

  if (c > 0 && originalBrightness / c < kLegacyLightEndValue) {
    // Light constant is already lower than our minimum right next to the light, so just set the radiance to 0.
    endDistance = 0.f;
  } else if (a < kEpsilon) {
    // No squared attenuation term
    if (b > kEpsilon) {
      // linear falloff
      if (true) {
        endDistance = intensityToEndDistance(leastSquareIntensity(originalBrightness, light.Attenuation2, light.Attenuation1, light.Attenuation0, light.Range));
      } else {
        // 1/(b*d + c) = kLegacyLightEndValue
        endDistance = ((originalBrightness / kLegacyLightEndValue) - c) / b;
      }
    } else {
      // No falloff - the light is at full power * c until the range runs out
      // TODO may want to do something different here - the light is still fully bright at light.Range...
    }
  } else {
    if (true) {
      endDistance = intensityToEndDistance(leastSquareIntensity(originalBrightness, light.Attenuation2, light.Attenuation1, light.Attenuation0, light.Range));
    } else {
      endDistance = solveQuadraticEndDistance(originalBrightness, light.Attenuation2, light.Attenuation1, light.Attenuation0, light.Range);
    }
  }

  // Calculate the radiance of the Sphere light to reach the desired perceptible radiance threshold at the calculated range of the D3D light.
  const float endDistanceSq = endDistance * endDistance;

  // Conversion factor from a desired distance squared to a radiance value based on a desired fixed light radius and the desired ending radiance value.
  // Derivation:
  // t = Threshold (ending) radiance value
  // i = Point Light Intensity
  // d = Distance
  // p = Power
  // r = Radiance
  //
  // i / d^2 = t (Inverse square law for intensity, solving for d to find the intensity of a point light to reach this radiance threshold)
  // p = i * 4 * pi (Point Light Intensity to Power)
  // r = p / ((4 * pi * r^2) * pi) (Power to Sphere Light Radiance)
  // r = (d^2 * t) / (pi * r^2) (Solve and Substitute)
  const float kDistanceSqToRadiance = kNewLightEndValue / (kPi * radius * radius);

  return (std::min)(kDistanceSqToRadiance * endDistanceSq * gain, FLT_MAX);
}


}
