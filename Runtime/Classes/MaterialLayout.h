#pragma once
#include "../Engine/RenderResource.h"
#include <vector>
#include <string>

// hard code variables in shader
const std::string GDefaultSamplerName = "DefaultAniso16";
const std::string GDefaultTextureChannel0Name = "UV0";

// ------------------------------ header for material layout defines

// cull mode, can be used as graphic api value
// the order here is following VkCullModeFlags
enum UHCullMode
{
	CullNone,
	CullFront,
	CullBack,
	CullModeMax
};

// blend mode
enum UHBlendMode
{
	Opaque = 0,
	Masked,
	TranditionalAlpha,
	Addition,
	BlendModeMax
};

// constant types
enum UHConstantTypes
{
	System = 0,
	Object,
	Material,
	ConstantTypeMax
};

// UH material inputs used for both runtime and editor
enum UHMaterialInputs
{
	Diffuse = 0,
	Occlusion,
	Specular,
	Normal,
	Opacity,
	Metallic,
	Roughness,
	FresnelFactor,
	ReflectionFactor,
	Emissive,
	Refraction,
	MaterialMax
};

enum UHMaterialCompileFlag
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
	bool bIsSkybox = false;
	bool bIsTangentSpace = false;
	bool bUseRefraction = false;
};

class UHTextureCube;
struct UHSystemMaterialData
{
	int32_t DefaultSamplerIndex;
	UHTextureCube* InEnvCube;
	int32_t RefractionClearIndex;
	int32_t RefractionBlurredIndex;
};