#pragma once

#include <string>
#include <string_view>
#include <windows.h>

namespace urlutil {

std::wstring Utf8ToWide(std::string_view s);
std::string WideToUtf8(std::wstring_view s);

// Percent-encode UTF-8 bytes.
std::wstring UrlEncode(std::wstring_view s);

// Random [A-Za-z0-9]
std::wstring RandomAlnum(size_t len);

bool SetClipboardText(HWND hwndOwner, std::wstring_view text);

} // namespace urlutil
