#include "Platform.h"

// Windows platform
#ifdef _WIN32

#include "UnheardEngine.h"
#include "framework.h"
#include "resource.h"
#include "Runtime/Engine/Engine.h"
#include "Runtime/Platform/PlatformInput.h"
#include "Runtime/Platform/Client.h"
#include "../Application.h"
#include <objbase.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInstance;                            // current instance
HWND UHEngineWindow;                            // engine window
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, UHApplication* InApp);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

bool UHPlatform::Initialize(UHApplication* InApp)
{
    if (GIsEditor)
    {
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }

    hInstance = GetModuleHandle(nullptr);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_UNHEARDENGINE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, InApp))
    {
        return false;
    }

    // Create engine instance and initialize with config settings
    CoInitialize(nullptr);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_UNHEARDENGINE));

    Client = MakeUnique<UHClient>();
    Client->SetClientInstance(hInstance);
    Client->SetClientWindow(UHEngineWindow);

    return true;
}

void UHPlatform::Shutdown()
{
    CoUninitialize();
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_UNHEARDENGINE));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (GIsEditor)
    {
        // show default menu when debug only
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_UNHEARDENGINE);
    }
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, UHApplication* InApp)
{
    UHEngineWindow = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, InApp);

    if (!UHEngineWindow)
    {
        return FALSE;
    }

    // do not show window at the beginning
    //ShowWindow(UHEngineWindow, nCmdShow);
    //UpdateWindow(UHEngineWindow);

    return TRUE;
}

#if WITH_EDITOR
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#if WITH_EDITOR
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }
#endif

    UHApplication* UHEApp = (UHApplication*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    UHEngine* Engine = (UHEApp != nullptr) ? UHEApp->GetEngine() : nullptr;

    switch (message)
    {
    case WM_NCCREATE:
    {
        CREATESTRUCT* CS = (CREATESTRUCT*)lParam;
        UHApplication* App = (UHApplication*)CS->lpCreateParams;

        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)App);
        return TRUE;
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
#if WITH_EDITOR
        if (Engine)
        {
            Engine->GetEditor()->OnMenuSelection(wmId);
        }
#endif

        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        // deal with minimize cases
        if (wParam == SIZE_MINIMIZED)
        {
        }
        else
        {
            if (Engine)
            {
                Engine->SetResizeReason(UHEngineResizeReason::FromWndMessage);
            }
        }
#if WITH_EDITOR
        if (Engine && Engine->GetEditor())
        {
            Engine->GetEditor()->OnEditorResize();
        }
#endif
        break;

    case WM_INPUT:
        if (Engine)
        {
            if (UHPlatformInput* RawInput = Engine->GetRawInput())
            {
                RawInput->ParseInputData((void*)lParam);
            }
        }
        break;

    case WM_MOVE:
#if WITH_EDITOR
        if (Engine && Engine->GetEditor())
        {
            Engine->GetEditor()->OnEditorMove();
        }
#endif
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
#endif