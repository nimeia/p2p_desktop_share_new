#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <filesystem>
#include <iostream>
#include <memory>
#include <chrono>
#include <iomanip>

// TODO: Add WinRT includes once C++/WinRT is properly configured
// #include <winrt/Windows.Foundation.h>
// #include <winrt/Windows.System.h>
// #include <winrt/Microsoft.UI.Xaml.h>
// etc.

