// material commons
#ifndef UHMATERIALCOMMON_H
#define UHMATERIALCOMMON_H

#define UH_ISOPAQUE 0
#define UH_ISMASKED 1
#define UH_ISALPHABLEND 2
#define UH_ISADDITION 3

#define UH_OPAQUE_MASK 1.0f / 3.0f
#define UH_TRANSLUCENT_MASK 2.0f / 3.0f

// material feature bits
#define UH_TANGENT_SPACE 1 << 0
#define UH_REFRACTION 1 << 1

// material inputs from graph system
struct UHMaterialInputs
{
    float3 Diffuse;
    float Occlusion;
    float3 Specular;
    float3 Normal;
    float Opacity;
    float Metallic;
    float Roughness;
    float FresnelFactor;
    float ReflectionFactor;
    float3 Emissive;
    float Refraction;
};

#endif