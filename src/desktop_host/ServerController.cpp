#include "pch.h"
#include "ServerController.h"
#include "UrlUtil.h"

#include <algorithm>
#include <sstream>

static std::wstring GetLastErrMsg(DWORD err) {
    wchar_t buf[256];
    _snwprintf_s(buf, _TRUNCATE, L"0x%08X", err);
    return buf;
}

ServerController::~ServerController() {
    Stop();
}

void ServerController::SetLogCallback(ServerLogCallback cb) {
    m_logCb = std::move(cb);
}

bool ServerController::IsRunning() const noexcept {
    if (!m_running.load()) return false;
    if (!m_pi.hProcess) return false;
    DWORD code = 0;
    if (GetExitCodeProcess(m_pi.hProcess, &code)) {
        if (code == STILL_ACTIVE) return true;
    }
    return false;
}

std::wstring ServerController::QuoteArg(const std::wstring& s) {
    // Minimal quoting: wrap if contains space or quotes.
    bool need = false;
    for (wchar_t c : s) {
        if (c == L' ' || c == L'\t' || c == L'"') { need = true; break; }
    }
    if (!need) return s;
    std::wstring out = L"\"";
    for (wchar_t c : s) {
        if (c == L'"') out += L"\\\"";
        else out += c;
    }
    out += L"\"";
    return out;
}

ServerStartResult ServerController::Start(const ServerOptions& opt) {
    ServerStartResult res;
    if (IsRunning()) {
        res.ok = false;
        res.message = L"Server already running";
        return res;
    }

    Cleanup();

    std::filesystem::path exe = opt.executable;
    if (exe.empty()) exe = std::filesystem::current_path() / L"lan_screenshare_server.exe";

    std::filesystem::path www = opt.wwwDir;
    if (www.empty()) www = std::filesystem::current_path() / L"www";

    std::filesystem::path admin = opt.adminDir;
    if (admin.empty()) admin = std::filesystem::current_path() / L"webui";

    if (!std::filesystem::exists(exe)) {
        res.ok = false;
        res.message = L"server exe not found: " + exe.wstring();
        return res;
    }
    if (!std::filesystem::exists(www) || !std::filesystem::is_directory(www)) {
        res.ok = false;
        res.message = L"server www dir not found: " + www.wstring();
        return res;
    }
    if (!std::filesystem::exists(www / L"host.html") || !std::filesystem::exists(www / L"viewer.html")) {
        res.ok = false;
        res.message = L"server www dir is incomplete: " + www.wstring();
        return res;
    }
    if (!std::filesystem::exists(admin) || !std::filesystem::is_directory(admin)) {
        res.ok = false;
        res.message = L"server admin dir not found: " + admin.wstring();
        return res;
    }
    if (!std::filesystem::exists(admin / L"index.html") ||
        !std::filesystem::exists(admin / L"app.js") ||
        !std::filesystem::exists(admin / L"style.css")) {
        res.ok = false;
        res.message = L"server admin dir is incomplete: " + admin.wstring();
        return res;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&m_outRead, &m_outWrite, &sa, 0)) {
        res.ok = false;
        res.message = L"CreatePipe stdout failed: " + GetLastErrMsg(GetLastError());
        Cleanup();
        return res;
    }
    if (!SetHandleInformation(m_outRead, HANDLE_FLAG_INHERIT, 0)) {
        // non-fatal
    }

    if (!CreatePipe(&m_errRead, &m_errWrite, &sa, 0)) {
        res.ok = false;
        res.message = L"CreatePipe stderr failed: " + GetLastErrMsg(GetLastError());
        Cleanup();
        return res;
    }
    if (!SetHandleInformation(m_errRead, HANDLE_FLAG_INHERIT, 0)) {
        // non-fatal
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = m_outWrite;
    si.hStdError = m_errWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); // not used when --no-stdin

    std::wstringstream cmd;
    cmd << QuoteArg(exe.wstring())
        << L" --bind " << QuoteArg(opt.bind)
        << L" --port " << QuoteArg(opt.port)
        << L" --www " << QuoteArg(www.wstring())
        << L" --admin-www " << QuoteArg(admin.wstring())
        << L" --no-stdin";

    std::wstring cmdLine = cmd.str();
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    // Workdir = folder of exe (so relative paths are stable)
    std::wstring workdir = exe.parent_path().wstring();

    DWORD flags = CREATE_NO_WINDOW;

    BOOL ok = CreateProcessW(
        nullptr,
        mutableCmd.data(),
        nullptr,
        nullptr,
        TRUE,
        flags,
        nullptr,
        workdir.empty() ? nullptr : workdir.c_str(),
        &si,
        &m_pi);

    // Parent doesn't need write ends
    if (m_outWrite) { CloseHandle(m_outWrite); m_outWrite = nullptr; }
    if (m_errWrite) { CloseHandle(m_errWrite); m_errWrite = nullptr; }

    if (!ok) {
        res.ok = false;
        res.message = L"CreateProcess failed: " + GetLastErrMsg(GetLastError());
        Cleanup();
        return res;
    }

    m_running.store(true);
    m_stopReaders.store(false);

    // Start reader threads
    m_outThread = std::make_unique<std::thread>(&ServerController::ReaderThreadProc, this, m_outRead);
    m_errThread = std::make_unique<std::thread>(&ServerController::ReaderThreadProc, this, m_errRead);

    res.ok = true;
    res.message = L"Server started";
    if (m_logCb) {
        m_logCb(L"[spawn] " + cmdLine);
    }
    return res;
}

void ServerController::Stop() noexcept {
    if (!m_pi.hProcess) {
        Cleanup();
        return;
    }

    m_stopReaders.store(true);

    // Try terminate
    TerminateProcess(m_pi.hProcess, 0);
    WaitForSingleObject(m_pi.hProcess, 1500);

    Cleanup();
}

void ServerController::Cleanup() noexcept {
    m_running.store(false);

    if (m_outThread && m_outThread->joinable()) {
        m_outThread->join();
    }
    if (m_errThread && m_errThread->joinable()) {
        m_errThread->join();
    }
    m_outThread.reset();
    m_errThread.reset();

    if (m_outRead) { CloseHandle(m_outRead); m_outRead = nullptr; }
    if (m_outWrite) { CloseHandle(m_outWrite); m_outWrite = nullptr; }
    if (m_errRead) { CloseHandle(m_errRead); m_errRead = nullptr; }
    if (m_errWrite) { CloseHandle(m_errWrite); m_errWrite = nullptr; }

    if (m_pi.hThread) { CloseHandle(m_pi.hThread); m_pi.hThread = nullptr; }
    if (m_pi.hProcess) { CloseHandle(m_pi.hProcess); m_pi.hProcess = nullptr; }
}

void ServerController::ReaderThreadProc(HANDLE hRead) {
    if (!hRead) return;

    std::string buffer;
    buffer.reserve(4096);

    char tmp[512];
    DWORD read = 0;

    while (!m_stopReaders.load()) {
        DWORD avail = 0;
        if (!PeekNamedPipe(hRead, nullptr, 0, nullptr, &avail, nullptr)) {
            break;
        }
        if (avail == 0) {
            // Check process still active
            if (m_pi.hProcess) {
                DWORD code = 0;
                if (GetExitCodeProcess(m_pi.hProcess, &code) && code != STILL_ACTIVE) {
                    break;
                }
            }
            Sleep(50);
            continue;
        }

        if (!ReadFile(hRead, tmp, static_cast<DWORD>(std::min(sizeof(tmp), static_cast<size_t>(avail))), &read, nullptr) || read == 0) {
            break;
        }

        buffer.append(tmp, tmp + read);

        // Emit complete lines
        for (;;) {
            size_t pos = buffer.find('\n');
            if (pos == std::string::npos) break;
            std::string line = buffer.substr(0, pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            buffer.erase(0, pos + 1);

            if (m_logCb) {
                std::wstring w = urlutil::Utf8ToWide(line);
                m_logCb(w);
            }
        }
    }

    // Flush remainder
    if (!buffer.empty() && m_logCb) {
        m_logCb(urlutil::Utf8ToWide(buffer));
    }
}
