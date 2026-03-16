#pragma once

#include <initializer_list>
#include <string_view>
#include <windows.h>

class NativeControlFactory {
public:
    NativeControlFactory(HWND parent, HINSTANCE instance);

    HWND CreateButton(std::wstring_view text,
                      DWORD style,
                      int x,
                      int y,
                      int width,
                      int height,
                      int controlId = 0) const;

    HWND CreateStatic(std::wstring_view text,
                      DWORD style,
                      int x,
                      int y,
                      int width,
                      int height,
                      int controlId = 0) const;

    HWND CreateEdit(std::wstring_view text,
                    DWORD style,
                    int x,
                    int y,
                    int width,
                    int height,
                    int controlId = 0,
                    DWORD exStyle = WS_EX_CLIENTEDGE) const;

    HWND CreateCombo(DWORD style,
                     int x,
                     int y,
                     int width,
                     int height,
                     int controlId = 0) const;

    void PopulateCombo(HWND combo,
                       std::initializer_list<const wchar_t*> items,
                       int selectedIndex) const;

private:
    HWND parent_ = nullptr;
    HINSTANCE instance_ = nullptr;
};
