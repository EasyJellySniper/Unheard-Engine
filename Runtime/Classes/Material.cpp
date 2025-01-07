#include "Material.h"
#include <fstream>
#include <filesystem>
#include "Utility.h"
#include "AssetPath.h"
#include "../Engine/Graphic.h"
#include "GraphNode/ParameterNode.h"
#include "GraphNode/TextureNode.h"
#include "GraphNode/MathNode.h"

#if WITH_EDITOR
#include <assert.h>
#endif

// default as opaque material and set cull off for now
UHMaterial::UHMaterial()
	: CullMode(UHCullMode::CullNone)
	, BlendMode(UHBlendMode::Opaque)
	, CutoffValue(0.33f)
	, MaxReflectionBounce(1)
	, CompileFlag(UHMaterialCompileFlag::UpToDate)
	, MaterialUsages(UHMaterialUsage{})
	, MaterialBufferSize(0)
#if WITH_EDITOR
	, MaterialProps(UHMaterialProperty())
	, bIsMaterialNodeDirty(false)
#endif
{
	MaterialNode = MakeUnique<UHMaterialNode>(this);
	DefaultMaterialNodePos.x = 544;
	DefaultMaterialNodePos.y = 208;
}

UHMaterial::~UHMaterial()
{
	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(MaterialConstantsGPU[Idx]);
		MaterialConstantsGPU[Idx].reset();

		UH_SAFE_RELEASE(MaterialRTDataGPU[Idx]);
		MaterialRTDataGPU[Idx].reset();
	}
}

bool UHMaterial::Import(std::filesystem::path InMatPath)
{
	std::ifstream FileIn(InMatPath, std::ios::in | std::ios::binary);

	if (!FileIn.is_open())
	{
		// skip if failed to open
		return false;
	}

	UHObject::OnLoad(FileIn);

	if (Version >= UH_ENUM_VALUE(UHMaterialVersion::AddRoughnessTexture))
	{
		UHUtilities::ReadStringData(FileIn, SourcePath);
	}

	// load referenced texture file name, doesn't read file name for sky cube at the moment
	const int32_t HighestTexture = Version < UH_ENUM_VALUE(UHMaterialVersion::AddRoughnessTexture) ? UH_ENUM_VALUE(UHMaterialInputs::Opacity) : UH_ENUM_VALUE(UHMaterialInputs::Roughness);
	for (int32_t Idx = 0; Idx <= HighestTexture; Idx++)
	{
		UHUtilities::ReadStringData(FileIn, TexFileNames[Idx]);
	}

	FileIn.read(reinterpret_cast<char*>(&CullMode), sizeof(CullMode));
	FileIn.read(reinterpret_cast<char*>(&BlendMode), sizeof(BlendMode));
	FileIn.read(reinterpret_cast<char*>(&CutoffValue), sizeof(CutoffValue));
	FileIn.read(reinterpret_cast<char*>(&MaxReflectionBounce), sizeof(MaxReflectionBounce));

	// material graph data
	UHUtilities::ReadStringVectorData(FileIn, RegisteredTextureNames);
	ImportGraphData(FileIn);

	// material constant data
	if (Version >= UH_ENUM_VALUE(UHMaterialVersion::GoingBindless))
	{
		FileIn.read(reinterpret_cast<char*>(&MaterialBufferSize), sizeof(MaterialBufferSize));
	}

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
	std::vector<int32_t> MaterialInputPinOutputIndex;
	for (size_t Idx = 0; Idx < NumMaterialInputs; Idx++)
	{
		int32_t NodeIdx;
		FileIn.read(reinterpret_cast<char*>(&NodeIdx), sizeof(NodeIdx));

		int32_t OutputIdx = 0;
		if (Version >= UH_ENUM_VALUE(UHMaterialVersion::AddRoughnessTexture))
		{
			FileIn.read(reinterpret_cast<char*>(&OutputIdx), sizeof(OutputIdx));
		}

		MaterialInputConnections.push_back(NodeIdx);
		MaterialInputPinOutputIndex.push_back(OutputIdx);
	}

	size_t NumEditNodes;
	FileIn.read(reinterpret_cast<char*>(&NumEditNodes), sizeof(NumEditNodes));

	for (size_t Idx = 0; Idx < NumEditNodes; Idx++)
	{
		UHGraphNodeType Type;
		FileIn.read(reinterpret_cast<char*>(&Type), sizeof(Type));

		// allocate node by type
		UniquePtr<UHGraphNode> NewNode = AllocateNewGraphNode(Type);
		NewNode->InputData(FileIn);
		EditNodes.push_back(std::move(NewNode));
	}

	for (size_t Idx = 0; Idx < EditNodes.size(); Idx++)
	{
		size_t NumInputs;
		FileIn.read(reinterpret_cast<char*>(&NumInputs), sizeof(NumInputs));

		std::vector<UniquePtr<UHGraphPin>>& Inputs = EditNodes[Idx]->GetInputs();
		for (size_t Jdx = 0; Jdx < NumInputs; Jdx++)
		{
			// restore connection state based on node index
			int32_t NodeIdx;
			FileIn.read(reinterpret_cast<char*>(&NodeIdx), sizeof(NodeIdx));

			int32_t OutputIdx = 0;
			if (Version >= UH_ENUM_VALUE(UHMaterialVersion::AddRoughnessTexture))
			{
				FileIn.read(reinterpret_cast<char*>(&OutputIdx), sizeof(OutputIdx));
			}

			if (NodeIdx != UHINDEXNONE)
			{
				Inputs[Jdx]->ConnectFrom(EditNodes[NodeIdx]->GetOutputs()[OutputIdx].get());
			}
		}
	}

	// finally, restore the connection for material inputs
	std::vector<UniquePtr<UHGraphPin>>& MaterialInputs = MaterialNode->GetInputs();
	for (size_t Idx = 0; Idx < NumMaterialInputs; Idx++)
	{
		int32_t NodeIdx = MaterialInputConnections[Idx];

		if (NodeIdx != UHINDEXNONE)
		{
			MaterialInputs[Idx]->ConnectFrom(EditNodes[NodeIdx]->GetOutputs()[MaterialInputPinOutputIndex[Idx]].get());
		}
	}

	// GUI pos data
	UHUtilities::ReadVectorData(FileIn, EditGUIRelativePos);
	FileIn.read(reinterpret_cast<char*>(&DefaultMaterialNodePos), sizeof(DefaultMaterialNodePos));
}

// post import callback
void UHMaterial::PostImport()
{
	if (GIsShipping)
	{
		// doesn't need edit node data in release build
		// the material node is still needed for copy parameters
		EditGUIRelativePos.clear();
	}

	if (Version < UH_ENUM_VALUE(UHMaterialVersion::AddRoughnessTexture))
	{
		SourcePath = Name;
	}

#if WITH_EDITOR
	// evaluate material constant buffer size for the old assets
	if (Version < UH_ENUM_VALUE(UHMaterialVersion::AddReflectionBounce))
	{
		GetCBufferDefineCode(MaterialBufferSize);
	}
#endif

	AllocateMaterialBuffer();
	AllocateRTMaterialBuffer();
	UpdateMaterialUsage();
}

void UHMaterial::SetName(std::string InName)
{
	Name = InName;
}

void UHMaterial::SetCompileFlag(UHMaterialCompileFlag InFlag)
{
	CompileFlag = InFlag;
}

void UHMaterial::SetRegisteredTextureIndexes(std::vector<int32_t> InData)
{
	RegisteredTextureIndexes = InData;
}

void UHMaterial::AllocateMaterialBuffer()
{
	MaterialConstantsCPU.resize(MaterialBufferSize);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(MaterialConstantsGPU[Idx]);
		MaterialConstantsGPU[Idx].reset();

		// the buffer size will be aligned to 256, check how many element it actually needs
		size_t ElementCount = (MaterialBufferSize + 256) / 256;
		MaterialConstantsGPU[Idx] = GfxCache->RequestRenderBuffer<uint8_t>(ElementCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
			, Name + "_Constant");
	}
}

void UHMaterial::AllocateRTMaterialBuffer()
{
	// allocate RT matrial buffer
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(MaterialRTDataGPU[Idx]);
		MaterialRTDataGPU[Idx].reset();

		MaterialRTDataGPU[Idx] = GfxCache->RequestRenderBuffer<UHRTMaterialData>(1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, Name + "_RTConstant");
	}
}

void UHMaterial::UploadMaterialData(int32_t CurrFrame)
{
	if (MaterialBufferSize == 0)
	{
		return;
	}

	// update usages before uploading
	UpdateMaterialUsage();

	// upload data one by one, this must be following the definitions in GetCBufferDefineCode()
	size_t BufferAddress = 0;
	size_t Stride = sizeof(float);

	// fill the index of textures
	for (size_t Idx = 0; Idx < RegisteredTextureIndexes.size(); Idx++)
	{
		memcpy_s(MaterialConstantsCPU.data() + BufferAddress, Stride, &RegisteredTextureIndexes[Idx], Stride);
		BufferAddress += Stride;
	}

	// copy material parameters
	MaterialNode->CopyMaterialParameter(MaterialConstantsCPU, BufferAddress);

	// fill cutoff
	memcpy_s(MaterialConstantsCPU.data() + BufferAddress, Stride, &CutoffValue, Stride);
	BufferAddress += Stride;

	// copy blend mode
	memcpy_s(MaterialConstantsCPU.data() + BufferAddress, Stride, &BlendMode, Stride);
	BufferAddress += Stride;

	// copy material usages
	uint32_t UsageValue = 0;
	UsageValue |= MaterialUsages.bIsTangentSpace ? UH_ENUM_VALUE_U(UHMaterialFeatureBits::MaterialTangentSpace) : 0;
	UsageValue |= MaterialUsages.bUseRefraction ? UH_ENUM_VALUE_U(UHMaterialFeatureBits::MaterialRefraction) : 0;

	memcpy_s(MaterialConstantsCPU.data() + BufferAddress, Stride, &UsageValue, Stride);
	BufferAddress += Stride;

	// copy max reflection
	memcpy_s(MaterialConstantsCPU.data() + BufferAddress, Stride, &MaxReflectionBounce, Stride);
	BufferAddress += Stride;

	// upload material data, the BufferAddress should reach the end of material buffer at this point
	assert(MaterialBufferSize == BufferAddress);
	MaterialConstantsGPU[CurrFrame]->UploadAllData(MaterialConstantsCPU.data(), MaterialBufferSize);

	if (GfxCache->IsRayTracingEnabled())
	{
		// copy the data to RT material data, the order does matter
		int32_t DstIndex = 0;

		// ** Start copy system values, be sure to adjust GRTMaterialDataStartIndex too! ** //

		// copy cutoff
		memset(&MaterialRTDataCPU, 0, sizeof(UHRTMaterialData));
		memcpy_s(&MaterialRTDataCPU.Data[DstIndex++], Stride, &CutoffValue, Stride);

		// copy blend mode
		memcpy_s(&MaterialRTDataCPU.Data[DstIndex++], Stride, &BlendMode, Stride);

		// copy usage value
		memcpy_s(&MaterialRTDataCPU.Data[DstIndex++], Stride, &UsageValue, Stride);

		// copy max reflection value
		memcpy_s(&MaterialRTDataCPU.Data[DstIndex++], Stride, &MaxReflectionBounce, Stride);

		assert(DstIndex == GRTMaterialDataStartIndex);
		// ** End copy system values ** //

		// copy texture indexes if necessary
		for (size_t Idx = 0; Idx < RegisteredTextureIndexes.size(); Idx++)
		{
			memcpy_s(&MaterialRTDataCPU.Data[DstIndex++], Stride, &RegisteredTextureIndexes[Idx], Stride);
			//@TODO: Custom sampler index? Be sure to modify the index in TextureNode.cpp too if this is going to be uncommented.
			//memcpy_s(&MaterialRTDataCPU.Data[DstIndex++], Stride, &DefaultSamplerIndex, Stride);
		}

		// copy material parameters
		MaterialNode->CopyRTMaterialParameter(MaterialRTDataCPU, DstIndex);
		assert(DstIndex < GMaxRTMaterialDataSlot);

		// finally, upload to GPU buffer
		MaterialRTDataGPU[CurrFrame]->UploadAllData(&MaterialRTDataCPU);
	}
}

std::string UHMaterial::GetName() const
{
	return Name;
}

std::string UHMaterial::GetSourcePath() const
{
	return SourcePath;
}

UHCullMode UHMaterial::GetCullMode() const
{
	return CullMode;
}

UHBlendMode UHMaterial::GetBlendMode() const
{
	return BlendMode;
}

bool UHMaterial::IsOpaque() const
{
	return GetBlendMode() < UHBlendMode::TranditionalAlpha;
}

UHMaterialUsage UHMaterial::GetMaterialUsages() const
{
	return MaterialUsages;
}

std::vector<std::string> UHMaterial::GetShaderDefines()
{
	std::vector<std::string> Defines;
	UpdateMaterialUsage();

	if (MaterialUsages.bIsTangentSpace)
	{
		Defines.push_back("TANGENT_SPACE");
	}

	if (BlendMode == UHBlendMode::Masked)
	{
		Defines.push_back("MASKED");
	}

	if (BlendMode > UHBlendMode::Masked)
	{
		Defines.push_back("TRANSLUCENT");
	}

	return Defines;
}

bool UHMaterial::IsDifferentBlendGroup(UHMaterial* InA, UHMaterial* InB)
{
	return (UH_ENUM_VALUE(InA->GetBlendMode()) / UH_ENUM_VALUE(UHBlendMode::TranditionalAlpha)) 
		!= (UH_ENUM_VALUE(InB->GetBlendMode()) / UH_ENUM_VALUE(UHBlendMode::TranditionalAlpha));
}

UHMaterialCompileFlag UHMaterial::GetCompileFlag() const
{
	return CompileFlag;
}

std::filesystem::path UHMaterial::GetPath() const
{
	return MaterialPath;
}

const std::vector<std::string>& UHMaterial::GetRegisteredTextureNames()
{
#if WITH_EDITOR
	MaterialNode->CollectTextureNames(RegisteredTextureNames);
#endif

	return RegisteredTextureNames;
}

const std::array<UniquePtr<UHRenderBuffer<uint8_t>>, GMaxFrameInFlight>& UHMaterial::GetMaterialConst()
{
	return MaterialConstantsGPU;
}

UHRenderBuffer<UHRTMaterialData>* UHMaterial::GetRTMaterialDataGPU(int32_t CurrFrame) const
{
	return MaterialRTDataGPU[CurrFrame].get();
}

bool UHMaterial::operator==(const UHMaterial& InMat)
{
	return InMat.Name == Name
		&& InMat.CullMode == CullMode
		&& InMat.BlendMode == BlendMode
		&& InMat.CutoffValue == CutoffValue
		&& InMat.MaxReflectionBounce == MaxReflectionBounce;
}

void UHMaterial::UpdateMaterialUsage()
{
	MaterialUsages.bIsTangentSpace = MaterialNode->GetInputs()[UH_ENUM_VALUE(UHMaterialInputs::Normal)]->GetSrcPin() != nullptr;
	MaterialUsages.bUseRefraction = MaterialNode->GetInputs()[UH_ENUM_VALUE(UHMaterialInputs::Refraction)]->GetSrcPin() != nullptr;
}

#if WITH_EDITOR

void UHMaterial::SetCullMode(UHCullMode InCullMode)
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

void UHMaterial::SetTexFileName(UHMaterialInputs TexType, std::string InName)
{
	TexFileNames[UH_ENUM_VALUE(TexType)] = InName;
}

void UHMaterial::SetMaterialBufferSize(size_t InSize)
{
	MaterialBufferSize = InSize;
	if (MaterialConstantsCPU.size() != MaterialBufferSize)
	{
		// reallocate the material buffer if size changed
		AllocateMaterialBuffer();
	}
}

void UHMaterial::Export(const std::filesystem::path InPath)
{
	// export UH Material
	// 
	// create folder if it's not existed
	if (!std::filesystem::exists(GMaterialAssetPath))
	{
		std::filesystem::create_directories(GMaterialAssetPath);
	}

	const std::string OutPath = InPath.empty() ? GMaterialAssetPath + SourcePath + GMaterialAssetExtension : InPath.string();
	std::ofstream FileOut(OutPath, std::ios::out | std::ios::binary);

	// get current version before saving
	Version = UH_ENUM_VALUE(UHMaterialVersion::MaterialVersionMax) - 1;
	UHObject::OnSave(FileOut);

	UHUtilities::WriteStringData(FileOut, SourcePath);

	// write texture filename used, doesn't write file name for sky cube/metallic at the moment
	for (int32_t Idx = 0; Idx <= UH_ENUM_VALUE(UHMaterialInputs::Roughness); Idx++)
	{
		UHUtilities::WriteStringData(FileOut, TexFileNames[Idx]);
	}

	// write properties
	FileOut.write(reinterpret_cast<const char*>(&CullMode), sizeof(CullMode));
	FileOut.write(reinterpret_cast<const char*>(&BlendMode), sizeof(BlendMode));
	FileOut.write(reinterpret_cast<const char*>(&CutoffValue), sizeof(CutoffValue));
	FileOut.write(reinterpret_cast<const char*>(&MaxReflectionBounce), sizeof(MaxReflectionBounce));

	// material graph data
	MaterialNode->CollectTextureNames(RegisteredTextureNames);
	UHUtilities::WriteStringVectorData(FileOut, RegisteredTextureNames);
	ExportGraphData(FileOut);

	// new version, going bindless
	if (Version >= UH_ENUM_VALUE(UHMaterialVersion::GoingBindless))
	{
		// ensure the MaterialBufferSize is up-to-date before saving
		GetCBufferDefineCode(MaterialBufferSize);
		FileOut.write(reinterpret_cast<const char*>(&MaterialBufferSize), sizeof(MaterialBufferSize));
	}

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

	auto FindEditNodeIndex = [this](UHGraphPin* InPin, int32_t& OutputIdx)
		{
			int32_t NodeIdx = UHINDEXNONE;
			OutputIdx = 0;

			if (InPin)
			{
				for (size_t Idx = 0; Idx < EditNodes.size(); Idx++)
				{
					if (InPin->GetOriginNode()->GetId() == EditNodes[Idx]->GetId())
					{
						for (const UniquePtr<UHGraphPin>& Output : EditNodes[Idx]->GetOutputs())
						{
							if (Output.get() == InPin)
							{
								break;
							}
							OutputIdx++;
						}
						NodeIdx = static_cast<int32_t>(Idx);
						break;
					}
				}
			}

			return NodeIdx;
		};

	// 1 & 2
	const std::vector<UniquePtr<UHGraphPin>>& Inputs = MaterialNode->GetInputs();
	size_t NumMaterialInputs = Inputs.size();
	FileOut.write(reinterpret_cast<const char*>(&NumMaterialInputs), sizeof(NumMaterialInputs));

	for (size_t Idx = 0; Idx < NumMaterialInputs; Idx++)
	{
		int32_t OutputIdx;
		int32_t NodeIdx = FindEditNodeIndex(Inputs[Idx]->GetSrcPin(), OutputIdx);
		FileOut.write(reinterpret_cast<const char*>(&NodeIdx), sizeof(NodeIdx));
		FileOut.write(reinterpret_cast<const char*>(&OutputIdx), sizeof(OutputIdx));
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

		const std::vector<UniquePtr<UHGraphPin>>& NodeInputs = EditNodes[Idx]->GetInputs();
		for (size_t Jdx = 0; Jdx < NumInputs; Jdx++)
		{
			int32_t OutputIdx;
			int32_t NodeIdx = FindEditNodeIndex(NodeInputs[Jdx]->GetSrcPin(), OutputIdx);
			FileOut.write(reinterpret_cast<const char*>(&NodeIdx), sizeof(NodeIdx));
			FileOut.write(reinterpret_cast<const char*>(&OutputIdx), sizeof(OutputIdx));
		}
	}

	// GUI pos data
	UHUtilities::WriteVectorData(FileOut, EditGUIRelativePos);
	FileOut.write(reinterpret_cast<const char*>(&DefaultMaterialNodePos), sizeof(DefaultMaterialNodePos));
}

std::string UHMaterial::GetCBufferDefineCode(size_t& OutSize)
{
	OutSize = 0;

	// get texture define code
	std::string Code;
	MaterialNode->CollectTextureIndex(Code, OutSize);
	MaterialNode->CollectMaterialParameter(Code, OutSize);

	// constant from system
	Code += "\tfloat GCutoff;\n";
	Code += "\tint GBlendMode;\n";
	Code += "\tuint GMaterialFeature;\n";
	Code += "\tuint GMaxReflectionBounce;\n";

	OutSize += sizeof(float) * 4;

	return Code;
}

std::string UHMaterial::GetMaterialInputCode(UHMaterialCompileData InData)
{
	MaterialNode->SetMaterialCompileData(InData);
	return MaterialNode->EvalHLSL(nullptr);
}

void UHMaterial::SetMaterialProps(UHMaterialProperty InProp)
{
	MaterialProps = InProp;
	SetRenderDirties(true);
}

UHMaterialProperty UHMaterial::GetMaterialProps() const
{
	return MaterialProps;
}


void UHMaterial::GenerateDefaultMaterialNodes()
{
	// generate default material graph
	// for now, it automatically adds Diffuse/Occlusion/Specular/Normal/Opacity
	EditGUIRelativePos.clear();
	EditNodes.clear();
	MaterialNode.reset();
	MaterialNode = MakeUnique<UHMaterialNode>(this);

	UniquePtr<UHGraphNode> NewNode;
	std::vector<UniquePtr<UHGraphPin>>& MaterialPins = MaterialNode->GetInputs();

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
		UniquePtr<UHGraphNode> NewTexNode;

		if (TextureNodeTable.find(Hash) == TextureNodeTable.end())
		{
			NewTexNode = MakeUnique<UHTexture2DNode>(InName);
			EditNodes.push_back(std::move(NewTexNode));
			TextureNodeTable[Hash] = EditNodes.back().get();
			EditGUIRelativePos.push_back(InPos);
		}
		return TextureNodeTable[Hash];
	};

	// add parameter node
	auto AddParameterNode = [this, &ParameterNodeTable](UniquePtr<UHGraphNode> InNewNode, POINT InPos)
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
	if (TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Diffuse)].empty())
	{
		// Diffuse = Diffuse
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepYLarger;

		UHGraphNode* NewParameterNode = AddParameterNode(MakeUnique<UHFloat3Node>(MaterialProps.Diffuse), Pos);
		UHGraphPin* DiffusePin = NewParameterNode->GetOutputs()[0].get();
		MaterialPins[UH_ENUM_VALUE(UHMaterialInputs::Diffuse)]->ConnectFrom(DiffusePin);
	}
	else
	{
		// Diffuse = DiffuseColor * DiffuseTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewParameterNode = AddParameterNode(MakeUnique<UHFloat3Node>(MaterialProps.Diffuse), Pos);
		UHGraphPin* DiffusePin = NewParameterNode->GetOutputs()[0].get();

		NewNode = MakeUnique<UHMathNode>(UHMathNodeOperator::Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();

		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Diffuse)], Pos);
		UHGraphPin* DiffuseTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[UH_ENUM_VALUE(UHMaterialInputs::Diffuse)]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(DiffusePin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(DiffuseTexPin);
	}

	// add occlusion
	if (TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Occlusion)].empty())
	{
		// Occlusion = Occlusion
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepYLarger;

		UHGraphNode* NewParameterNode = AddParameterNode(MakeUnique<UHFloatNode>(MaterialProps.Occlusion), Pos);
		UHGraphPin* OcclusionPin = NewParameterNode->GetOutputs()[0].get();
		MaterialPins[UH_ENUM_VALUE(UHMaterialInputs::Occlusion)]->ConnectFrom(OcclusionPin);
	}
	else
	{
		// Occlusion = Occlusion * OcclusionTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewParameterNode = AddParameterNode(MakeUnique<UHFloatNode>(MaterialProps.Occlusion), Pos);
		UHGraphPin* OcclusionPin = NewParameterNode->GetOutputs()[0].get();

		NewNode = MakeUnique<UHMathNode>(UHMathNodeOperator::Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();


		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Occlusion)], Pos);
		UHGraphPin* OcclusionTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[UH_ENUM_VALUE(UHMaterialInputs::Occlusion)]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(OcclusionPin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(OcclusionTexPin);
	}

	// add specular
	if (TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Specular)].empty())
	{
		// Specular = Specular
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepYLarger;

		UHGraphNode* NewParameterNode = AddParameterNode(MakeUnique<UHFloat3Node>(MaterialProps.Specular), Pos);
		UHGraphPin* SpecularPin = NewParameterNode->GetOutputs()[0].get();
		MaterialPins[UH_ENUM_VALUE(UHMaterialInputs::Specular)]->ConnectFrom(SpecularPin);
	}
	else
	{
		// Specular = SpecularColor * SpecularTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewParameterNode = AddParameterNode(MakeUnique<UHFloat3Node>(MaterialProps.Specular), Pos);
		UHGraphPin* SpecularPin = NewParameterNode->GetOutputs()[0].get();

		NewNode = MakeUnique<UHMathNode>(UHMathNodeOperator::Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();

		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Specular)], Pos);
		UHGraphPin* SpecularTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[UH_ENUM_VALUE(UHMaterialInputs::Specular)]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(SpecularPin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(SpecularTexPin);
	}

	if (!TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Normal)].empty())
	{
		// Normal = BumpScale * BumpTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewParameterNode = AddParameterNode(MakeUnique<UHFloatNode>(MaterialProps.BumpScale), Pos);
		UHGraphPin* NormalPin = NewParameterNode->GetOutputs()[0].get();

		NewNode = MakeUnique<UHMathNode>(UHMathNodeOperator::Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();

		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Normal)], Pos);
		UHGraphPin* NormalTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[UH_ENUM_VALUE(UHMaterialInputs::Normal)]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(NormalPin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(NormalTexPin);
	}

	// add opacity
	if (!TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Opacity)].empty())
	{
		// Opacity = OpacityColor * OpacityTexture
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepYLarger;
		UHGraphNode* NewParameterNode = AddParameterNode(MakeUnique<UHFloatNode>(MaterialProps.Opacity), Pos);
		UHGraphPin* OpacityPin = NewParameterNode->GetOutputs()[0].get();

		NewNode = MakeUnique<UHMathNode>(UHMathNodeOperator::Multiply);
		EditNodes.push_back(std::move(NewNode));
		Pos.x = -GUIToLeft;
		Pos.y += GUIStepY;
		EditGUIRelativePos.push_back(Pos);
		UHGraphPin* MathPin = EditNodes.back()->GetOutputs()[0].get();

		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepY;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Opacity)], Pos);
		UHGraphPin* OpacityTexPin = NewTex->GetOutputs()[0].get();

		MaterialPins[UH_ENUM_VALUE(UHMaterialInputs::Opacity)]->ConnectFrom(MathPin);
		MathPin->GetOriginNode()->GetInputs()[0]->ConnectFrom(OpacityPin);
		MathPin->GetOriginNode()->GetInputs()[1]->ConnectFrom(OpacityTexPin);
	}

	// add roughness
	if (!TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Roughness)].empty())
	{
		// Roughness = Roughness
		Pos.x = -GUIToFurtherLeft;
		Pos.y += GUIStepYLarger;
		UHGraphNode* NewTex = AddTextureNode(TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::Roughness)], Pos);
		UHGraphPin* RoughnessTexPin = NewTex->GetOutputs()[1].get();
		MaterialPins[UH_ENUM_VALUE(UHMaterialInputs::Roughness)]->ConnectFrom(RoughnessTexPin);
	}

	if (EditGUIRelativePos.size() != EditNodes.size())
	{
		UHE_LOG("Material: Mismatched EditNodes and EditGUIRelativePos size.\n");
	}

	UpdateMaterialUsage();

	GetRegisteredTextureNames();
	GetCBufferDefineCode(MaterialBufferSize);
}

UniquePtr<UHMaterialNode>& UHMaterial::GetMaterialNode()
{
	return MaterialNode;
}

std::vector<UniquePtr<UHGraphNode>>& UHMaterial::GetEditNodes()
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

void UHMaterial::SetSourcePath(const std::string InPath)
{
	SourcePath = InPath;
}

bool UHMaterial::IsMaterialNodeDirty() const
{
	return bIsMaterialNodeDirty;
}

#endif