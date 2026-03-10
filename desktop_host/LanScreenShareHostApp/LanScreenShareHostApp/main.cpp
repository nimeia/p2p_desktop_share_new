#include "pch.h"
#include "App.h"

#include <objbase.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    // WebView2 requires COM (STA is fine for UI thread)
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    App app;
    app.Run();

    CoUninitialize();
    return 0;
}
