#include "pch.h"
#include "HttpClient.h"

#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

static std::wstring LastErrorText(DWORD err) {
    wchar_t buf[256];
    _snwprintf_s(buf, _TRUNCATE, L"0x%08X", err);
    return buf;
}

HttpResponse HttpClient::Get(const std::wstring& url, DWORD timeoutMs) {
    HttpResponse out;

    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256];
    wchar_t path[2048];
    uc.lpszHostName = host;
    uc.dwHostNameLength = (DWORD)_countof(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = (DWORD)_countof(path);
    uc.dwSchemeLength = (DWORD)-1;

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) {
        out.error = L"WinHttpCrackUrl failed: " + LastErrorText(GetLastError());
        return out;
    }

    hSession = WinHttpOpen(L"ViewMesh/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        out.error = L"WinHttpOpen failed: " + LastErrorText(GetLastError());
        return out;
    }

    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
    if (!hConnect) {
        out.error = L"WinHttpConnect failed: " + LastErrorText(GetLastError());
        WinHttpCloseHandle(hSession);
        return out;
    }

    DWORD flags = WINHTTP_FLAG_REFRESH;
    if (uc.nScheme == INTERNET_SCHEME_HTTPS) flags |= WINHTTP_FLAG_SECURE;

    hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        out.error = L"WinHttpOpenRequest failed: " + LastErrorText(GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return out;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        out.error = L"WinHttpSendRequest failed: " + LastErrorText(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return out;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        out.error = L"WinHttpReceiveResponse failed: " + LastErrorText(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return out;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    out.status = status;

    std::string body;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
        if (!avail) break;
        size_t old = body.size();
        body.resize(old + avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, body.data() + old, avail, &read)) break;
        body.resize(old + read);
        if (read == 0) break;
    }
    out.body = std::move(body);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return out;
}
