#pragma once
#include "ShaderClass.h"

// texture and sampler table for bindless rendering

// texture table
class UHTextureTable : public UHShaderClass
{
public:
	UHTextureTable(UHGraphic* InGfx, std::string Name, uint32_t NumTexes);

	virtual void OnCompile() override {}

	uint32_t GetNumTexes() const;
	static uint32_t MaxNumTexes;
private:
	uint32_t NumTexes;
};

// sampler table
class UHSamplerTable : public UHShaderClass
{
public:
	UHSamplerTable(UHGraphic* InGfx, std::string Name, uint32_t NumSamplers);

	virtual void OnCompile() override {}

	uint32_t GetNumSamplers() const;

private:
	uint32_t NumSamplers;
};