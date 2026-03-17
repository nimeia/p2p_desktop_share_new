#pragma once

#include <filesystem>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <windows.h>

struct ServerOptions {
    std::filesystem::path executable;  // lan_screenshare_server.exe
    std::filesystem::path wwwDir;      // www
    std::filesystem::path adminDir;    // webui
    std::wstring bind = L"0.0.0.0";
    std::wstring port = L"9443";
};

struct ServerStartResult {
    bool ok = false;
    std::wstring message;
};

using ServerLogCallback = std::function<void(const std::wstring&)>;

class ServerController {
public:
    ServerController() = default;
    ~ServerController();

    ServerController(const ServerController&) = delete;
    ServerController& operator=(const ServerController&) = delete;

    bool IsRunning() const noexcept;
    ServerStartResult Start(const ServerOptions& opt);
    void Stop() noexcept;

    void SetLogCallback(ServerLogCallback cb);

private:
    void Cleanup() noexcept;
    void ReaderThreadProc(HANDLE hRead);

    static std::wstring QuoteArg(const std::wstring& s);

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopReaders{false};

    PROCESS_INFORMATION m_pi{};

    HANDLE m_outRead = nullptr;
    HANDLE m_outWrite = nullptr;
    HANDLE m_errRead = nullptr;
    HANDLE m_errWrite = nullptr;

    std::unique_ptr<std::thread> m_outThread;
    std::unique_ptr<std::thread> m_errThread;

    ServerLogCallback m_logCb;
};
