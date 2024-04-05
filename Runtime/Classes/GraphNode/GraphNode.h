#pragma once
#include "../../../../UnheardEngine.h"
#include <vector>
#include <string>
#include <fstream>
#include "GraphPin.h"
#include "../../../../Runtime/Classes/Object.h"
#include "../../../../Runtime/Classes/Types.h"

const std::string GOutChannelShared[] = { ".rgb",".r",".g",".b" };

class UHGraphNodeGUI;

enum class UHGraphNodeType
{
    UnknownNode,
    FloatNode = 100,
    Float2Node,
    Float3Node,
    Float4Node,
    MathNode,
    Texture2DNode
};

// Graph Node Class in UHE, a node can contain multiple inputs and outputs, the name, this is a base class
class UHGraphNode : public UHObject
{
public:
    UHGraphNode(bool bInCanBeDeleted);
    virtual ~UHGraphNode();

    // node functions
    virtual bool CanEvalHLSL() { return true; }
    virtual std::string EvalDefinition() { return ""; }
    virtual std::string EvalHLSL(const UHGraphPin* CallerPin) = 0;
    virtual bool IsEqual(const UHGraphNode* InNode) { return GetId() == InNode->GetId(); }

    // data I/O override
    virtual void InputData(std::ifstream& FileIn) = 0;
    virtual void OutputData(std::ofstream& FileOut) = 0;

    // GUI function for lookup
#if WITH_EDITOR
    void SetGUI(UHGraphNodeGUI* InGUI);
    UHGraphNodeGUI* GetGUI() const;
#endif
    void SetIsCompilingRayTracing(bool bInFlag);

    std::string GetName() const;
    UHGraphNodeType GetType() const;
    std::vector<UniquePtr<UHGraphPin>>& GetInputs();
    std::vector<UniquePtr<UHGraphPin>>& GetOutputs();
    bool CanBeDeleted() const;

protected:
    int32_t FindOutputPinIndexInternal(const UHGraphPin* InPin);

    std::string Name;
    UHGraphNodeType NodeType;
    std::vector<UniquePtr<UHGraphPin>> Inputs;
    std::vector<UniquePtr<UHGraphPin>> Outputs;
    bool bCanBeDeleted;
    bool bIsCompilingRayTracing;

#if WITH_EDITOR
    UHGraphNodeGUI* GUICache;
#endif
};