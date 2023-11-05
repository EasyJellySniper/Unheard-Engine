#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

struct Panorama2CubemapOutput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float3 SpherePos : TEXCOORD1;
};

static const float2 GTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

cbuffer Panorama2CubemapConstant : register(b0)
{
    int GOutputFaceIndex;
    uint GInputMipmapIndex;
};

Texture2D InputTexture : register(t1);
SamplerState LinearClampedSampler: register(s2);

// convert UV to xyz and back, based on https://en.wikipedia.org/wiki/Cube_mapping
void ConvertUVtoXYZ(float2 UV, out float3 Pos, int Index)
{
    // convert range 0 to 1 to -1 to 1
    float Uc = 2.0f * UV.x - 1.0f;
    float Vc = 2.0f * UV.y - 1.0f;
    
    if (Index == 0)
    {
        Pos.x = 1.0f;
        Pos.y = Vc;
        Pos.z = -Uc;
    }
    else if (Index == 1)
    {
        Pos.x = -1.0f;
        Pos.y = Vc;
        Pos.z = Uc;
    }
    else if (Index == 2)
    {
        Pos.x = Uc;
        Pos.y = 1.0f;
        Pos.z = -Vc;
    }
    else if (Index == 3)
    {
        Pos.x = Uc;
        Pos.y = -1.0f;
        Pos.z = Vc;
    }
    else if (Index == 4)
    {
        Pos.x = Uc;
        Pos.y = Vc;
        Pos.z = 1.0f;
    }
    else if (Index == 5)
    {
        Pos.x = -Uc;
        Pos.y = Vc;
        Pos.z = -1.0f;
    }
}

float2 ConvertSpherePosToUV(float3 SpherePos, int Index, float2 QuadUV)
{
    // convert to sphere coordinate based on https://en.wikipedia.org/wiki/Spherical_coordinate_system definition
    // x = sin(theta * v) * cos(phi * u)
    // y = sin(theta * v) * sin(phi * u)
    // z = cos(theta * v)
    // theta = [0, PI], and phi = [0, 2PI]
    float Theta = UH_PI;
    float Phi = UH_PI * 2.0f;
    
    // flip the position to the (ISO 80000-2:2019) definition
    float3 Pos;
    Pos.x = -SpherePos.z;
    Pos.y = SpherePos.x;
    Pos.z = SpherePos.y;
    Pos = normalize(Pos);
    
    // so given a sphere pos, I just need to solve the equation above to get the UV
    float2 UV = 0;
    
    if (Index < 2)
    {
        UV.y = acos(Pos.z) / Theta;
        UV.x = acos(Pos.x / sin(Theta * UV.y)) / Phi;
    }
    else if (Index < 4)
    {
        UV.y = acos(Pos.z) / Theta;
        
        float Val = Pos.x / sin(Theta * UV.y);
        
        // be careful for UV.x since it could go singularity, clamp the value between[-1,1]
        Val = clamp(Val, -1.0f, 1.0f);
        UV.x = acos(Val) / Phi;
        
        if (QuadUV.x < 0.5f)
        {
            UV.x = 1.0f - UV.x;
        }
    }
    else
    {
        UV.y = acos(Pos.z) / Theta;
        UV.x = asin(Pos.y / sin(Theta * UV.y)) / Phi;
    }
    
    // flip to proper direction, following the sign in ConvertUVtoXYZ()
    if (Index == 0 || Index == 5)
    {
        UV.x = 1.0f - UV.x;
    }
    
    // assume input width & height ratio is 2:1, adjust the U for face -Y+Y+Z
    if (Index >= 2 && Index <= 4)
    {
        UV.x += 0.5f;
    }
   
    return UV;
}

// almost the same as the post process output, but add the selection of render target index
Panorama2CubemapOutput PanoramaToCubemapVS(uint Vid : SV_VertexID)
{
    Panorama2CubemapOutput Vout;

    Vout.UV = GTexCoords[Vid];
    Vout.Position = float4(2.0f * Vout.UV.x - 1.0f, 2.0f * Vout.UV.y - 1.0f, 1.0f, 1.0f);
    
    return Vout;
}

float4 PanoramaToCubemapPS(Panorama2CubemapOutput Vin) : SV_Target
{    
    // inverse the UV.y for Y faces
    float2 QuadUV = Vin.UV;
    if (GOutputFaceIndex != 2 && GOutputFaceIndex != 3)
    {
        QuadUV.y = 1.0f - QuadUV.y;
    }
    
    // the goal is to solve the "original UV" of InputTexture and sampling from it
    // so the first thing to do is reconstruct the sphere pos based on the quad UV and the requested cube face
    float3 SpherePos = 0;
    ConvertUVtoXYZ(QuadUV, SpherePos, GOutputFaceIndex);
    
    // the second step, calculate the original uv with spherical formula
    float2 UV = ConvertSpherePosToUV(SpherePos, GOutputFaceIndex, QuadUV);
    UV = fmod(UV, 1.0f);
    
    return InputTexture.SampleLevel(LinearClampedSampler, UV, GInputMipmapIndex);
}