#include "MaterialDialog.h"

#if WITH_EDITOR
#include "../../Resource.h"
#include "../../Runtime/Engine/Asset.h"
#include "../Classes/EditorUtils.h"
#include "../Classes/ParameterNodeGUI.h"
#include "../Classes/MathNodeGUI.h"
#include "../Classes/TextureNodeGUI.h"
#include "../../Runtime/Classes/Material.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"

enum UHNodeMenuAction
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
    , NodeMenuAction(UHNodeMenuAction::NoAction)
    , bNeedRepaint(false)
    , CurrentMaterialIndex(-1)
    , CurrentMaterial(nullptr)
    , GdiplusToken(0)
    , WorkAreaBmp(nullptr)
    , WorkAreaMemDC(nullptr)
    , bIsUpdatingDragLine(false)
    , DragRect(RECT())
{
    // create popup menu for node functions, only do these in construction time
    ParameterMenu.InsertOption("Float", UHGraphNodeType::Float);
    ParameterMenu.InsertOption("Float2", UHGraphNodeType::Float2);
    ParameterMenu.InsertOption("Float3", UHGraphNodeType::Float3);
    ParameterMenu.InsertOption("Float4", UHGraphNodeType::Float4);

    TextureMenu.InsertOption("Texture2D", UHGraphNodeType::Texture2DNode);

    AddNodeMenu.InsertOption("Parameter", 0, ParameterMenu.GetMenu());
    AddNodeMenu.InsertOption("Math", UHGraphNodeType::MathNode);
    AddNodeMenu.InsertOption("Texture", 0, TextureMenu.GetMenu());

    NodeFunctionMenu.InsertOption("Add node", 0, AddNodeMenu.GetMenu());
    NodeFunctionMenu.InsertOption("Delete node", UHNodeMenuAction::Deletion);
    NodePinMenu.InsertOption("Disconnect", UHNodeMenuAction::Disconnect);

    GdiplusStartup(&GdiplusToken, &GdiplusStartupInput, NULL);

    OnDestroy.push_back(StdBind(&UHMaterialDialog::OnFinished, this));
    OnMenuClicked.push_back(StdBind(&UHMaterialDialog::OnMenuHit, this, std::placeholders::_1));
    OnPaint.push_back(StdBind(&UHMaterialDialog::OnDrawing, this, std::placeholders::_1));
    OnResized.push_back(StdBind(&UHMaterialDialog::OnResizing, this));
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
    }
}

void UHMaterialDialog::Update()
{
    if (!IsDialogActive(IDD_MATERIAL))
    {
        return;
    }

    // call the update function of all GUI nodes
    for (const UniquePtr<UHGraphNodeGUI>& GUI : EditNodeGUIs)
    {
        GUI->Update();
    }

    // store mouse states
    GetCursorPos(&MousePos);
    if (RawInput.IsRightMouseDown())
    {
        MousePosWhenRightDown = MousePos;
    }

    // get current selected material index
    const int32_t MatIndex = MaterialListGUI->GetSelectedIndex();
    if (MatIndex != CurrentMaterialIndex)
    {
        // select material
        SelectMaterial(MatIndex);
        CurrentMaterialIndex = MatIndex;
        bNeedRepaint = true;
    }

    // force a invalidate if it needs repainting
    if (bNeedRepaint)
    {
        RECT R = WorkAreaGUI->GetCurrentRelativeRect();
        InvalidateRect(Dialog, &R, false);
        bNeedRepaint = false;
    }

    // only do node operation when a material is selected
    if (MatIndex > -1)
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

void UHMaterialDialog::Init()
{
    Dialog = CreateDialog(Instance, MAKEINTRESOURCE(IDD_MATERIAL), ParentWindow, (DLGPROC)GDialogProc);
    Dialog = Dialog;
    RawInput.ResetMouseState();
    GPinSelectInfo = MakeUnique<UHPinSelectInfo>();

    HWND Wnd = Dialog;
    RegisterUniqueActiveDialog(IDD_MATERIAL, this);

    // controls init
    MaterialListGUI = MakeUnique<UHListBox>(GetDlgItem(Wnd, IDC_MATERIAL_LIST), UHGUIProperty().SetAutoSize(AutoSizeY));
    const std::vector<UHMaterial*>& Materials = AssetManager->GetMaterials();
    for (const UHMaterial* Mat : Materials)
    {
        MaterialListGUI->AddItem(Mat->GetName());
    }

    CompileButtonGUI = MakeUnique<UHButton>(GetDlgItem(Wnd, IDC_MATERIALCOMPILE), UHGUIProperty().SetAutoMove(AutoMoveY));
    CompileButtonGUI->OnClicked.push_back(StdBind(&UHMaterialDialog::ControlRecompileMaterial, this));

    SaveButtonGUI = MakeUnique<UHButton>(GetDlgItem(Wnd, IDC_MATERIALSAVE), UHGUIProperty().SetAutoMove(AutoMoveY));
    SaveButtonGUI->OnClicked.push_back(StdBind(&UHMaterialDialog::ControlResaveMaterial, this));

    SaveAllButtonGUI = MakeUnique<UHButton>(GetDlgItem(Wnd, IDC_MATERIALSAVEALL), UHGUIProperty().SetAutoMove(AutoMoveY));
    SaveAllButtonGUI->OnClicked.push_back(StdBind(&UHMaterialDialog::ControlResaveAllMaterials, this));

    CullTextGUI = MakeUnique<UHLabel>(GetDlgItem(Wnd, IDC_CULLTEXT), UHGUIProperty().SetAutoMove(AutoMoveY));
    CullModeGUI = MakeUnique<UHComboBox>(GetDlgItem(Wnd, IDC_MATERIAL_CULLMODE), UHGUIProperty().SetAutoMove(AutoMoveY));
    CullModeGUI->Init(L"None", GCullModeNames);
    CullModeGUI->OnSelected.push_back(StdBind(&UHMaterialDialog::ControlCullMode, this));

    BlendTextGUI = MakeUnique<UHLabel>(GetDlgItem(Wnd, IDC_BLENDTEXT), UHGUIProperty().SetAutoMove(AutoMoveY));
    BlendModeGUI = MakeUnique<UHComboBox>(GetDlgItem(Wnd, IDC_MATERIAL_BLENDMODE), UHGUIProperty().SetAutoMove(AutoMoveY));
    BlendModeGUI->Init(L"Opaque", GBlendModeNames);
    BlendModeGUI->OnSelected.push_back(StdBind(&UHMaterialDialog::ControlBlendMode, this));

    WorkAreaGUI = MakeUnique<UHGroupBox>(GetDlgItem(Wnd, IDC_MATERIAL_GRAPHAREA), UHGUIProperty().SetAutoSize(AutoSizeBoth));
    RECT R = WorkAreaGUI->GetCurrentRelativeRect();
    CreateWorkAreaMemDC((int32_t)(R.right - R.left), (int32_t)(R.bottom - R.top));

    // reset material selection
    CurrentMaterialIndex = -1;
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
}

void UHMaterialDialog::OnDrawing(HDC Hdc)
{
    // copy bitmap to work area
    RECT R = WorkAreaGUI->GetCurrentRelativeRect();
    BitBlt(Hdc, R.left, R.top, R.right - R.left, R.bottom - R.top, WorkAreaMemDC, 0, 0, SRCCOPY);
}

void UHMaterialDialog::SelectMaterial(int32_t MatIndex)
{
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
        NodeMenuAction = EditNodes[Idx].get()->GetType();
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

    // select cull mode and blend mode
    CullModeGUI->Select(GCullModeNames[CurrentMaterial->GetCullMode()]);
    BlendModeGUI->Select(GBlendModeNames[CurrentMaterial->GetBlendMode()]);
}

void UHMaterialDialog::TryAddNodes(UHGraphNode* InputNode, POINT GUIRelativePos)
{
    // Node menu action for adding could be from individual node type
    // return if it's not adding nodes, this also means AddNode needs to be put the bottom of UHNodeMenuAction
    if (NodeMenuAction < UHNodeMenuAction::AddNode)
    {
        return;
    }

    UniquePtr<UHGraphNode> NewNode = AllocateNewGraphNode(static_cast<UHGraphNodeType>(NodeMenuAction));
    UniquePtr<UHGraphNodeGUI> NewGUI;
    std::string GUIName = "";

    switch (NodeMenuAction)
    {
    case UHGraphNodeType::Float:
        NewGUI = MakeUnique<UHFloatNodeGUI>();
        GUIName = "Float";
        break;
    case UHGraphNodeType::Float2:
        NewGUI = MakeUnique<UHFloat2NodeGUI>();
        GUIName = "Float2";
        break;
    case UHGraphNodeType::Float3:
        NewGUI = MakeUnique<UHFloat3NodeGUI>();
        GUIName = "Float3";
        break;
    case UHGraphNodeType::Float4:
        NewGUI = MakeUnique<UHFloat4NodeGUI>();
        GUIName = "Float4";
        break;
    case UHGraphNodeType::MathNode:
        NewGUI = MakeUnique<UHMathNodeGUI>();
        GUIName = "Math";
        break;
    case UHGraphNodeType::Texture2DNode:
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

    NodeMenuAction = UHNodeMenuAction::NoAction;
}

void UHMaterialDialog::TryDeleteNodes()
{
    if (NodeMenuAction != UHNodeMenuAction::Deletion || NodeToDelete == nullptr)
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

    NodeMenuAction = UHNodeMenuAction::NoAction;
    NodeToDelete = nullptr;
    bNeedRepaint = true;
}

void UHMaterialDialog::TryDisconnectPin()
{
    if (NodeMenuAction != UHNodeMenuAction::Disconnect || PinToDisconnect == nullptr)
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

    NodeMenuAction = UHNodeMenuAction::NoAction;
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
    const int32_t MatIndex = MaterialListGUI->GetSelectedIndex();
    if (MatIndex == -1)
    {
        return;
    }

    UHMaterial* Mat = AssetManager->GetMaterials()[MatIndex];
    Mat->SetCompileFlag(FullCompileTemporary);
    RecompileMaterial(MatIndex);
}

void UHMaterialDialog::ControlResaveMaterial()
{
    const int32_t MatIndex = MaterialListGUI->GetSelectedIndex();
    if (MatIndex == -1)
    {
        return;
    }

    UHMaterial* Mat = AssetManager->GetMaterials()[MatIndex];
    Mat->SetCompileFlag(FullCompileResave);
    ResaveMaterial(MatIndex);
    MessageBoxA(Dialog, "Current editing material is saved.", "Material Editor", MB_OK);
}

void UHMaterialDialog::ControlResaveAllMaterials()
{
    ResaveAllMaterials();
}

void UHMaterialDialog::ControlCullMode()
{
    const int32_t MatIndex = MaterialListGUI->GetSelectedIndex();
    if (MatIndex == -1)
    {
        return;
    }

    const int32_t SelectedCullMode = CullModeGUI->GetSelectedIndex();
    UHMaterial* Mat = AssetManager->GetMaterials()[MatIndex];
    Mat->SetCullMode(static_cast<UHCullMode>(SelectedCullMode));

    // changing cull mode doesn't need a recompiling, just refresh the material shader so they will recreate graphic state
    Mat->SetCompileFlag(StateChangedOnly);
    Renderer->RefreshMaterialShaders(Mat);
}

void UHMaterialDialog::ControlBlendMode()
{
    const int32_t MatIndex = MaterialListGUI->GetSelectedIndex();
    if (MatIndex == -1)
    {
        return;
    }

    // toggling blend mode does a little more than cull mode. it not only needs re-compiling, but also the renderer list refactoring
    // for example: when an object goes alpha blend from masked object, it needs to be in TranslucentRenderer list instead of OpaqueRenderer list
    const int32_t SelectedBlendMode = BlendModeGUI->GetSelectedIndex();
    UHMaterial* Mat = AssetManager->GetMaterials()[MatIndex];

    const UHBlendMode OldMode = Mat->GetBlendMode();
    const UHBlendMode NewMode = static_cast<UHBlendMode>(SelectedBlendMode);

    Mat->SetBlendMode(NewMode);
    bool bRenderGroupChanged = false;
    if ((OldMode / UHBlendMode::TranditionalAlpha) != (NewMode / UHBlendMode::TranditionalAlpha))
    {
        bRenderGroupChanged = true;
    }

    Mat->SetCompileFlag(FullCompileTemporary);
    Renderer->RefreshMaterialShaders(Mat, bRenderGroupChanged);
}

void UHMaterialDialog::RecompileMaterial(int32_t MatIndex)
{
    if (MatIndex == -1)
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
    Renderer->RefreshMaterialShaders(Mat);
}

void UHMaterialDialog::ResaveMaterial(int32_t MatIndex)
{
    if (MatIndex == -1)
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

    AssetManager->GetMaterials()[MatIndex]->Export();

    // request a compile after exporting, so the shader cache will get proper material file mod time
    RecompileMaterial(MatIndex);
}

void UHMaterialDialog::ResaveAllMaterials()
{
    for (int32_t Idx = 0; Idx < static_cast<int32_t>(AssetManager->GetMaterials().size()); Idx++)
    {
        AssetManager->GetMaterials()[Idx]->SetCompileFlag(FullCompileResave);
        ResaveMaterial(Idx);
    }

    MessageBoxA(Dialog, "All materials are saved.", "Material Editor", MB_OK);
}

#endif