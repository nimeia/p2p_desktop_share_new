#pragma once

#include <string>
#include <windows.h>

struct HttpResponse {
    DWORD status = 0;
    std::string body;
    std::wstring error;
};

class HttpClient {
public:
    // Synchronous GET. Call from worker thread.
    static HttpResponse Get(const std::wstring& url, DWORD timeoutMs = 1500);
};
