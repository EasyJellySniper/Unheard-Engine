#include "GameScript.h"
#include "../Classes/Utility.h"

UHGameScript::UHGameScript()
{
	// add to script table
	if (UHGameScripts.size() == 0 || UHGameScripts.find(GetId()) == UHGameScripts.end())
	{
		UHGameScripts[GetId()] = this;
	}
}