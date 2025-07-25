#include "MaterialDialog.h"

#if WITH_EDITOR
#include "../../Resource.h"
#include "Runtime/Engine/Asset.h"
#include "../Classes/EditorUtils.h"
#include "../Classes/ParameterNodeGUI.h"
#include "../Classes/MathNodeGUI.h"
#include "../Classes/TextureNodeGUI.h"
#include "Runtime/Classes/Material.h"
#include "Runtime/Renderer/DeferredShadingRenderer.h"
#include "StatusDialog.h"

enum class UHNodeMenuAction
{
    NoAction = 50,
    Deletion,
    Disconnect,

    // add node must be put at the bottom for some reasons
    AddNode,
};

// the number of the names must follow the enum number (or smaller)
std::vector<std::wstring> GCullModeNames = { L"None", L"Front", L"Back" };
std::vector<std::wstring> GBlendModeNames = { L"Opaque", L"Masked", L"AlphaBlend", L"Addition" };

UHMaterialDialog::UHMaterialDialog(HINSTANCE InInstance, HWND InWindow, UHAssetManager* InAssetManager, UHDeferredShadingRenderer* InRenderer)
    : UHDialog(InInstance, InWindow)
    , AssetManager(InAssetManager)
    , Renderer(InRenderer)
    , GUIToMove(nullptr)
    , MousePos(POINT())
    , PrevMousePos(POINT())
    , MousePosWhenRightDown(POINT())
    , NodeToDelete(nullptr)
    , PinToDisconnect(nullptr)
    , NodeMenuAction(UH_ENUM_VALUE(UHNodeMenuAction::NoAction))
    , bNeedRepaint(false)
    , CurrentMaterialIndex(-1)
    , CurrentMaterial(nullptr)
    , GdiplusToken(0)
    , WorkAreaBmp(nullptr)
    , WorkAreaMemDC(nullptr)
    , bIsUpdatingDragLine(false)
    , DragRect(RECT())
    , WindowPos(ImVec2())
    , WindowWidth(0.0f)
    , WindowHeight(0.0f)
    , bResetWindow(false)
    , bIsInitializing(false)
{
    // create popup menu for node functions, only do these in construction time
    ParameterMenu.InsertOption("Float", UH_ENUM_VALUE(UHGraphNodeType::FloatNode));
    ParameterMenu.InsertOption("Float2", UH_ENUM_VALUE(UHGraphNodeType::Float2Node));
    ParameterMenu.InsertOption("Float3", UH_ENUM_VALUE(UHGraphNodeType::Float3Node));
    ParameterMenu.InsertOption("Float4", UH_ENUM_VALUE(UHGraphNodeType::Float4Node));

    TextureMenu.InsertOption("Texture2D", UH_ENUM_VALUE(UHGraphNodeType::Texture2DNode));

    AddNodeMenu.InsertOption("Parameter", 0, ParameterMenu.GetMenu());
    AddNodeMenu.InsertOption("Math", UH_ENUM_VALUE(UHGraphNodeType::MathNode));
    AddNodeMenu.InsertOption("Texture", 0, TextureMenu.GetMenu());

    NodeFunctionMenu.InsertOption("Add node", 0, AddNodeMenu.GetMenu());
    NodeFunctionMenu.InsertOption("Delete node", UH_ENUM_VALUE(UHNodeMenuAction::Deletion));
    NodePinMenu.InsertOption("Disconnect", UH_ENUM_VALUE(UHNodeMenuAction::Disconnect));

    GdiplusStartup(&GdiplusToken, &GdiplusStartupInput, NULL);

    OnDestroy.push_back(StdBind(&UHMaterialDialog::OnFinished, this));
    OnMenuClicked.push_back(StdBind(&UHMaterialDialog::OnMenuHit, this, std::placeholders::_1));
    OnPaint.push_back(StdBind(&UHMaterialDialog::OnDrawing, this, std::placeholders::_1));
    OnResized.push_back(StdBind(&UHMaterialDialog::OnResizing, this));
    OnMoved.push_back(StdBind(&UHMaterialDialog::ResetDialogWindow, this));
}

UHMaterialDialog::~UHMaterialDialog()
{
    // clear GUI
    for (UniquePtr<UHGraphNodeGUI>& GUI : EditNodeGUIs)
    {
        GUI.reset();
    }
    EditNodeGUIs.clear();
    
    GdiplusShutdown(GdiplusToken);
}

void UHMaterialDialog::CreateWorkAreaMemDC(int32_t Width, int32_t Height)
{
    // create mem DC / bitmap for workarea
    HDC WorkAreaDC = GetDC(WorkAreaGUI->GetHwnd());
    WorkAreaMemDC = CreateCompatibleDC(WorkAreaDC);
    WorkAreaBmp = CreateCompatibleBitmap(WorkAreaDC, Width, Height);
    ReleaseDC(WorkAreaGUI->GetHwnd(), WorkAreaDC);
}

void UHMaterialDialog::ShowDialog()
{
    if (!IsDialogActive(IDD_MATERIAL))
    {
        Init();
        ShowWindow(Dialog, SW_SHOW);
        ResetDialogWindow();
    }
}

void UHMaterialDialog::Update(bool& bIsDialogActive)
{
    if (!IsDialogActive(IDD_MATERIAL))
    {
        return;
    }

    if (bResetWindow && DialogSize.has_value())
    {
        ImGui::SetNextWindowPos(ImVec2(WindowPos.x - WindowWidth, WindowPos.y));
        ImGui::SetNextWindowSize(ImVec2(WindowWidth, WindowHeight));
        bResetWindow = false;
    }

    ImGui::Begin("Material Editor", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_HorizontalScrollbar| ImGuiWindowFlags_NoMove);
    const ImVec2 WndSize = ImGui::GetWindowSize();
    ImGui::Text("Select the material to edit");

    // Material list
    if (ImGui::BeginListBox("##", ImVec2(-FLT_MIN, WndSize.y * 0.8f)))
    {
        const std::vector<UHMaterial*>& Materials = AssetManager->GetMaterials();
        for (int32_t Idx = 0; Idx < static_cast<int32_t>(Materials.size()); Idx++)
        {
            bool bIsSelected = (CurrentMaterialIndex == Idx);
            std::string SourcePath = Materials[Idx]->GetSourcePath();
            if (Materials[Idx]->bIsMaterialNodeDirty)
            {
                SourcePath = "*" + SourcePath;
            }

            if (ImGui::Selectable(SourcePath.c_str(), &bIsSelected))
            {
                SelectMaterial(Idx);
                CurrentMaterialIndex = Idx;
                bNeedRepaint = true;
            }
        }
        ImGui::EndListBox();
    }
    ImGui::NewLine();

    if (ImGui::Button("Compile"))
    {
        ControlRecompileMaterial();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        ControlResaveMaterial();
    }

    if (ImGui::Button("Save All"))
    {
        ControlResaveAllMaterials();
    }
    ImGui::NewLine();

    // Cull mode list
    if (CurrentMaterial)
    {
        const std::string CurrCullModeText = UHUtilities::ToStringA(GCullModeNames[UH_ENUM_VALUE(CurrentMaterial->GetCullMode())]);
        if (ImGui::BeginCombo("Cull Mode", CurrCullModeText.c_str()))
        {
            for (int32_t Idx = 0; Idx < static_cast<int32_t>(GCullModeNames.size()); Idx++)
            {
                const bool bIsSelected = (UH_ENUM_VALUE(CurrentMaterial->GetCullMode()) == Idx);
                std::string CullModeText = UHUtilities::ToStringA(GCullModeNames[Idx]);
                if (ImGui::Selectable(CullModeText.c_str(), bIsSelected))
                {
                    ControlCullMode(Idx);
                }
            }
            ImGui::EndCombo();
        }

        const std::string CurrBlendModeText = UHUtilities::ToStringA(GBlendModeNames[UH_ENUM_VALUE(CurrentMaterial->GetBlendMode())]);
        if (ImGui::BeginCombo("Blend Mode", CurrBlendModeText.c_str()))
        {
            if (CurrentMaterial)
            {
                for (int32_t Idx = 0; Idx < static_cast<int32_t>(GBlendModeNames.size()); Idx++)
                {
                    const bool bIsSelected = (UH_ENUM_VALUE(CurrentMaterial->GetBlendMode()) == Idx);
                    std::string BlendModeText = UHUtilities::ToStringA(GBlendModeNames[Idx]);
                    if (ImGui::Selectable(BlendModeText.c_str(), bIsSelected))
                    {
                        ControlBlendMode(Idx);
                    }
                }
            }
            ImGui::EndCombo();
        }

        // Material properties
        bool bHasPropertyChanged = false;
        if (ImGui::InputFloat("Cutoff", &CurrentMaterial->CutoffValue))
        {
            bHasPropertyChanged = true;
        }

        int32_t Bounce = static_cast<int32_t>(CurrentMaterial->MaxReflectionBounce);
        if (ImGui::SliderInt("Reflection Bounce", &Bounce, 1, UHRTReflectionShader::MaxReflectionRecursion))
        {
            CurrentMaterial->MaxReflectionBounce = static_cast<uint32_t>(Bounce);
            bHasPropertyChanged = true;
        }

        if (bHasPropertyChanged)
        {
            CurrentMaterial->SetRenderDirties(true);
            CurrentMaterial->SetMotionDirties(true);
        }
    }

    DialogSize = ImGui::GetWindowSize();
    bIsDialogActive |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::End();

    // store mouse states
    GetCursorPos(&MousePos);
    if (RawInput.IsRightMouseDown())
    {
        MousePosWhenRightDown = MousePos;
    }

    // Edit GUI nodes
    for (const UniquePtr<UHGraphNodeGUI>& GUI : EditNodeGUIs)
    {
        GUI->Update();
    }

    // force a invalidate if it needs repainting
    if (bNeedRepaint)
    {
        RECT R = WorkAreaGUI->GetCurrentRelativeRect();
        InvalidateRect(Dialog, &R, false);
        bNeedRepaint = false;
    }

    // only do node operation when a material is selected
    if (CurrentMaterialIndex > -1)
    {
        TryAddNodes();
        TryDeleteNodes();
        TryDisconnectPin();
        TryMoveNodes();
        TryConnectNodes();
        DrawPinConnectionLine();
        ProcessPopMenu();
    }

    PrevMousePos = MousePos;
    RawInput.CacheKeyStates();
}

void UHMaterialDialog::ResetDialogWindow()
{
    // always put the dialog on the right side of main window
    RECT MainWndRect;
    GetClientRect(Dialog, &MainWndRect);
    ClientToScreen(Dialog, (POINT*)&MainWndRect.left);
    ClientToScreen(Dialog, (POINT*)&MainWndRect.right);

    WindowPos = ImVec2((float)MainWndRect.left, (float)MainWndRect.top);
    WindowWidth = 250.0f;
    WindowHeight = static_cast<float>(MainWndRect.bottom - MainWndRect.top);
    bResetWindow = true;
}

void UHMaterialDialog::Init()
{
    Dialog = CreateDialog(Instance, MAKEINTRESOURCE(IDD_MATERIAL), ParentWindow, (DLGPROC)GDialogProc);
    Dialog = Dialog;
    RawInput.ResetMouseState();
    GPinSelectInfo = MakeUnique<UHPinSelectInfo>();

    HWND Wnd = Dialog;
    RegisterUniqueActiveDialog(IDD_MATERIAL, this);

    WorkAreaGUI = MakeUnique<UHGroupBox>(GetDlgItem(Wnd, IDC_MATERIAL_GRAPHAREA), UHGUIProperty().SetAutoSize(UHAutoSizeMethod::AutoSizeBoth));
    RECT R = WorkAreaGUI->GetCurrentRelativeRect();
    CreateWorkAreaMemDC((int32_t)(R.right - R.left), (int32_t)(R.bottom - R.top));

    // reset material selection
    CurrentMaterialIndex = UHINDEXNONE;
    CurrentMaterial = nullptr;
}

void UHMaterialDialog::OnFinished()
{
    DeleteDC(WorkAreaMemDC);
    DeleteObject(WorkAreaBmp);
    GPinSelectInfo.reset();
}

void UHMaterialDialog::OnMenuHit(uint32_t wParamLow)
{
    NodeMenuAction = static_cast<int32_t>(wParamLow);
}

void UHMaterialDialog::OnResizing()
{
    // resize bitmap
    DeleteDC(WorkAreaMemDC);
    DeleteObject(WorkAreaBmp);

    RECT R = WorkAreaGUI->GetCurrentRelativeRect();
    CreateWorkAreaMemDC((int32_t)(R.right - R.left), (int32_t)(R.bottom - R.top));

    bNeedRepaint = true;
    ResetDialogWindow();
}

void UHMaterialDialog::OnDrawing(HDC Hdc)
{
    // copy bitmap to work area
    RECT R = WorkAreaGUI->GetCurrentRelativeRect();
    BitBlt(Hdc, R.left, R.top, R.right - R.left, R.bottom - R.top, WorkAreaMemDC, 0, 0, SRCCOPY);
}

void UHMaterialDialog::SelectMaterial(int32_t MatIndex)
{
    bIsInitializing = true;
    CurrentMaterial = AssetManager->GetMaterials()[MatIndex];

    // initialize GUI
    for (UniquePtr<UHGraphNodeGUI>& GUI : EditNodeGUIs)
    {
        GUI.reset();
    }
    EditNodeGUIs.clear();

    // init node GUI
    POINT MaterialNodePos = CurrentMaterial->GetDefaultMaterialNodePos();
    EditNodeGUIs.push_back(std::move(MakeUnique<UHMaterialNodeGUI>()));
    EditNodeGUIs[0]->Init(Instance, WorkAreaGUI->GetHwnd(), CurrentMaterial->GetMaterialNode().get(), "Material Inputs", MaterialNodePos.x, MaterialNodePos.y);

    const std::vector<UniquePtr<UHGraphNode>>& EditNodes = CurrentMaterial->GetEditNodes();
    const std::vector<POINT>& GUIRelativePos = CurrentMaterial->GetGUIRelativePos();

    for (size_t Idx = 0; Idx < EditNodes.size(); Idx++)
    {
        NodeMenuAction = UH_ENUM_VALUE(EditNodes[Idx].get()->GetType());
        TryAddNodes(EditNodes[Idx].get(), GUIRelativePos[Idx]);
    }

    // mark pin button state for both material node and edit nodes
    for (UniquePtr<UHGraphPin>& Input : CurrentMaterial->GetMaterialNode()->GetInputs())
    {
        if (UHGraphPin* Pin = Input->GetSrcPin())
        {
            Input->GetPinGUI()->Checked(true);
            Pin->GetPinGUI()->Checked(true);
        }
    }

    for (UniquePtr<UHGraphNode>& Node : CurrentMaterial->GetEditNodes())
    {
        for (UniquePtr<UHGraphPin>& Input : Node->GetInputs())
        {
            if (UHGraphPin* Pin = Input->GetSrcPin())
            {
                Input->GetPinGUI()->Checked(true);
                Pin->GetPinGUI()->Checked(true);
            }
        }
    }

    bIsInitializing = false;
}

void UHMaterialDialog::TryAddNodes(UHGraphNode* InputNode, POINT GUIRelativePos)
{
    // Node menu action for adding could be from individual node type
    // return if it's not adding nodes, this also means AddNode needs to be put the bottom of UHNodeMenuAction
    if (NodeMenuAction < UH_ENUM_VALUE(UHNodeMenuAction::AddNode))
    {
        return;
    }

    UniquePtr<UHGraphNode> NewNode = AllocateNewGraphNode(static_cast<UHGraphNodeType>(NodeMenuAction));
    UniquePtr<UHGraphNodeGUI> NewGUI;
    std::string GUIName = "";

    switch (NodeMenuAction)
    {
    case UH_ENUM_VALUE(UHGraphNodeType::FloatNode):
        NewGUI = MakeUnique<UHFloatNodeGUI>(Renderer, CurrentMaterial);
        GUIName = "Float";
        break;
    case UH_ENUM_VALUE(UHGraphNodeType::Float2Node):
        NewGUI = MakeUnique<UHFloat2NodeGUI>(Renderer, CurrentMaterial);
        GUIName = "Float2";
        break;
    case UH_ENUM_VALUE(UHGraphNodeType::Float3Node):
        NewGUI = MakeUnique<UHFloat3NodeGUI>(Renderer, CurrentMaterial);
        GUIName = "Float3";
        break;
    case UH_ENUM_VALUE(UHGraphNodeType::Float4Node):
        NewGUI = MakeUnique<UHFloat4NodeGUI>(Renderer, CurrentMaterial);
        GUIName = "Float4";
        break;
    case UH_ENUM_VALUE(UHGraphNodeType::MathNode):
        NewGUI = MakeUnique<UHMathNodeGUI>();
        GUIName = "Math";
        break;
    case UH_ENUM_VALUE(UHGraphNodeType::Texture2DNode):
        NewGUI = MakeUnique<UHTexture2DNodeGUI>(AssetManager, Renderer, CurrentMaterial);
        GUIName = "Texture2D";
        break;
    }

    // add GUI only if there is an input node, otherwise adding a new node
    HWND WorkArea = WorkAreaGUI->GetHwnd();
    if (InputNode)
    {
        POINT P = CurrentMaterial->GetDefaultMaterialNodePos();
        NewNode.reset();
        NewGUI->Init(Instance, WorkArea, InputNode, GUIName, P.x + GUIRelativePos.x, P.y + GUIRelativePos.y);

        EditNodeGUIs.push_back(std::move(NewGUI));
    }
    else if (NewNode)
    {
        CurrentMaterial->GetEditNodes().push_back(std::move(NewNode));

        POINT P = MousePosWhenRightDown;
        ScreenToClient(WorkArea, &P);
        NewGUI->Init(Instance, WorkArea, CurrentMaterial->GetEditNodes().back().get(), GUIName, P.x, P.y);

        // syc the gui relative pos as well
        RECT R;
        UHEditorUtil::GetWindowSize(NewGUI->GetHWND(), R, EditNodeGUIs[0]->GetHWND());
        P.x = R.left;
        P.y = R.top;
        CurrentMaterial->GetGUIRelativePos().push_back(P);

        EditNodeGUIs.push_back(std::move(NewGUI));
    }

    // invalidate the newly added edit node rect
    RECT R;
    UHEditorUtil::GetWindowSize(EditNodeGUIs.back()->GetHWND(), R, Dialog);
    InvalidateRect(Dialog, &R, false);

    CurrentMaterial->bIsMaterialNodeDirty = !bIsInitializing;
    NodeMenuAction = UH_ENUM_VALUE(UHNodeMenuAction::NoAction);
}

void UHMaterialDialog::TryDeleteNodes()
{
    if (NodeMenuAction != UH_ENUM_VALUE(UHNodeMenuAction::Deletion) || NodeToDelete == nullptr)
    {
        return;
    }

    // disconnect all input pin GUI
    for (UniquePtr<UHGraphPin>& InputPin : NodeToDelete->GetInputs())
    {
        if (UHGraphPin* SrcPin = InputPin->GetSrcPin())
        {
            SrcPin->Disconnect(InputPin->GetId());
            if (SrcPin->GetDestPins().size() == 0)
            {
                SrcPin->GetPinGUI()->Checked(false);
            }
        }
    }

    // disconnect all output pin GUI
    for (UniquePtr<UHGraphPin>& OutputPin : NodeToDelete->GetOutputs())
    {
        for (UHGraphPin* DestPin : OutputPin->GetDestPins())
        {
            DestPin->Disconnect();
            DestPin->GetPinGUI()->Checked(false);
        }
    }

    // delete node
    for (int32_t Idx = static_cast<int32_t>(CurrentMaterial->GetEditNodes().size()) - 1; Idx >= 0; Idx--)
    {
        if (CurrentMaterial->GetEditNodes()[Idx]->GetId() == NodeToDelete->GetId())
        {
            UHUtilities::RemoveByIndex(CurrentMaterial->GetGUIRelativePos(), Idx);
            UHUtilities::RemoveByIndex(CurrentMaterial->GetEditNodes(), Idx);
            break;
        }
    }

    // also delete GUI
    for (int32_t Idx = static_cast<int32_t>(EditNodeGUIs.size()) - 1; Idx >= 0; Idx--)
    {
        if (EditNodeGUIs[Idx]->GetNode()->GetId() == NodeToDelete->GetId())
        {
            UHUtilities::RemoveByIndex(EditNodeGUIs, Idx);
            break;
        }
    }

    CurrentMaterial->bIsMaterialNodeDirty = !bIsInitializing;
    NodeMenuAction = UH_ENUM_VALUE(UHNodeMenuAction::NoAction);
    NodeToDelete = nullptr;
    bNeedRepaint = true;
}

void UHMaterialDialog::TryDisconnectPin()
{
    if (NodeMenuAction != UH_ENUM_VALUE(UHNodeMenuAction::Disconnect) || PinToDisconnect == nullptr)
    {
        return;
    }

    // disconnect an input pin
    PinToDisconnect->GetPinGUI()->Checked(false);
    if (PinToDisconnect->GetSrcPin())
    {
        PinToDisconnect->GetSrcPin()->Disconnect(PinToDisconnect->GetId());
        if (PinToDisconnect->GetSrcPin()->GetDestPins().size() == 0)
        {
            PinToDisconnect->GetSrcPin()->GetPinGUI()->Checked(false);
        }
        PinToDisconnect->Disconnect();
    }
    else
    {
        // disconnect an output pin
        for (UHGraphPin* DestPin : PinToDisconnect->GetDestPins())
        {
            DestPin->GetPinGUI()->Checked(false);
            PinToDisconnect->Disconnect(DestPin->GetId());
            DestPin->Disconnect();
        }
    }

    CurrentMaterial->bIsMaterialNodeDirty = !bIsInitializing;
    NodeMenuAction = UH_ENUM_VALUE(UHNodeMenuAction::NoAction);
    PinToDisconnect = nullptr;
    bNeedRepaint = true;
}

void UHMaterialDialog::TryMoveNodes()
{
    // select node to move first
    if (RawInput.IsLeftMouseDown())
    {
        // move one GUI at the same time
        for (UniquePtr<UHGraphNodeGUI>& GUI : EditNodeGUIs)
        {
            if (GUI->IsPointInside(MousePos))
            {
                GUIToMove = GUI->GetHWND();
                break;
            }
        }
    }
    else if (RawInput.IsLeftMouseUp())
    {
        GUIToMove = nullptr;
    }

    // function for move GUI
    HWND WorkArea = WorkAreaGUI->GetHwnd();
    auto MoveGUI = [WorkArea](HWND GUIToMove, uint32_t Dx, uint32_t Dy)
    {
        RECT R;
        UHEditorUtil::GetWindowSize(GUIToMove, R, WorkArea);
        MoveWindow(GUIToMove, R.left + Dx, R.top + Dy, R.right - R.left, R.bottom - R.top, false);
    };

    // cache mouse movement before doing any operations
    uint32_t MouseDeltaX;
    uint32_t MouseDeltaY;
    RawInput.GetMouseDelta(MouseDeltaX, MouseDeltaY);

    if (WorkAreaGUI->IsPointInside(MousePos))
    {
        if (RawInput.IsLeftMouseHold() && GUIToMove != nullptr)
        {
            RECT OldRect;
            UHEditorUtil::GetWindowSize(GUIToMove, OldRect, Dialog);

            // erase old lines
            bIsUpdatingDragLine = true;
            DragRect = OldRect;
            DrawPinConnectionLine(true);

            // move node
            MoveGUI(GUIToMove, MouseDeltaX, MouseDeltaY);

            RECT NewRect;
            UHEditorUtil::GetWindowSize(GUIToMove, NewRect, Dialog);

            // invalidate the node rect
            NewRect.left = std::min(NewRect.left, OldRect.left) - 10;
            NewRect.top = std::min(NewRect.top, OldRect.top) - 10;
            NewRect.right = std::max(NewRect.right, OldRect.right) + 10;
            NewRect.bottom = std::max(NewRect.bottom, OldRect.bottom) + 10;
            InvalidateRect(Dialog, &NewRect, false);

            // invalidate the line drawing after moving
            bIsUpdatingDragLine = true;
            DragRect = NewRect;
            DrawPinConnectionLine();
        }
        else if (RawInput.IsRightMouseHold())
        {
            // move all nodes as like we're moving the view client
            for (UniquePtr<UHGraphNodeGUI>& GUI : EditNodeGUIs)
            {
                MoveGUI(GUI->GetHWND(), MouseDeltaX, MouseDeltaY);
            }

            bNeedRepaint = true;
        }
    }
}

void UHMaterialDialog::TryConnectNodes()
{
    if (!GPinSelectInfo->bReadyForConnect)
    {
        return;
    }

    // ready for connect, find the GUI we're connecting to
    for (UniquePtr<UHGraphNodeGUI>& NodeGUI : EditNodeGUIs)
    {
        // can not connect to self of course..
        if (NodeGUI->GetNode() == GPinSelectInfo->CurrOutputPin->GetOriginNode())
        {
            continue;
        }

        int32_t DestPinIndex;
        if (UHGraphPin* DestPin = NodeGUI->GetInputPinByMousePos(GPinSelectInfo->MouseUpPos, DestPinIndex))
        {
            UHGraphPin* OldSrcPin = DestPin->GetSrcPin();
            bool bConnectSucceed = DestPin->ConnectFrom(GPinSelectInfo->CurrOutputPin);

            if (bConnectSucceed)
            {
                // mark radio button
                GPinSelectInfo->CurrOutputPin->GetPinGUI()->Checked(true);
                DestPin->GetPinGUI()->Checked(true);

                if (OldSrcPin && OldSrcPin->GetDestPins().size() == 0)
                {
                    OldSrcPin->GetPinGUI()->Checked(false);
                }
            }
        }
    }

    CurrentMaterial->bIsMaterialNodeDirty = !bIsInitializing;

    // clear the selection in the end
    GPinSelectInfo->bReadyForConnect = false;
    GPinSelectInfo->CurrOutputPin = nullptr;
    bNeedRepaint = true;
}

void UHMaterialDialog::ProcessPopMenu()
{
    if (RawInput.IsRightMouseUp() && WorkAreaGUI->IsPointInside(MousePos))
    {
        if (MousePosWhenRightDown.x == MousePos.x && MousePosWhenRightDown.y == MousePos.y)
        {
            // see if there is a node to delete
            for (UniquePtr<UHGraphNodeGUI>& GUI : EditNodeGUIs)
            {
                if (GUI->IsPointInside(MousePos) && GUI->GetNode()->CanBeDeleted())
                {
                    NodeToDelete = GUI->GetNode();
                    break;
                }
            }

            NodeFunctionMenu.SetOptionActive(1, NodeToDelete != nullptr);
            NodeFunctionMenu.ShowMenu(Dialog, MousePos.x, MousePos.y);
        }
    }
    else if (GPinSelectInfo->RightClickedPin != nullptr)
    {
        // process pin disconnect menu
        NodePinMenu.ShowMenu(Dialog, MousePos.x, MousePos.y);
        PinToDisconnect = GPinSelectInfo->RightClickedPin;
        GPinSelectInfo->RightClickedPin = nullptr;
    }
}

void UHMaterialDialog::DrawPinConnectionLine(bool bIsErasing)
{
    SelectObject(WorkAreaMemDC, WorkAreaBmp);
    Graphics Graphics(WorkAreaMemDC);
    Pen Pen(Color(255, 0, 0, 255));
    Graphics.Clear(Color(255, 240, 240, 240));

    // draw line when user is dragging
    HWND WorkArea = WorkAreaGUI->GetHwnd();

    if (GPinSelectInfo->CurrOutputPin)
    {
        // draw line, point is relative to window 
        POINT P1 = GPinSelectInfo->MouseDownPos;
        POINT P2 = MousePos;
        ScreenToClient(WorkArea, &P1);
        ScreenToClient(WorkArea, &P2);

        Graphics.DrawLine(&Pen, (int32_t)P1.x, (int32_t)P1.y, (int32_t)P2.x, (int32_t)P2.y);

        P1 = GPinSelectInfo->MouseDownPos;
        P2 = MousePos;
        POINT P3 = PrevMousePos;
        ScreenToClient(Dialog, &P1);
        ScreenToClient(Dialog, &P2);
        ScreenToClient(Dialog, &P3);

        DragRect.left = std::min(P1.x, P2.x);
        DragRect.right = std::max(P1.x, P2.x);
        DragRect.top = std::min(P1.y, P2.y);
        DragRect.bottom = std::max(P1.y, P2.y);
 
        DragRect.left = std::min(DragRect.left, P3.x) - 10;
        DragRect.right = std::max(DragRect.right, P3.x) + 10;
        DragRect.top = std::min(DragRect.top, P3.y) - 10;
        DragRect.bottom = std::max(DragRect.bottom, P3.y) + 10;

        // only invalidate the rect passed by the line, include previous mouse position
        InvalidateRect(Dialog, &DragRect, false);
        bIsUpdatingDragLine = true;
    }

    // draw line for connected pins
    for (UniquePtr<UHGraphNodeGUI>& NodeGUI : EditNodeGUIs)
    {
        UHGraphNode* SrcNode = NodeGUI->GetNode();

        const std::vector<UniquePtr<UHGraphPin>>& Inputs = SrcNode->GetInputs();
        for (int32_t Idx = 0; Idx < static_cast<int32_t>(Inputs.size()); Idx++)
        {
            // only draw when the pin is connected
            if (UHGraphPin* Pin = Inputs[Idx]->GetSrcPin())
            {
                HWND Src = Pin->GetPinGUI()->GetHwnd();
                HWND Dst = NodeGUI->GetInputPin(Idx)->GetHwnd();

                RECT R1{};
                RECT R2{};
                GetWindowRect(Src, &R1);
                GetWindowRect(Dst, &R2);

                POINT P1{};
                POINT P2{};
                P1.x = R1.right;
                P1.y = (R1.top + R1.bottom) / 2;
                P2.x = R2.left;
                P2.y = (R2.top + R2.bottom) / 2;

                POINT P3 = P1;
                POINT P4 = P2;

                ScreenToClient(WorkArea, &P1);
                ScreenToClient(WorkArea, &P2);
                ScreenToClient(Dialog, &P3);
                ScreenToClient(Dialog, &P4);

                if (!bIsErasing)
                {
                    Graphics.DrawLine(&Pen, (int32_t)P1.x, (int32_t)P1.y, (int32_t)P2.x, (int32_t)P2.y);
                }

                // invalidate rect when necessary
                RECT R;
                R.left = std::min(P3.x, P4.x) - 10;
                R.right = std::max(P3.x, P4.x) + 10;
                R.top = std::min(P3.y, P4.y) - 10;
                R.bottom = std::max(P3.y, P4.y) + 10;

                RECT Dummy;
                if (bIsUpdatingDragLine && IntersectRect(&Dummy, &R, &DragRect))
                {
                    InvalidateRect(Dialog, &R, false);
                }
            }
        }
    }

    bIsUpdatingDragLine = false;
}

void UHMaterialDialog::ControlRecompileMaterial()
{
    if (CurrentMaterialIndex == UHINDEXNONE)
    {
        return;
    }

    UHMaterial* Mat = AssetManager->GetMaterials()[CurrentMaterialIndex];
    Mat->SetCompileFlag(UHMaterialCompileFlag::FullCompileTemporary);
    RecompileMaterial(CurrentMaterialIndex, false);
    CurrentMaterial->bIsMaterialNodeDirty = false;
}

void UHMaterialDialog::ControlResaveMaterial()
{
    if (CurrentMaterialIndex == UHINDEXNONE)
    {
        return;
    }

    {
        UHStatusDialogScope Status("Saving...");
        UHMaterial* Mat = AssetManager->GetMaterials()[CurrentMaterialIndex];
        Mat->SetCompileFlag(UHMaterialCompileFlag::FullCompileResave);
        ResaveMaterial(CurrentMaterialIndex, false);
    }
    MessageBoxA(Dialog, "Current editing material is saved.", "Material Editor", MB_OK);
}

void UHMaterialDialog::ControlResaveAllMaterials()
{
    ResaveAllMaterials();
}

void UHMaterialDialog::ControlCullMode(const int32_t InCullMode)
{
    if (CurrentMaterialIndex == UHINDEXNONE)
    {
        return;
    }

    UHMaterial* Mat = AssetManager->GetMaterials()[CurrentMaterialIndex];
    Mat->SetCullMode(static_cast<UHCullMode>(InCullMode));

    // changing cull mode doesn't need a recompiling, just refresh the material shader so they will recreate graphic state
    Mat->SetCompileFlag(UHMaterialCompileFlag::StateChangedOnly);
    Renderer->RefreshMaterialShaders(Mat, false, false);
}

void UHMaterialDialog::ControlBlendMode(const int32_t InBlendMode)
{
    if (CurrentMaterialIndex == UHINDEXNONE)
    {
        return;
    }

    // toggling blend mode does a little more than cull mode. it not only needs re-compiling, but also the renderer list refactoring
    // for example: when an object goes alpha blend from masked object, it needs to be in TranslucentRenderer list instead of OpaqueRenderer list
    UHMaterial* Mat = AssetManager->GetMaterials()[CurrentMaterialIndex];

    const UHBlendMode OldMode = Mat->GetBlendMode();
    const UHBlendMode NewMode = static_cast<UHBlendMode>(InBlendMode);

    Mat->SetBlendMode(NewMode);
    bool bRenderGroupChanged = false;
    if ((UH_ENUM_VALUE(OldMode) / UH_ENUM_VALUE(UHBlendMode::TranditionalAlpha)) != (UH_ENUM_VALUE(NewMode) / UH_ENUM_VALUE(UHBlendMode::TranditionalAlpha)))
    {
        bRenderGroupChanged = true;
    }

    Mat->SetCompileFlag(UHMaterialCompileFlag::FullCompileTemporary);
    Renderer->RefreshMaterialShaders(Mat, bRenderGroupChanged, false);
}

void UHMaterialDialog::RecompileMaterial(int32_t MatIndex, bool bDelayRTShaderCreation)
{
    if (MatIndex == UHINDEXNONE)
    {
        return;
    }

    if (CurrentMaterialIndex == MatIndex)
    {
        for (UniquePtr<UHGraphNodeGUI>& GUI : EditNodeGUIs)
        {
            GUI->SetDefaultValueFromGUI();
        }
    }

    UHMaterial* Mat = AssetManager->GetMaterials()[MatIndex];
    
    bool bNeedRefreshShaders = false;
    for (const UHObject* Obj : Mat->GetReferenceObjects())
    {
        if (Obj->GetObjectClassId() == UHMeshRendererComponent::ClassId)
        {
            bNeedRefreshShaders = true;
        }
    }

    // only refresh shaders when the material is actually used in rendering
    if (bNeedRefreshShaders)
    {
        Renderer->RefreshMaterialShaders(Mat, false, bDelayRTShaderCreation);
    }
}

void UHMaterialDialog::ResaveMaterial(int32_t MatIndex, bool bDelayRTShaderCreation)
{
    if (MatIndex == UHINDEXNONE)
    {
        return;
    }

    if (CurrentMaterialIndex == MatIndex)
    {
        // GUI sync if it's the current material
        RECT Rect;
        UHEditorUtil::GetWindowSize(EditNodeGUIs[0]->GetHWND(), Rect, WorkAreaGUI->GetHwnd());
        POINT Pos{};
        Pos.x = Rect.left;
        Pos.y = Rect.top;
        CurrentMaterial->SetDefaultMaterialNodePos(Pos);

        std::vector<POINT> EditGUIPos;
        for (size_t Idx = 1; Idx < EditNodeGUIs.size(); Idx++)
        {
            UHEditorUtil::GetWindowSize(EditNodeGUIs[Idx]->GetHWND(), Rect, EditNodeGUIs[0]->GetHWND());
            Pos.x = Rect.left;
            Pos.y = Rect.top;
            EditGUIPos.push_back(Pos);
        }
        CurrentMaterial->SetGUIRelativePos(EditGUIPos);

        // sync value from GUI
        for (UniquePtr<UHGraphNodeGUI>& GUI : EditNodeGUIs)
        {
            GUI->SetDefaultValueFromGUI();
        }
    }

    UHMaterial* Mat = AssetManager->GetMaterials()[MatIndex];
    Mat->Export();

    // request a compile after exporting, so the shader cache will get proper material file mod time
    RecompileMaterial(MatIndex, bDelayRTShaderCreation);
    Mat->bIsMaterialNodeDirty = false;
}

void UHMaterialDialog::ResaveAllMaterials()
{
    {
        UHStatusDialogScope Status("Saving...");
        for (int32_t Idx = 0; Idx < static_cast<int32_t>(AssetManager->GetMaterials().size()); Idx++)
        {
            AssetManager->GetMaterials()[Idx]->SetCompileFlag(UHMaterialCompileFlag::FullCompileResave);
            ResaveMaterial(Idx, true);

            // need to mark the resave flag again for RT shaders
            AssetManager->GetMaterials()[Idx]->SetCompileFlag(UHMaterialCompileFlag::FullCompileResave);
        }

        // recreate RT shaders in one go, much faster
        Renderer->RecreateRTShaders(AssetManager->GetMaterials(), false);
        Renderer->UpdateDescriptors();
    }

    MessageBoxA(Dialog, "All materials are saved.", "Material Editor", MB_OK);
}

#endif