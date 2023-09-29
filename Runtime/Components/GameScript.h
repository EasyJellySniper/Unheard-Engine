#pragma once
#include "../../UnheardEngine.h"
#include "Component.h"
#include <vector>
#include <unordered_map>

class UHEngine;
class UHScene;
class UHAssetManager;
class UHGraphic;
class UHGameTimer;

// game script class in UH, gameplay logic should be put here as virtual function
// overrided class will implement them
class UHGameScript : public UHComponent
{
public:
	UHGameScript();
	virtual ~UHGameScript() {}
	virtual void Update() override {}
	virtual void OnEngineUpdate(float DeltaTime) {}
	virtual void OnSceneInitialized(UHScene* InScene, UHAssetManager* InAsset, UHGraphic* InGfx) {}

	// --------------- Add whatever is needed here --------------- //
};

inline std::unordered_map<uint32_t, UHGameScript*> UHGameScripts;