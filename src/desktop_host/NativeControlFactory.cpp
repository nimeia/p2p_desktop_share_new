#include "pch.h"
#include "NativeControlFactory.h"

NativeControlFactory::NativeControlFactory(HWND parent, HINSTANCE instance)
    : parent_(parent), instance_(instance) {
}

HWND NativeControlFactory::CreateButton(std::wstring_view text,
                                        DWORD style,
                                        int x,
                                        int y,
                                        int width,
                                        int height,
                                        int controlId) const {
    return CreateWindowW(L"BUTTON",
                         std::wstring(text).c_str(),
                         WS_CHILD | WS_VISIBLE | style,
                         x,
                         y,
                         width,
                         height,
                         parent_,
                         controlId != 0 ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)) : nullptr,
                         instance_,
                         nullptr);
}

HWND NativeControlFactory::CreateStatic(std::wstring_view text,
                                        DWORD style,
                                        int x,
                                        int y,
                                        int width,
                                        int height,
                                        int controlId) const {
    return CreateWindowW(L"STATIC",
                         std::wstring(text).c_str(),
                         WS_CHILD | WS_VISIBLE | style,
                         x,
                         y,
                         width,
                         height,
                         parent_,
                         controlId != 0 ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)) : nullptr,
                         instance_,
                         nullptr);
}

HWND NativeControlFactory::CreateEdit(std::wstring_view text,
                                      DWORD style,
                                      int x,
                                      int y,
                                      int width,
                                      int height,
                                      int controlId,
                                      DWORD exStyle) const {
    return CreateWindowExW(exStyle,
                           L"EDIT",
                           std::wstring(text).c_str(),
                           WS_CHILD | WS_VISIBLE | style,
                           x,
                           y,
                           width,
                           height,
                           parent_,
                           controlId != 0 ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)) : nullptr,
                           instance_,
                           nullptr);
}

HWND NativeControlFactory::CreateCombo(DWORD style,
                                       int x,
                                       int y,
                                       int width,
                                       int height,
                                       int controlId) const {
    return CreateWindowW(L"COMBOBOX",
                         L"",
                         WS_CHILD | WS_VISIBLE | style,
                         x,
                         y,
                         width,
                         height,
                         parent_,
                         controlId != 0 ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)) : nullptr,
                         instance_,
                         nullptr);
}

void NativeControlFactory::PopulateCombo(HWND combo,
                                         std::initializer_list<const wchar_t*> items,
                                         int selectedIndex) const {
    if (!combo) return;
    for (const auto* item : items) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    }
    SendMessageW(combo, CB_SETCURSEL, selectedIndex, 0);
}
