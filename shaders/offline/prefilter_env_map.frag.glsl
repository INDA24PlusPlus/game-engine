// Based upon code from "Vulkan 3D Graphics Rendering Cookbook - Second Edition" by Sergey Kosarevsky, Alexey Medvedev, Viktor Latypov
// Modified by vidar-h
//
// MIT License
//
// Copyright (c) 2024 Packt
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#version 460 core

#define MATH_PI 3.1415926535897932384626433832795

layout(location = 0)
out vec4 o_color;

layout(location = 0)
in vec3 local_pos;

layout(binding = 0)
uniform samplerCube env_map;

// These will be Push Constants when we switch to Vulkan se let them be uniforms for now.
layout(location = 0)
uniform uint sample_count;

layout(location = 1)
uniform uint distribution;

layout(location = 2)
uniform float roughness;

layout(location = 3)
uniform uint width;

layout(location = 4)
uniform uint height;


const int cLambertian = 0;
const int cGGX = 1;

// hammersley2d describes a sequence of points in the 2d unit square [0,1)^2 that can be used for quasi Monte Carlo integration
vec2 hammersley2d(uint i, uint N) {
    // radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return vec2(float(i) / float(N), rdi);
}

struct MicrofacetDistributionSample {
    float pdf;
    float cosTheta;
    float sinTheta;
    float phi;
};

float D_GGX(float NdotH, float roughness) {
    float a = NdotH * roughness;
    float k = roughness / (1.0 - NdotH * NdotH + a * a);
    return k * k * (1.0 / MATH_PI);
}

// GGX microfacet distribution
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html
// This implementation is based on https://bruop.github.io/ibl/,
//  https://www.tobias-franke.eu/log/2014/03/30/notes_on_importance_sampling.html
// and https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html
MicrofacetDistributionSample GGX(vec2 xi, float roughness) {
    MicrofacetDistributionSample ggx;

    // evaluate sampling equations
    float alpha = roughness * roughness;
    ggx.cosTheta = clamp(sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y)), 0.0, 1.0);
    ggx.sinTheta = sqrt(1.0 - ggx.cosTheta * ggx.cosTheta);
    ggx.phi = 2.0 * MATH_PI * xi.x;

    // evaluate GGX pdf (for half vector)
    ggx.pdf = D_GGX(ggx.cosTheta, alpha);

    // Apply the Jacobian to obtain a pdf that is parameterized by l see https://bruop.github.io/ibl/
    // Typically you'd have the following:
    //   float pdf = D_GGX(NoH, roughness) * NoH / (4.0 * VoH);
    // but since V = N => VoH == NoH
    ggx.pdf /= 4.0;

    return ggx;
}

MicrofacetDistributionSample Lambertian(vec2 xi, float roughness) {
    MicrofacetDistributionSample lambertian;

    // Cosine weighted hemisphere sampling
    // http://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations.html#Cosine-WeightedHemisphereSampling
    lambertian.cosTheta = sqrt(1.0 - xi.y);
    lambertian.sinTheta = sqrt(xi.y); // equivalent to `sqrt(1.0 - cosTheta*cosTheta)`;
    lambertian.phi = 2.0 * MATH_PI * xi.x;

    lambertian.pdf = lambertian.cosTheta / MATH_PI; // evaluation for solid angle, therefore drop the sinTheta

    return lambertian;
}

// TBN generates a tangent bitangent normal coordinate frame from the normal (the normal must be normalized)
mat3 generateTBN(vec3 normal) {
    vec3 bitangent = vec3(0.0, 1.0, 0.0);

    float NdotUp = dot(normal, vec3(0.0, 1.0, 0.0));
    float epsilon = 0.0000001;
    if (1.0 - abs(NdotUp) <= epsilon) {
        // Sampling +Y or -Y, so we need a more robust bitangent.
        bitangent = (NdotUp > 0.0) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 0.0, -1.0);
    }

    vec3 tangent = normalize(cross(bitangent, normal));
    bitangent = cross(normal, tangent);

    return mat3(tangent, bitangent, normal);
}

// getImportanceSample returns an importance sample direction with pdf in the .w component
vec4 getImportanceSample(uint sampleIndex, vec3 N, float roughness) {
    // generate a quasi monte carlo point in the unit square [0.1)^2
    vec2 xi = hammersley2d(sampleIndex, sample_count);

    MicrofacetDistributionSample importanceSample;

    // generate the points on the hemisphere with a fitting mapping for
    // the distribution (e.g. lambertian uses a cosine importance)
    if (distribution == cLambertian) {
        importanceSample = Lambertian(xi, roughness);
    }
    else if (distribution == cGGX) {
        // Trowbridge-Reitz / GGX microfacet model (Walter et al)
        // https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html
        importanceSample = GGX(xi, roughness);
    }

    // transform the hemisphere sample to the normal coordinate frame
    // i.e. rotate the hemisphere to the normal direction
    vec3 localSpaceDirection = normalize(vec3(
                importanceSample.sinTheta * cos(importanceSample.phi),
                importanceSample.sinTheta * sin(importanceSample.phi),
                importanceSample.cosTheta));
    mat3 TBN = generateTBN(N);
    vec3 direction = TBN * localSpaceDirection;

    return vec4(direction, importanceSample.pdf);
}

// Mipmap Filtered Samples (GPU Gems 3, 20.4)
// https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-20-gpu-based-importance-sampling
// https://cgg.mff.cuni.cz/~jaroslav/papers/2007-sketch-fis/Final_sap_0073.pdf
float computeLod(float pdf) {
    // // Solid angle of current sample -- bigger for less likely samples
    // float omegaS = 1.0 / (float(perFrameData.sampleCount) * pdf);
    // // Solid angle of texel
    // // note: the factor of 4.0 * MATH_PI
    // float omegaP = 4.0 * MATH_PI / (6.0 * float(perFrameData.width) * float(perFrameData.width));
    // // Mip level is determined by the ratio of our sample's solid angle to a texel's solid angle
    // // note that 0.5 * log2 is equivalent to log4
    // float lod = 0.5 * log2(omegaS / omegaP);

    // babylon introduces a factor of K (=4) to the solid angle ratio
    // this helps to avoid undersampling the environment map
    // this does not appear in the original formulation by Jaroslav Krivanek and Mark Colbert
    // log4(4) == 1
    // lod += 1.0;

    // We achieved good results by using the original formulation from Krivanek & Colbert adapted to cubemaps

    // https://cgg.mff.cuni.cz/~jaroslav/papers/2007-sketch-fis/Final_sap_0073.pdf
    float sampleCount = float(sample_count);
    float lod = 0.5 * log2(6.0 * float(width) * float(height) / (sampleCount * pdf));

    return lod;
}

vec3 filterColor(vec3 N) {
    vec3 color = vec3(0.f);
    float weights = 0.0f;

    for (uint i = 0; i < sample_count; i++) {
        vec4 importanceSample = getImportanceSample(i, N, roughness);

        vec3 H = vec3(importanceSample.xyz);
        float pdf = importanceSample.w;

        // mipmap filtered samples (GPU Gems 3, 20.4)
        float lod = computeLod(pdf);

        if (distribution == cLambertian) {
            // sample lambertian at a lower resolution to avoid fireflies
            //vec3 lambertian = textureBindlessCubeLod(perFrameData.envMap, perFrameData.samplerIdx, H, lod).xyz;
            vec3 lambertian = textureLod(env_map, H, lod).xyz;

            color += lambertian;
        }
        else if (distribution == cGGX) {
            // Note: reflect takes incident vector.
            vec3 V = N;
            vec3 L = normalize(reflect(-V, H));
            float NdotL = dot(N, L);

            if (NdotL > 0.0) {
                if (roughness == 0.0) {
                    // without this the roughness=0 lod is too high
                    lod = 0.0;
                }
                //vec3 sampleColor = textureBindlessCubeLod(perFrameData.envMap, perFrameData.samplerIdx, L, lod).xyz;
                vec3 sampleColor = textureLod(env_map, L, lod).xyz;
                color += sampleColor * NdotL;
                weights += NdotL;
            }
        }
    }

    color /= (weights != 0.0f) ? weights : float(sample_count);

    return color.rgb;
}

void main() {
    vec3 dir = normalize(local_pos);
    o_color = vec4(filterColor(dir), 1.0);
}
