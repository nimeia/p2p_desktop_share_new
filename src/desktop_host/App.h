#pragma once

#include <windows.h>

class App
{
public:
    App() = default;
    ~App();

    void Run();

private:
    HANDLE m_singleInstanceMutex = nullptr;
};
