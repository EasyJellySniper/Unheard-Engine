#include "Material.h"
#include <fstream>
#include <filesystem>
#include "Utility.h"
#include "AssetPath.h"

#include "GraphNode/ParameterNode.h"
#include "GraphNode/TextureNode.h"
#include "GraphNode/MathNode.h"

#if WITH_DEBUG
#include <assert.h>
#endif

// default as opaque material and set cull off for now
UHMaterial::UHMaterial()
	: CullMode(VK_CULL_MODE_NONE)
	, BlendMode(UHBlendMode::Opaque)
	, MaterialProps(UHMaterialProperty())
	, CompileFlag(UpToDate)
	, bIsSkybox(false)
	, bIsTangentSpace(false)
{
	for (int32_t Idx = 0; Idx < UHMaterialTextureType::TextureTypeMax; Idx++)
	{
		Textures[Idx] = nullptr;
		Samplers[Idx] = nullptr;
		TextureIndex[Idx] = -1;
	}

	for (int32_t Idx = 0; Idx < UHMaterialShaderType::MaterialShaderTypeMax; Idx++)
	{
		Shaders[Idx] = nullptr;
	}

	TexDefines = { "WITH_DIFFUSE", "WITH_OCCLUSION","WITH_SPECULAR","WITH_NORMAL","WITH_OPACITY", "WITH_ENVCUBE", "WITH_METALLIC", "WITH_ROUGHNESS" };

	MaterialNode = std::make_unique<UHMaterialNode>(this);
	DefaultMaterialNodePos.x = 544;
	DefaultMaterialNodePos.y = 208;
}

bool UHMaterial::Import(std::filesystem::path InMatPath)
{
	std::ifstream FileIn(InMatPath, std::ios::in | std::ios::binary);

	if (!FileIn.is_open())
	{
		// skip if failed to open
		return false;
	}

	// load material data
	FileIn.read(reinterpret_cast<char*>(&Version), sizeof(Version));
	UHUtilities::ReadStringData(FileIn, Name);

	// load referenced texture file name, doesn't read file name for sky cube at the moment
	for (int32_t Idx = 0; Idx <= UHMaterialTextureType::Opacity; Idx++)
	{
		UHUtilities::ReadStringData(FileIn, TexFileNames[Idx]);
	}

	FileIn.read(reinterpret_cast<char*>(&CullMode), sizeof(CullMode));
	FileIn.read(reinterpret_cast<char*>(&BlendMode), sizeof(BlendMode));
	FileIn.read(reinterpret_cast<char*>(&MaterialProps), sizeof(MaterialProps));

	// material graph data
	FileIn.read(reinterpret_cast<char*>(&bIsTangentSpace), sizeof(bIsTangentSpace));
	UHUtilities::ReadStringVectorData(FileIn, RegisteredTextureNames);
	ImportGraphData(FileIn);

	FileIn.close();

	MaterialPath = InMatPath;
	return true;
}

void UHMaterial::ImportGraphData(std::ifstream& FileIn)
{
	// import graph data, use the same format as in ExportGraphData
	size_t NumMaterialInputs;
	FileIn.read(reinterpret_cast<char*>(&NumMaterialInputs), sizeof(NumMaterialInputs));

	std::vector<int32_t> MaterialInputConnections;
	for (size_t Idx = 0; Idx < NumMaterialInputs; Idx++)
	{
		int32_t NodeIdx;
		FileIn.read(reinterpret_cast<char*>(&NodeIdx), sizeof(NodeIdx));
		MaterialInputConnections.push_back(NodeIdx);
	}

	size_t NumEditNodes;
	FileIn.read(reinterpret_cast<char*>(&NumEditNodes), sizeof(NumEditNodes));

	for (size_t Idx = 0; Idx < NumEditNodes; Idx++)
	{
		UHGraphNodeType Type;
		FileIn.read(reinterpret_cast<char*>(&Type), sizeof(Type));

		// allocate node by type
		std::unique_ptr<UHGraphNode> NewNode = AllocateNewGraphNode(Type);
		NewNode->InputData(FileIn);
		EditNodes.push_back(std::move(NewNode));
	}

	for (size_t Idx = 0; Idx < EditNodes.size(); Idx++)
	{
		size_t NumInputs;
		FileIn.read(reinterpret_cast<char*>(&NumInputs), sizeof(NumInputs));

		std::vector<std::unique_ptr<UHGraphPin>>& Inputs = EditNodes[Idx]->GetInputs();
		for (size_t Jdx = 0; Jdx < NumInputs; Jdx++)
		{
			// restore connection state based on node index
			int32_t NodeIdx;
			FileIn.read(reinterpret_cast<char*>(&NodeIdx), sizeof(NodeIdx));

			if (NodeIdx != -1)
			{
				Inputs[Jdx]->ConnectFrom(EditNodes[NodeIdx]->GetOutputs()[0].get());
			}
		}
	}

	// finally, restore the connection for material inputs
	std::vector<std::unique_ptr<UHGraphPin>>& MaterialInputs = MaterialNode->GetInputs();
	for (size_t Idx = 0; Idx < NumMaterialInputs; Idx++)
	{
		int32_t NodeIdx = MaterialInputConnections[Idx];

		if (NodeIdx != -1)
		{
			MaterialInputs[Idx]->ConnectFrom(EditNodes[NodeIdx]->GetOutputs()[0].get());
		}
	}

	// GUI pos data
	UHUtilities::ReadVectorData(FileIn, EditGUIRelativePos);
	FileIn.read(reinterpret_cast<char*>(&DefaultMaterialNodePos), sizeof(DefaultMaterialNodePos));
}

// post import callback
void UHMaterial::PostImport()
{
#if WITH_SHIP
	// doesn't need these data in release build
	EditGUIRelativePos.clear();
	EditNodes.clear();
	MaterialNode.reset();
#endif
}

#if WITH_DEBUG
void UHMaterial::SetTexFileName(UHMaterialTextureType TexType, std::string InName)
{
	TexFileNames[TexType] = InName;
}

void UHMaterial::Export()
{
	// export UH Material
	// 
	// create folder if it's not existed
	if (!std::filesystem::exists(GMaterialAssetPath))
	{
		std::filesystem::create_directories(GMaterialAssetPath);
	}

	// get current version before saving
	Version = static_cast<UHMaterialVersion>(MaterialVersionMax - 1);

	std::ofstream FileOut(GMaterialAssetPath + Name + GMaterialAssetExtension, std::ios::out | std::ios::binary);
	FileOut.write(reinterpret_cast<const char*>(&Version), sizeof(Version));

	// write material name
	UHUtilities::WriteStringData(FileOut, Name);

	// write texture filename used, doesn't write file name for sky cube/metallic at the moment
	for (int32_t Idx = 0; Idx <= UHMaterialTextureType::Opacity; Idx++)
	{
		UHUtilities::WriteStringData(FileOut, TexFileNames[Idx]);
	}

	// write cullmode and blend mode
	FileOut.write(reinterpret_cast<const char*>(&CullMode), sizeof(CullMode));
	FileOut.write(reinterpret_cast<const char*>(&BlendMode), sizeof(BlendMode));

	// write constants
	FileOut.write(reinterpret_cast<const char*>(&MaterialProps), sizeof(MaterialProps));

	// material graph data
	FileOut.write(reinterpret_cast<const char*>(&bIsTangentSpace), sizeof(bIsTangentSpace));
	UHUtilities::WriteStringVectorData(FileOut, RegisteredTextureNames);
	ExportGraphData(FileOut);

	FileOut.close();
}

void UHMaterial::ExportGraphData(std::ofstream& FileOut)
{
	// export graph data, format as follow
	// 1 Number of material inputs
	// 2 Connection states, the index to EditNode array
	// 3 All edit node type
	// 4 Node inputs and connection states
	//	 = Number of inputs
	//	 = Connection states
	//   = The value of the Node
	// 3 & 4 are separated since it needs initalize the node first before setting connection states

	auto FindEditNodeIndex = [this](UHGraphPin* InPin)
	{
		int32_t NodeIdx = -1;

		if (InPin)
		{
			for (size_t Idx = 0; Idx < EditNodes.size(); Idx++)
			{
				if (InPin->GetOriginNode()->GetId() == EditNodes[Idx]->GetId())
				{
					NodeIdx = static_cast<int32_t>(Idx);
					break;
				}
			}
		}

		return NodeIdx;
	};

	// 1 & 2
	const std::vector<std::unique_ptr<UHGraphPin>>& Inputs = MaterialNode->GetInputs();
	size_t NumMaterialInputs = Inputs.size();
	FileOut.write(reinterpret_cast<const char*>(&NumMaterialInputs), sizeof(NumMaterialInputs));

	for (size_t Idx = 0; Idx < NumMaterialInputs; Idx++)
	{
		int32_t NodeIdx = FindEditNodeIndex(Inputs[Idx]->GetSrcPin());
		FileOut.write(reinterpret_cast<const char*>(&NodeIdx), sizeof(NodeIdx));
	}

	// 3
	size_t NumEditNodes = EditNodes.size();
	FileOut.write(reinterpret_cast<const char*>(&NumEditNodes), sizeof(NumEditNodes));

	for (size_t Idx = 0; Idx < NumEditNodes; Idx++)
	{
		UHGraphNodeType Type = EditNodes[Idx]->GetType();
		FileOut.write(reinterpret_cast<const char*>(&Type), sizeof(Type));
		EditNodes[Idx]->OutputData(FileOut);
	}

	// 4
	for (size_t Idx = 0; Idx < NumEditNodes; Idx++)
	{
		size_t NumInputs = EditNodes[Idx]->GetInputs().size();
		FileOut.write(reinterpret_cast<const char*>(&NumInputs), sizeof(NumInputs));

		const std::vector<std::unique_ptr<UHGraphPin>>& NodeInputs = EditNodes[Idx]->GetInputs();
		for (size_t Jdx = 0; Jdx < NumInputs; Jdx++)
		{
			int32_t NodeIdx = FindEditNodeIndex(NodeInputs[Jdx]->GetSrcPin());
			FileOut.write(reinterpret_cast<const char*>(&NodeIdx), sizeof(NodeIdx));
		}
	}

	// GUI pos data
	UHUtilities::WriteVectorData(FileOut, EditGUIRelativePos);
	FileOut.write(reinterpret_cast<const char*>(&DefaultMaterialNodePos), sizeof(DefaultMaterialNodePos));
}

void CollectTexDefinitions(const UHGraphPin* Pin, int32_t& RegisterStart, std::string& Code, std::unordered_map<uint32_t, bool>& OutDefTable)
{
	if (Pin->GetSrcPin() == nullptr || Pin->GetSrcPin()->GetOriginNode() == nullptr)
	{
		return;
	}

	UHGraphNode* InputNode = Pin->GetSrcPin()->GetOriginNode();

	// prevent redefinition with table
	if (OutDefTable.find(InputNode->GetId()) == OutDefTable.end() && InputNode->GetType() == Texture2DNode)
	{
		Code += "Texture2D Node_" + std::to_string(InputNode->GetId()) + " : register(t" + std::to_string(RegisterStart++) + ");\n";

		OutDefTable[InputNode->GetId()] = true;
	}

	// trace all input pins
	for (const std::unique_ptr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CollectTexDefinitions(InputPins.get(), RegisterStart, Code, OutDefTable);
	}
}

std::string UHMaterial::GetTextureDefineCode(bool bIsDepthOrMotionPass)
{
	// get texture define code
	int32_t RegisterStart = GMaterialTextureRegisterStart;
	std::string Code;
	std::unordered_map<uint32_t, bool> TexTable;

	// only opacity is needed in depth or motion pass
	if (bIsDepthOrMotionPass)
	{
		const std::unique_ptr<UHGraphPin>& Input = MaterialNode->GetInputs()[Experimental::Opacity];
		CollectTexDefinitions(Input.get(), RegisterStart, Code, TexTable);
	}
	else
	{
		for (const std::unique_ptr<UHGraphPin>& Input : MaterialNode->GetInputs())
		{
			CollectTexDefinitions(Input.get(), RegisterStart, Code, TexTable);
		}
	}

	// @TODO: Differentiate sampler state in the future
	Code += "SamplerState " + GDefaultSamplerName + " : register(s" + std::to_string(RegisterStart++) + ");\n";
	
	return Code;
}

std::string UHMaterial::GetMaterialInputCode(UHMaterialCompileData InData)
{
	MaterialNode->SetMaterialCompileData(InData);
	return MaterialNode->EvalHLSL();
}

#endif

void UHMaterial::SetName(std::string InName)
{
	Name = InName;
}

void UHMaterial::SetCullMode(VkCullModeFlagBits InCullMode)
{
	// use for initialization at the moment
	// need to support runtime change in the future
	CullMode = InCullMode;
}

void UHMaterial::SetBlendMode(UHBlendMode InBlendMode)
{
	// use for initialization at the moment
	// need to support runtime change in the future
	BlendMode = InBlendMode;
}

void UHMaterial::SetMaterialProps(UHMaterialProperty InProp)
{
	MaterialProps = InProp;
	SetRenderDirties(true);
}

void UHMaterial::SetTex(UHMaterialTextureType InType, UHTexture* InTex)
{
	Textures[InType] = InTex;
}

void UHMaterial::SetSampler(UHMaterialTextureType InType, UHSampler* InSampler)
{
	Samplers[InType] = InSampler;
}

void UHMaterial::SetShader(UHMaterialShaderType InType, UHShader* InShader)
{
	Shaders[InType] = InShader;
}

void UHMaterial::SetTextureIndex(UHMaterialTextureType InType, int32_t InIndex)
{
	TextureIndex[InType] = InIndex;
}

void UHMaterial::SetIsSkybox(bool InFlag)
{
	bIsSkybox = InFlag;
}

void UHMaterial::SetCompileFlag(UHMaterialCompileFlag InFlag)
{
	CompileFlag = InFlag;
}

std::string UHMaterial::GetName() const
{
	return Name;
}

VkCullModeFlagBits UHMaterial::GetCullMode() const
{
	return CullMode;
}

UHBlendMode UHMaterial::GetBlendMode() const
{
	return BlendMode;
}

UHMaterialProperty UHMaterial::GetMaterialProps() const
{
	return MaterialProps;
}

bool UHMaterial::IsSkybox() const
{
	return bIsSkybox;
}

UHMaterialCompileFlag UHMaterial::GetCompileFlag() const
{
	return CompileFlag;
}

UHMaterialVersion UHMaterial::GetVersion() const
{
	return Version;
}

std::filesystem::path UHMaterial::GetPath() const
{
	return MaterialPath;
}

std::string UHMaterial::GetTexFileName(UHMaterialTextureType InType) const
{
	return TexFileNames[InType];
}

UHTexture* UHMaterial::GetTex(UHMaterialTextureType InType) const
{
	return Textures[InType];
}

UHSampler* UHMaterial::GetSampler(UHMaterialTextureType InType) const
{
	return Samplers[InType];
}

UHShader* UHMaterial::GetShader(UHMaterialShaderType InType) const
{
	return Shaders[InType];
}

int32_t UHMaterial::GetTextureIndex(UHMaterialTextureType InType) const
{
	// the texture index is set via asset manager
	// but it could have no texture without setting properly
	// in this case, return -1
	if (Textures[InType] == nullptr)
	{
		return -1;
	}

	return TextureIndex[InType];
}

std::string UHMaterial::GetTexDefineName(UHMaterialTextureType InType) const
{
	return TexDefines[InType];
}

std::vector<std::string> UHMaterial::GetMaterialDefines(UHMaterialShaderType InType) const
{
	std::vector<std::string> Defines;
	for (int32_t Idx = 0; Idx < UHMaterialTextureType::TextureTypeMax; Idx++)
	{
		if (Textures[Idx])
		{
			switch (InType)
			{
			case UHMaterialShaderType::VS:
				if (Idx == UHMaterialTextureType::Normal || Idx == UHMaterialTextureType::SkyCube)
				{
					Defines.push_back(TexDefines[Idx]);
				}
				break;
			case UHMaterialShaderType::PS:
				Defines.push_back(TexDefines[Idx]);
				break;
			}
		}
	}

	if (BlendMode == UHBlendMode::Masked && InType == PS)
	{
		Defines.push_back("WITH_ALPHATEST");
	}

	if (bIsTangentSpace)
	{
		Defines.push_back("WITH_TANGENT_SPACE");
	}

	return Defines;
}

void CollectTexNames(const UHGraphPin* Pin, std::vector<std::string>& Names, std::unordered_map<uint32_t, bool>& OutDefTable)
{
	if (Pin->GetSrcPin() == nullptr || Pin->GetSrcPin()->GetOriginNode() == nullptr)
	{
		return;
	}

	UHGraphNode* InputNode = Pin->GetSrcPin()->GetOriginNode();

	// prevent redefinition with table
	if (OutDefTable.find(InputNode->GetId()) == OutDefTable.end() && InputNode->GetType() == Texture2DNode)
	{
		UHTexture2DNode* TexNode = static_cast<UHTexture2DNode*>(InputNode);
		std::string TexName = TexNode->GetSelectedTextureName();
		if (!TexName.empty())
		{
			Names.push_back(TexName);
		}

		OutDefTable[InputNode->GetId()] = true;
	}

	// trace all input pins
	for (const std::unique_ptr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CollectTexNames(InputPins.get(), Names, OutDefTable);
	}
}

std::vector<std::string> UHMaterial::GetRegisteredTextureNames(bool bIsDepthOrMotionPass)
{
#if WITH_DEBUG
	RegisteredTextureNames.clear();
	std::unordered_map<uint32_t, bool> TexTable;

	if (bIsDepthOrMotionPass)
	{
		const std::unique_ptr<UHGraphPin>& Input = MaterialNode->GetInputs()[Experimental::Opacity];
		CollectTexNames(Input.get(), RegisteredTextureNames, TexTable);
	}
	else
	{
		for (const std::unique_ptr<UHGraphPin>& Input : MaterialNode->GetInputs())
		{
			CollectTexNames(Input.get(), RegisteredTextureNames, TexTable);
		}
	}
#endif

	return RegisteredTextureNames;
}

bool UHMaterial::operator==(const UHMaterial& InMat)
{
	return InMat.GetName() == Name
		&& InMat.GetCullMode() == CullMode
		&& InMat.GetBlendMode() == BlendMode
		&& InMat.GetMaterialProps() == MaterialProps;
}

#if WITH_DEBUG
void UHMaterial::GenerateDefaultMaterialNodes()
{
	// generate default material graph
	// for now, it automatically adds Diffuse/Occlusion/Specular/Normal/Opacity
	EditGUIRelativePos.clear();
	EditNodes.clear();
	MaterialNode.reset();
	MaterialNode = std::make_unique<UHMaterialNode>(this);

	std::unique_ptr<UHGraphNode> NewNode;
	std::vector<std::unique_ptr<UHGraphPin>>& MaterialPins = MaterialNode->GetInputs();

	POINT Pos{};
	int32_t GUIToLeft = 300;
	int32_t GUIToFurtherLeft = 750;
	int32_t GUIStartY = -400;
	int32_t GUIStepY = 60;
	int32_t GUIStepYLarger = 90;
	Pos.y = GUIStartY;

	// containers for prevent duplicating texture/parameter nodes
	std::unordered_map<size_t, UHGraphNode*> TextureNodeTable;
	std::vector<UHGraphNode*> ParameterNodeTable;

	// add texture node function
	auto AddTextureNode = [this, &TextureNodeTable](std::string InName, POINT InPos)
	{
		size_t Hash = UHUtilities::StringToHash(InName);
		std::unique_ptr<UHGraphNode> NewTexNode;

		if (TextureNodeTable.find(Hash) == TextureNodeTable.end())
		{
			NewTexNode = std::make_unique<UHTexture2DNode>(InName);
			EditNodes.push_back(std::move(NewTexNode));
			TextureNodeTable[Hash] = EditNodes.back().get();
			EditGUIRelativePos.push_back(InPos);
		}
		return TextureNodeTable[Hash];
	};

	// add parameter node
	auto AddParameterNode = [this, &ParameterNodeTable](std::unique_ptr<UHGraphNode> InNewNode, POINT InPos)
	{
		for (UHGraphNode* Node : ParameterNodeTable)
		{
			if (InNewNode->IsEqual(Node))
			{
				InNewNode.reset();
				return Node;
			}
		}

		EditNodes.push_back(std::move(InNewNode));
		EditGUIRelativePos.push_back(InPos);
		ParameterNodeTable.push_back(EditNodes.back().get());
		return EditNodes.back().get();
	};

	// add diffuse
	if (TexFileNames[Diffuse].empty())
	{
		// Diffuse = Diffuse
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepYLarger;

		UHGraphNode* NewParameterNode = AddParameterNode(std::make_unique<UHFloat3Node>(MaterialProps.Diffuse), Pos);
		UHGraphPin* DiffusePin = NewParameterNode->GetOutputs()[0].get();
		MaterialPins[Experimental::UHMaterialInputs::Diffuse]->ConnectFrom(DiffusePin);
	}
	else
	{
		// Diffuse = DiffuseColor * DiffuseTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewParameterNode = AddParameterNode(std::make_unique<UHFloat3Node>(MaterialProps.Diffuse), Pos);
		UHGraphPin* DiffusePin = NewParameterNode->GetOutputs()[0].get();

		NewNode = std::make_unique<UHMathNode>(Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();

		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[Diffuse], Pos);
		UHGraphPin* DiffuseTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[Experimental::UHMaterialInputs::Diffuse]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(DiffusePin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(DiffuseTexPin);
	}

	// add occlusion
	if (TexFileNames[Occlusion].empty())
	{
		// Occlusion = Occlusion
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepYLarger;

		UHGraphNode* NewParameterNode = AddParameterNode(std::make_unique<UHFloatNode>(MaterialProps.Occlusion), Pos);
		UHGraphPin* OcclusionPin = NewParameterNode->GetOutputs()[0].get();
		MaterialPins[Experimental::UHMaterialInputs::Occlusion]->ConnectFrom(OcclusionPin);
	}
	else
	{
		// Occlusion = Occlusion * OcclusionTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewParameterNode = AddParameterNode(std::make_unique<UHFloatNode>(MaterialProps.Occlusion), Pos);
		UHGraphPin* OcclusionPin = NewParameterNode->GetOutputs()[0].get();

		NewNode = std::make_unique<UHMathNode>(Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();


		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[Occlusion], Pos);
		UHGraphPin* OcclusionTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[Experimental::UHMaterialInputs::Occlusion]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(OcclusionPin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(OcclusionTexPin);
	}

	// add specular
	if (TexFileNames[Specular].empty())
	{
		// Specular = Specular
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepYLarger;

		UHGraphNode* NewParameterNode = AddParameterNode(std::make_unique<UHFloat3Node>(MaterialProps.Specular), Pos);
		UHGraphPin* SpecularPin = NewParameterNode->GetOutputs()[0].get();
		MaterialPins[Experimental::UHMaterialInputs::Specular]->ConnectFrom(SpecularPin);
	}
	else
	{
		// Specular = SpecularColor * SpecularTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewParameterNode = AddParameterNode(std::make_unique<UHFloat3Node>(MaterialProps.Specular), Pos);
		UHGraphPin* SpecularPin = NewParameterNode->GetOutputs()[0].get();

		NewNode = std::make_unique<UHMathNode>(Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();

		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[Specular], Pos);
		UHGraphPin* SpecularTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[Experimental::UHMaterialInputs::Specular]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(SpecularPin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(SpecularTexPin);
	}

	if (!TexFileNames[Normal].empty())
	{
		// Normal = BumpScale * BumpTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewParameterNode = AddParameterNode(std::make_unique<UHFloatNode>(MaterialProps.BumpScale), Pos);
		UHGraphPin* NormalPin = NewParameterNode->GetOutputs()[0].get();

		NewNode = std::make_unique<UHMathNode>(Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();

		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[Normal], Pos);
		UHGraphPin* NormalTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[Experimental::UHMaterialInputs::Normal]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(NormalPin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(NormalTexPin);
	}

	// add opacity
	if (!TexFileNames[Opacity].empty())
	{
		// Opacity = OpacityColor * OpacityTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepYLarger;
		UHGraphNode* NewParameterNode = AddParameterNode(std::make_unique<UHFloatNode>(MaterialProps.Opacity), Pos);
		UHGraphPin* OpacityPin = NewParameterNode->GetOutputs()[0].get();

		NewNode = std::make_unique<UHMathNode>(Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();

		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[Opacity], Pos);
		UHGraphPin* OpacityTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[Experimental::UHMaterialInputs::Opacity]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(OpacityPin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(OpacityTexPin);
	}

	if (EditGUIRelativePos.size() != EditNodes.size())
	{
		UHE_LOG("Material: Mismatched EditNodes and EditGUIRelativePos size.\n");
	}

	if (MaterialNode->GetInputs()[Experimental::Normal]->GetSrcPin())
	{
		bIsTangentSpace = true;
	}

	GetRegisteredTextureNames(false);
}

std::unique_ptr<UHMaterialNode>& UHMaterial::GetMaterialNode()
{
	return MaterialNode;
}

std::vector<std::unique_ptr<UHGraphNode>>& UHMaterial::GetEditNodes()
{
	return EditNodes;
}

void UHMaterial::SetGUIRelativePos(std::vector<POINT> InPos)
{
	EditGUIRelativePos = InPos;
}

std::vector<POINT>& UHMaterial::GetGUIRelativePos()
{
	return EditGUIRelativePos;
}

void UHMaterial::SetDefaultMaterialNodePos(POINT InPos)
{
	DefaultMaterialNodePos = InPos;
}

POINT UHMaterial::GetDefaultMaterialNodePos()
{
	return DefaultMaterialNodePos;
}

#endif