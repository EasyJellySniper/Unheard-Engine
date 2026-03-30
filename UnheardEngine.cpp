// UnheardEngine.cpp : Defines the entry point for the application.
#include "Runtime/Application.h"

#ifdef _WIN32
#include <Windows.h>
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
#else
int main()
{
#endif
    UHApplication App;
    return App.Run();
}
