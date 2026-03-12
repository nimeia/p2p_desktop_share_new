#include "pch.h"
#include "App.h"
#include "MainWindow.h"

void App::Run()
{
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
