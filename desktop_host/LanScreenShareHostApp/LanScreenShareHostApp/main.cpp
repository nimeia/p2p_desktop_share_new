#include "pch.h"
#include "App.h"

#include <objbase.h>

namespace {

void EnableDpiAwareHosting() {
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE)) {
        SetProcessDPIAware();
    }
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    EnableDpiAwareHosting();

    // WebView2 requires COM (STA is fine for UI thread)
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    App app;
    app.Run();

    CoUninitialize();
    return 0;
}
