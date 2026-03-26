#include "pch.h"
#include "App.h"
#include "MainWindow.h"

namespace {

constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\ViewMeshApp.Singleton";

HWND FindExistingHostWindow() {
    return FindWindowW(L"ViewMeshApp", nullptr);
}

bool WakeExistingInstance() {
    HWND hwnd = nullptr;
    for (int attempt = 0; attempt < 20 && !hwnd; ++attempt) {
        hwnd = FindExistingHostWindow();
        if (!hwnd) Sleep(100);
    }
    if (!hwnd) {
        return false;
    }

    DWORD existingPid = 0;
    GetWindowThreadProcessId(hwnd, &existingPid);
    if (existingPid != 0) {
        AllowSetForegroundWindow(existingPid);
    }

    PostMessageW(hwnd, MainWindow::kSingleInstanceWakeMessage, 0, 0);
    ShowWindowAsync(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(hwnd);
    return true;
}

} // namespace

App::~App() {
    if (m_singleInstanceMutex) {
        CloseHandle(m_singleInstanceMutex);
        m_singleInstanceMutex = nullptr;
    }
}

void App::Run()
{
    m_singleInstanceMutex = CreateMutexW(nullptr, FALSE, kSingleInstanceMutexName);
    if (!m_singleInstanceMutex) {
        std::cerr << "Failed to create single-instance mutex" << std::endl;
        return;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        WakeExistingInstance();
        return;
    }

    // Create main window
    MainWindow mainWindow;
    if (!mainWindow.Create())
    {
        std::cerr << "Failed to create main window" << std::endl;
        return;
    }

    mainWindow.Show();

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
