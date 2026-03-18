#include "pch.h"
#include "ServerController.h"
#include "UrlUtil.h"

#include <algorithm>
#include <iphlpapi.h>
#include <sstream>

#pragma comment(lib, "iphlpapi.lib")

static std::wstring GetLastErrMsg(DWORD err) {
    wchar_t buf[256];
    _snwprintf_s(buf, _TRUNCATE, L"0x%08X", err);
    return buf;
}

namespace {

std::wstring NormalizeComparablePath(const std::filesystem::path& path) {
    if (path.empty()) return L"";

    std::error_code ec;
    auto normalized = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        normalized = path.lexically_normal();
    }

    std::wstring value = normalized.wstring();
    constexpr std::wstring_view kLongPathPrefix = L"\\\\?\\";
    if (value.rfind(kLongPathPrefix, 0) == 0) {
        value.erase(0, kLongPathPrefix.size());
    }
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(::towlower(ch));
    });
    return value;
}

std::wstring QueryProcessImagePath(DWORD pid) {
    if (pid == 0) return L"";

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return L"";

    std::wstring path;
    DWORD size = 32768;
    path.resize(size);
    if (QueryFullProcessImageNameW(process, 0, path.data(), &size) && size > 0) {
        path.resize(size);
    } else {
        path.clear();
    }

    CloseHandle(process);
    return path;
}

std::vector<DWORD> FindListeningProcessIdsForPort(int port) {
    std::vector<DWORD> pids;
    if (port <= 0 || port > 65535) {
        return pids;
    }

    ULONG size = 0;
    DWORD status = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
    if (status != ERROR_INSUFFICIENT_BUFFER || size == 0) {
        return pids;
    }

    std::vector<unsigned char> buffer(size);
    auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
    status = GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
    if (status != NO_ERROR || !table) {
        return pids;
    }

    const auto wantedPort = htons(static_cast<u_short>(port));
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];
        if (static_cast<u_short>(row.dwLocalPort) != wantedPort) continue;
        if (row.dwOwningPid == 0) continue;
        pids.push_back(row.dwOwningPid);
    }

    std::sort(pids.begin(), pids.end());
    pids.erase(std::unique(pids.begin(), pids.end()), pids.end());
    return pids;
}

bool TerminateProcessById(DWORD pid, DWORD timeoutMs, std::wstring* detail) {
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        if (detail) {
            *detail = L"OpenProcess failed for PID " + std::to_wstring(pid) + L": " + GetLastErrMsg(GetLastError());
        }
        return false;
    }

    bool ok = false;
    if (TerminateProcess(process, 0)) {
        const DWORD wait = WaitForSingleObject(process, timeoutMs);
        ok = (wait == WAIT_OBJECT_0);
        if (!ok && detail) {
            *detail = L"TerminateProcess timed out for PID " + std::to_wstring(pid) + L".";
        }
    } else if (detail) {
        *detail = L"TerminateProcess failed for PID " + std::to_wstring(pid) + L": " + GetLastErrMsg(GetLastError());
    }

    CloseHandle(process);
    return ok;
}

} // namespace

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

ServerCleanupResult ServerController::CleanupStaleProcess(const ServerOptions& opt, int port) noexcept {
    ServerCleanupResult result;

    std::filesystem::path exe = opt.executable;
    if (exe.empty()) exe = std::filesystem::current_path() / L"lan_screenshare_server.exe";
    if (!std::filesystem::exists(exe)) {
        result.message = L"Cannot recover stale server because the executable is missing: " + exe.wstring();
        return result;
    }

    const auto expectedPath = NormalizeComparablePath(exe);
    const auto pids = FindListeningProcessIdsForPort(port);
    if (pids.empty()) {
        return result;
    }

    std::vector<DWORD> matchingPids;
    std::vector<std::wstring> otherOwners;
    for (DWORD pid : pids) {
        if (pid == 0 || pid == GetCurrentProcessId()) continue;

        const auto imagePath = QueryProcessImagePath(pid);
        if (imagePath.empty()) {
            otherOwners.push_back(L"PID " + std::to_wstring(pid) + L" (image path unavailable)");
            continue;
        }

        if (NormalizeComparablePath(imagePath) == expectedPath) {
            matchingPids.push_back(pid);
        } else {
            otherOwners.push_back(L"PID " + std::to_wstring(pid) + L" (" + imagePath + L")");
        }
    }

    if (matchingPids.empty()) {
        if (!otherOwners.empty()) {
            std::wstringstream ss;
            ss << L"Port " << port << L" is occupied by another process: ";
            for (std::size_t i = 0; i < otherOwners.size(); ++i) {
                if (i != 0) ss << L"; ";
                ss << otherOwners[i];
            }
            result.message = ss.str();
        }
        return result;
    }

    result.matched = true;

    std::vector<DWORD> stoppedPids;
    std::vector<std::wstring> failures;
    for (DWORD pid : matchingPids) {
        std::wstring failure;
        if (TerminateProcessById(pid, 2000, &failure)) {
            stoppedPids.push_back(pid);
        } else {
            failures.push_back(failure.empty()
                                   ? (L"Failed to terminate PID " + std::to_wstring(pid))
                                   : std::move(failure));
        }
    }

    result.stopped = !stoppedPids.empty() && failures.empty();

    std::wstringstream ss;
    if (!stoppedPids.empty()) {
        ss << L"Recovered stale lan_screenshare_server instance";
        if (stoppedPids.size() > 1) ss << L"s";
        ss << L": ";
        for (std::size_t i = 0; i < stoppedPids.size(); ++i) {
            if (i != 0) ss << L", ";
            ss << L"PID " << stoppedPids[i];
        }
        ss << L".";
    }
    if (!failures.empty()) {
        if (ss.tellp() > 0) ss << L" ";
        ss << L"Failed to recover stale server: ";
        for (std::size_t i = 0; i < failures.size(); ++i) {
            if (i != 0) ss << L"; ";
            ss << failures[i];
        }
    }
    result.message = ss.str();
    return result;
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
