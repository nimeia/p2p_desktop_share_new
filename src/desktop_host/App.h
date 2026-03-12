#pragma once

#include <memory>

// TODO: Add Windows App SDK / C++/WinRT includes if the desktop shell is migrated later
// #include <winrt/Microsoft.UI.Xaml.h>
// #include "MainWindow.h"

class App
{
public:
    App() = default;
    ~App() = default;

    void Run();

private:
    // TODO: Add main window reference
    // std::unique_ptr<MainWindow> m_main;
};
