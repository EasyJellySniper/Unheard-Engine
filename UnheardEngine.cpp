// UnheardEngine.cpp : Defines the entry point for the application.
//

#include "UnheardEngine.h"
#include "framework.h"
#include "resource.h"
#include "Runtime/Engine/Engine.h"
#include "Runtime/Engine/Input.h"
#include "Editor/Dialog/StatusDialog.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND UHEngineWindow;                            // engine window
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// Unheard Engine Instance
UniquePtr<UHEngine> GUnheardEngine = nullptr;
bool GIsMinimized = false;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
#if WITH_DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_UNHEARDENGINE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    // setup rand seed
    srand((unsigned int)time(NULL));

    // Create engine instance and initialize with config settings
    CoInitialize(nullptr);

    {
        UHStatusDialogScope StatusDialog(hInstance, UHEngineWindow, "Loading...");
        GUnheardEngine = MakeUnique<UHEngine>();
        GUnheardEngine->LoadConfig();

        if (!GUnheardEngine->InitEngine(hInst, UHEngineWindow))
        {
            UHE_LOG(L"Engine creation failed!\n");
            GUnheardEngine->ReleaseEngine();
            GUnheardEngine.reset();
            return FALSE;
        }
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_UNHEARDENGINE));

    MSG msg = {};

    // Main message loop, use peek message instead of get message. Otherwise it will pause when there is no message and drop FPS
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // call the game loop
            if (GUnheardEngine)
            {
            #if WITH_DEBUG
                // profile does not contain editor update time
                GUnheardEngine->GetEditor()->OnEditorUpdate();
                GUnheardEngine->BeginProfile();
            #endif
                GUnheardEngine->BeginFPSLimiter();

                // update despite it's minimized (can be opt out in the future)
                GUnheardEngine->Update();

                // only render when it's not minimized
                if (!GIsMinimized)
                {
                    GUnheardEngine->RenderLoop();
                }

                GUnheardEngine->EndFPSLimiter();
            #if WITH_DEBUG
                GUnheardEngine->EndProfile();
            #endif
            }
        }
    }
    CoUninitialize();

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_UNHEARDENGINE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
#if WITH_DEBUG
    // show default menu when debug only
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_UNHEARDENGINE);
#endif
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   UHEngineWindow = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!UHEngineWindow)
   {
      return FALSE;
   }

   // do not show window at the beginning
   //ShowWindow(UHEngineWindow, nCmdShow);
   //UpdateWindow(UHEngineWindow);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
        #if WITH_DEBUG
            GUnheardEngine->GetEditor()->OnMenuSelection(wmId);
        #endif

            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
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
        // save config and release engine when destroy
        if (GUnheardEngine)
        {
            GUnheardEngine->SaveConfig();
            GUnheardEngine->ReleaseEngine();
            GUnheardEngine.reset();
        }

        PostQuitMessage(0);
        break;

    case WM_SIZE:
        // deal with minimize cases
        if (wParam == SIZE_MINIMIZED)
        {
            GIsMinimized = true;
        }
        else
        {
            GIsMinimized = false;
            if (GUnheardEngine)
            {
                GUnheardEngine->SetResizeReason(UHEngineResizeReason::FromWndMessage);
            }
        }
        break;

    case WM_INPUT:
        if (GUnheardEngine)
        {
            if (UHRawInput* RawInput = GUnheardEngine->GetRawInput())
            {
                RawInput->ParseInputData(lParam);
            }
        }
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
