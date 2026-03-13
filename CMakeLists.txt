cmake_minimum_required(VERSION 3.24)

# Suppress dev warning about removed FindBoost module in newer CMake
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
project(lan_screenshare LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg toolchain should be configured outside (e.g., -DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake)

find_package(boost_system CONFIG REQUIRED)
find_package(boost_thread CONFIG REQUIRED)
find_package(boost_json CONFIG REQUIRED)
find_package(boost_asio CONFIG REQUIRED)
find_package(boost_beast CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_library(lan_screenshare_core
  src/core/server/service_host.cpp
  src/core/server/listener.cpp
  src/core/server/http_session.cpp
  src/core/server/http_router.cpp
  src/core/server/ws_session.cpp
  src/core/server/ws_session_handlers.cpp
  src/core/server/ws_hub.cpp
  src/core/server/tls_context.cpp
  src/core/cert/cert_manager.cpp
  src/core/protocol/messages.cpp
  src/core/util/json_util.cpp
  src/core/util/fs_util.cpp
  src/core/util/log.cpp
  src/core/network/network_manager.cpp
)

target_include_directories(lan_screenshare_core PUBLIC src)
if (WIN32)
  target_compile_definitions(lan_screenshare_core PRIVATE LAN_CERT_USE_OPENSSL_API=1)
endif()

target_link_libraries(lan_screenshare_core PUBLIC
  Boost::system
  Boost::thread
  Boost::json
  Boost::asio
  Boost::beast
  OpenSSL::SSL
  OpenSSL::Crypto
  Threads::Threads
)

# MSVC: Boost.Beast/Asio heavy templates may exceed the default COFF section limit.
# /bigobj raises the limit and avoids C1128 on large translation units.
if (MSVC)
  target_compile_options(lan_screenshare_core PRIVATE /bigobj)
  # Ensure Boost.Asio targets modern Windows APIs; suppress "Assuming Windows 7 target" warnings.
  target_compile_definitions(lan_screenshare_core PRIVATE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
endif()

if (WIN32)
  target_link_libraries(lan_screenshare_core PUBLIC ws2_32 bcrypt crypt32 iphlpapi)
endif()

# Desktop host project would link against lan_screenshare_core


# --- WIP: standalone server executable (for desktop host app to spawn)
add_executable(lan_screenshare_server
  src/tools/lan_screenshare_server_main.cpp
)
target_link_libraries(lan_screenshare_server PRIVATE lan_screenshare_core)
