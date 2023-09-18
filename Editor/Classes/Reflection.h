#pragma once

#if WITH_EDITOR

#include <string>
#include <vector>

struct UHReflectionTypeDescriptor
{
	UHReflectionTypeDescriptor()
	{

	}

	// stores the member name and type
	std::vector<std::string> MemberName;
	std::vector<std::string> MemberType;
};

// reflection system for UHE
// for now it shall only record all public properties, which include member name and the type
class UHReflection
{
public:
	template <typename T>
	static UHReflectionTypeDescriptor Get()
	{
		return T::Reflection();
	}
};

#define UH_BEGIN_REFLECT static UHReflectionTypeDescriptor Reflection() \
{ \
	UHReflectionTypeDescriptor RefDesc{}; \

#define UH_MEMBER_REFLECT(Type, Name) RefDesc.MemberType.push_back(Type); \
RefDesc.MemberName.push_back(Name); \

#define UH_END_REFLECT return RefDesc; \
}
#endif