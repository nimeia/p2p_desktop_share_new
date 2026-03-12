#include "pch.h"
#include "UrlUtil.h"

#include <bcrypt.h>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace urlutil {

std::wstring Utf8ToWide(std::string_view s) {
    if (s.empty()) return L"";
    int need = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (need <= 0) return L"";
    std::wstring out(need, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), need);
    return out;
}

std::string WideToUtf8(std::wstring_view s) {
    if (s.empty()) return {};
    int need = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (need <= 0) return {};
    std::string out(need, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), need, nullptr, nullptr);
    return out;
}

static bool IsUnreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

std::wstring UrlEncode(std::wstring_view s) {
    // Encode as UTF-8 then percent-encode bytes.
    std::string utf8 = WideToUtf8(s);
    std::wstring out;
    out.reserve(utf8.size() * 3);
    const wchar_t* hex = L"0123456789ABCDEF";
    for (unsigned char c : utf8) {
        if (IsUnreserved(c)) {
            out.push_back((wchar_t)c);
        } else {
            out.push_back(L'%');
            out.push_back(hex[(c >> 4) & 0xF]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

std::wstring RandomAlnum(size_t len) {
    static const wchar_t kChars[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::vector<unsigned char> buf(len);
    if (len > 0) {
        if (BCryptGenRandom(nullptr, buf.data(), (ULONG)buf.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
            // fallback
            for (size_t i = 0; i < len; ++i) buf[i] = (unsigned char)(GetTickCount64() + i);
        }
    }
    std::wstring out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(kChars[buf[i] % (sizeof(kChars) / sizeof(kChars[0]) - 1)]);
    }
    return out;
}

bool SetClipboardText(HWND hwndOwner, std::wstring_view text) {
    if (!OpenClipboard(hwndOwner)) return false;
    EmptyClipboard();

    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!h) {
        CloseClipboard();
        return false;
    }

    void* p = GlobalLock(h);
    if (!p) {
        GlobalFree(h);
        CloseClipboard();
        return false;
    }

    memcpy(p, text.data(), text.size() * sizeof(wchar_t));
    ((wchar_t*)p)[text.size()] = L'\0';
    GlobalUnlock(h);

    SetClipboardData(CF_UNICODETEXT, h);
    CloseClipboard();
    // Do not free h; clipboard owns it now.
    return true;
}

} // namespace urlutil
