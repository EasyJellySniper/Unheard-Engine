#include "GameScript.h"
#include "../Classes/Utility.h"

UHGameScript::UHGameScript()
{
	// add to script table
	if (UHGameScripts.find(GetId()) == UHGameScripts.end())
	{
		UHGameScripts[GetId()] = this;
	}

#if WITH_EDITOR
	bIsEditable = false;
#endif
}