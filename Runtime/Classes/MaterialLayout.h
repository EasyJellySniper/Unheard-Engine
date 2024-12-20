#pragma once
#include "../Engine/RenderResource.h"
#include <vector>
#include <string>

// hard code variables in shader
const std::string GDefaultSamplerIndexName = "GDefaultAnisoSamplerIndex";
const std::string GDefaultTextureChannel0Name = "UV0";
const int32_t GRTMaterialDataStartIndex = 3;
const int32_t GSystemPreservedTextureSlots = 2;

// ------------------------------ header for material layout defines

// cull mode, can be used as graphic api value
// the order here is following VkCullModeFlags
enum class UHCullMode
{
	CullNone,
	CullFront,
	CullBack,
	CullModeMax
};

// blend mode
enum class UHBlendMode
{
	Opaque = 0,
	Masked,
	TranditionalAlpha,
	Addition,
	BlendModeMax
};

// UH material inputs used for both runtime and editor
enum class UHMaterialInputs
{
	Diffuse = 0,
	Occlusion,
	Specular,
	Normal,
	Opacity,
	Metallic,
	Roughness,
	FresnelFactor,
	Emissive,
	Refraction,
	MaterialMax
};

enum class UHMaterialCompileFlag
{
	UpToDate,
	FullCompileTemporary,
	BindOnly,
	FullCompileResave,
	IncludeChanged,
	StateChangedOnly,
	RendererMaterialChanged
};

struct UHMaterialUsage
{
	bool bIsTangentSpace = false;
	bool bUseRefraction = false;
};

enum class UHMaterialFeatureBits
{
	MaterialTangentSpace = 1 << 0,
	MaterialRefraction = 1 << 1,
};