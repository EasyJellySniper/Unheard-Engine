#pragma once
#include "../../UnheardEngine.h"
#include <unordered_map>
#include "../Controls/GUIControl.h"
#include "../../Runtime/Engine/Input.h"

// UH shared Dialog class, needs to implement ShowDialog in child class
class UHDialog
{
public:
	UHDialog(HINSTANCE InInstance, HWND InWindow);
	virtual ~UHDialog() {}

	virtual void ShowDialog() { bIsOpened = true; }
	virtual void Close() { bIsOpened = false; }

	HWND GetDialog() const;

	// callbacks
	std::vector<std::function<void()>> OnDestroy;
	std::vector<std::function<void(uint32_t)>> OnMenuClicked;
	std::vector<std::function<void()>> OnResized;
	std::vector<std::function<void()>> OnMoved;
	std::vector<std::function<void(HDC)>> OnPaint;

	UHRawInput RawInput;

protected:
	HINSTANCE Instance;
	HWND ParentWindow;
	HWND Dialog;
	bool bIsOpened;
};

// shared global dialog proc
extern INT_PTR CALLBACK GDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
extern std::unordered_map<int32_t, UHDialog*> GActiveDialogTable;
extern std::unordered_map<HWND, UHGUIControlBase*> GControlGUITable;

// register unique active dialog
inline void RegisterUniqueActiveDialog(int32_t Id, UHDialog* InDialog)
{
	GActiveDialogTable[Id] = InDialog;
}

inline bool IsDialogActive(int32_t Id)
{
	return GActiveDialogTable.find(Id) != GActiveDialogTable.end();
}