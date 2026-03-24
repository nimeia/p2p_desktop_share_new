cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
project(lan_screenshare LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8)
endif()

find_package(boost_system CONFIG REQUIRED)
find_package(boost_thread CONFIG REQUIRED)
find_package(boost_json CONFIG REQUIRED)
find_package(boost_asio CONFIG REQUIRED)
find_package(boost_beast CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(lan_screenshare_network_shared
  src/core/network/endpoint_selection.cpp
)
target_include_directories(lan_screenshare_network_shared PUBLIC src)

add_library(lan_screenshare_shared
  src/core/runtime/runtime_controller.cpp
  src/core/runtime/desktop_shell_presenter.cpp
  src/core/runtime/desktop_layout_presenter.cpp
  src/core/runtime/desktop_edit_session_presenter.cpp
  src/core/runtime/shell_chrome_presenter.cpp
  src/core/runtime/shell_bridge_presenter.cpp
  src/core/runtime/native_shell_alert_coordinator.cpp
  src/core/runtime/native_shell_status_tracker.cpp
  src/core/runtime/admin_shell_coordinator.cpp
  src/core/runtime/admin_shell_runtime_publisher.cpp
  src/core/runtime/host_observability_coordinator.cpp
  src/core/runtime/host_runtime_scheduler.cpp
  src/core/runtime/host_shell_lifecycle_coordinator.cpp
  src/core/runtime/desktop_runtime_snapshot.cpp
  src/core/runtime/share_artifact_service.cpp
  src/core/runtime/host_runtime_coordinator.cpp
  src/core/runtime/host_action_coordinator.cpp
  src/core/runtime/host_session_coordinator.cpp
  src/core/runtime/admin_view_model_assembler.cpp
  src/core/runtime/diagnostics_view_model_assembler.cpp
  src/core/runtime/network_diagnostics_policy.cpp
  src/core/runtime/remote_probe_orchestrator.cpp
  src/core/server/service_host.cpp
  src/core/server/listener.cpp
  src/core/server/http_session.cpp
  src/core/server/http_router.cpp
  src/core/server/ws_session.cpp
  src/core/server/ws_session_handlers.cpp
  src/core/server/ws_hub.cpp
  src/core/protocol/messages.cpp
  src/core/util/json_util.cpp
  src/core/util/fs_util.cpp
  src/core/util/log.cpp
)

target_include_directories(lan_screenshare_shared PUBLIC src)

target_link_libraries(lan_screenshare_shared PUBLIC
  lan_screenshare_network_shared
  Boost::system
  Boost::thread
  Boost::json
  Boost::asio
  Boost::beast
  Threads::Threads
)

if (MSVC)
  target_compile_options(lan_screenshare_shared PRIVATE /bigobj)
  target_compile_definitions(lan_screenshare_shared PRIVATE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
endif()

if (WIN32)
  target_link_libraries(lan_screenshare_shared PUBLIC ws2_32 bcrypt crypt32 iphlpapi)
endif()

add_library(lan_screenshare_core ALIAS lan_screenshare_shared)

add_library(lan_screenshare_platform_abstraction
  src/platform/abstraction/runtime_paths.cpp
  src/platform/factory.cpp
)
target_include_directories(lan_screenshare_platform_abstraction PUBLIC src)
target_link_libraries(lan_screenshare_platform_abstraction PUBLIC lan_screenshare_shared)

if (WIN32)
  add_library(lan_screenshare_platform_default
    src/core/network/network_manager.cpp
    src/platform/windows/network_probe_win.cpp
    src/platform/windows/firewall_diagnostics_win.cpp
    src/platform/platform_service_facade.cpp
    src/platform/host_runtime_refresh_pipeline.cpp
    src/platform/windows/network_actions_win.cpp
    src/platform/windows/network_service_win.cpp
    src/platform/windows/system_actions_win.cpp
  )
elseif(APPLE)
  enable_language(OBJCXX)
  add_library(lan_screenshare_platform_default
    src/core/network/network_manager.cpp
    src/platform/posix/network_probe_posix.cpp
    src/platform/platform_service_facade.cpp
    src/platform/host_runtime_refresh_pipeline.cpp
    src/platform/posix/network_actions_posix.cpp
    src/platform/posix/network_service_posix.cpp
    src/platform/macos/system_actions_macos.mm
  )
else()
  add_library(lan_screenshare_platform_default
    src/core/network/network_manager.cpp
    src/platform/posix/network_probe_posix.cpp
    src/platform/platform_service_facade.cpp
    src/platform/host_runtime_refresh_pipeline.cpp
    src/platform/posix/network_actions_posix.cpp
    src/platform/posix/network_service_posix.cpp
    src/platform/posix/system_actions_posix.cpp
  )
endif()

target_include_directories(lan_screenshare_platform_default PUBLIC src)
target_link_libraries(lan_screenshare_platform_default PUBLIC lan_screenshare_platform_abstraction)
if (APPLE)
  target_link_libraries(lan_screenshare_platform_default PUBLIC "-framework Cocoa")
endif()

add_executable(lan_screenshare_server
  apps/server_cli/main.cpp
)
if (WIN32)
  target_sources(lan_screenshare_server PRIVATE
    apps/server_cli/resource.rc
  )
endif()
target_link_libraries(lan_screenshare_server PRIVATE
  lan_screenshare_shared
  lan_screenshare_platform_abstraction
  lan_screenshare_platform_default
)

add_library(lan_screenshare_native_shell
  src/core/runtime/runtime_controller.cpp
  src/core/runtime/shell_chrome_presenter.cpp
  src/core/runtime/native_shell_alert_coordinator.cpp
  src/core/runtime/native_shell_status_tracker.cpp
  src/platform/platform_service_facade.cpp
  src/host_shell/native_shell_live_poller.cpp
  src/host_shell/native_shell_runtime_loop.cpp
  src/host_shell/native_shell_action_controller.cpp
)
target_include_directories(lan_screenshare_native_shell PUBLIC src)
target_link_libraries(lan_screenshare_native_shell PUBLIC
  Threads::Threads
)
if (UNIX AND NOT APPLE)
  target_link_libraries(lan_screenshare_native_shell PUBLIC dl)
endif()
if (WIN32)
  target_sources(lan_screenshare_native_shell PRIVATE src/platform/windows/system_actions_win.cpp)
elseif(APPLE)
  enable_language(OBJCXX)
  target_sources(lan_screenshare_native_shell PRIVATE src/platform/macos/system_actions_macos.mm)
  target_link_libraries(lan_screenshare_native_shell PUBLIC "-framework Cocoa")
else()
  target_sources(lan_screenshare_native_shell PRIVATE src/platform/posix/system_actions_posix.cpp)
endif()

add_executable(lan_screenshare_linux_tray
  apps/linux_tray/main.cpp
)
target_link_libraries(lan_screenshare_linux_tray PRIVATE lan_screenshare_native_shell)

if (APPLE)
  enable_language(OBJCXX)
  add_executable(lan_screenshare_macos_menubar
    apps/macos_menubar/main.mm
  )
  target_link_libraries(lan_screenshare_macos_menubar PRIVATE lan_screenshare_native_shell "-framework Cocoa")
  
  # Set macOS bundle properties
  set_target_properties(lan_screenshare_macos_menubar PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/apps/macos_menubar/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "LAN Screen Share"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_ICON_FILE "AppIcon"
  )
  
  # Copy icon file to app bundle
  add_custom_command(TARGET lan_screenshare_macos_menubar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/icons/macos/AppIcon.icns"
    "$<TARGET_BUNDLE_CONTENT_DIR:lan_screenshare_macos_menubar>/Resources/AppIcon.icns"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/icons/macos/statusbar"
    "$<TARGET_BUNDLE_CONTENT_DIR:lan_screenshare_macos_menubar>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/icons/macos/statusbar_states"
    "$<TARGET_BUNDLE_CONTENT_DIR:lan_screenshare_macos_menubar>/Resources"
    COMMENT "Copying macOS app icon"
  )
endif()

include(CTest)
if (BUILD_TESTING)
  add_executable(lan_screenshare_network_tests
    tests/shared/network_tests.cpp
  )
  target_link_libraries(lan_screenshare_network_tests PRIVATE lan_screenshare_network_shared)
  add_test(NAME lan_screenshare_network_tests COMMAND lan_screenshare_network_tests)

  add_executable(lan_screenshare_runtime_tests
    tests/shared/runtime_controller_tests.cpp
    tests/shared/desktop_runtime_snapshot_tests.cpp
  )
  target_link_libraries(lan_screenshare_runtime_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_runtime_tests COMMAND lan_screenshare_runtime_tests)

  add_executable(lan_screenshare_desktop_shell_presenter_tests
    tests/shared/desktop_shell_presenter_tests.cpp
  )
  target_link_libraries(lan_screenshare_desktop_shell_presenter_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_desktop_shell_presenter_tests COMMAND lan_screenshare_desktop_shell_presenter_tests)

  add_executable(lan_screenshare_desktop_edit_session_presenter_tests
    tests/shared/desktop_edit_session_presenter_tests.cpp
  )
  target_link_libraries(lan_screenshare_desktop_edit_session_presenter_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_desktop_edit_session_presenter_tests COMMAND lan_screenshare_desktop_edit_session_presenter_tests)

  add_executable(lan_screenshare_desktop_layout_presenter_tests
    tests/shared/desktop_layout_presenter_tests.cpp
  )
  target_link_libraries(lan_screenshare_desktop_layout_presenter_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_desktop_layout_presenter_tests COMMAND lan_screenshare_desktop_layout_presenter_tests)

  add_executable(lan_screenshare_shell_chrome_presenter_tests
    tests/shared/shell_chrome_presenter_tests.cpp
  )
  target_link_libraries(lan_screenshare_shell_chrome_presenter_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_shell_chrome_presenter_tests COMMAND lan_screenshare_shell_chrome_presenter_tests)

  add_executable(lan_screenshare_shell_bridge_presenter_tests
    tests/shared/shell_bridge_presenter_tests.cpp
  )
  target_link_libraries(lan_screenshare_shell_bridge_presenter_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_shell_bridge_presenter_tests COMMAND lan_screenshare_shell_bridge_presenter_tests)

  add_executable(lan_screenshare_admin_shell_coordinator_tests
    tests/shared/admin_shell_coordinator_tests.cpp
  )
  target_link_libraries(lan_screenshare_admin_shell_coordinator_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_admin_shell_coordinator_tests COMMAND lan_screenshare_admin_shell_coordinator_tests)

  add_executable(lan_screenshare_admin_shell_runtime_publisher_tests
    tests/shared/admin_shell_runtime_publisher_tests.cpp
  )
  target_link_libraries(lan_screenshare_admin_shell_runtime_publisher_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_admin_shell_runtime_publisher_tests COMMAND lan_screenshare_admin_shell_runtime_publisher_tests)

  add_executable(lan_screenshare_host_observability_coordinator_tests
    tests/shared/host_observability_coordinator_tests.cpp
  )
  target_link_libraries(lan_screenshare_host_observability_coordinator_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_host_observability_coordinator_tests COMMAND lan_screenshare_host_observability_coordinator_tests)

  add_executable(lan_screenshare_host_runtime_scheduler_tests
    tests/shared/host_runtime_scheduler_tests.cpp
  )
  target_link_libraries(lan_screenshare_host_runtime_scheduler_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_host_runtime_scheduler_tests COMMAND lan_screenshare_host_runtime_scheduler_tests)

  add_executable(lan_screenshare_host_shell_lifecycle_coordinator_tests
    tests/shared/host_shell_lifecycle_coordinator_tests.cpp
  )
  target_link_libraries(lan_screenshare_host_shell_lifecycle_coordinator_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_host_shell_lifecycle_coordinator_tests COMMAND lan_screenshare_host_shell_lifecycle_coordinator_tests)

  add_executable(lan_screenshare_host_runtime_coordinator_tests
    tests/shared/host_runtime_coordinator_tests.cpp
  )
  target_link_libraries(lan_screenshare_host_runtime_coordinator_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_host_runtime_coordinator_tests COMMAND lan_screenshare_host_runtime_coordinator_tests)

  add_executable(lan_screenshare_host_action_coordinator_tests
    tests/shared/host_action_coordinator_tests.cpp
  )
  target_link_libraries(lan_screenshare_host_action_coordinator_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_host_action_coordinator_tests COMMAND lan_screenshare_host_action_coordinator_tests)

  add_executable(lan_screenshare_host_session_coordinator_tests
    tests/shared/host_session_coordinator_tests.cpp
  )
  target_link_libraries(lan_screenshare_host_session_coordinator_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_host_session_coordinator_tests COMMAND lan_screenshare_host_session_coordinator_tests)

  add_executable(lan_screenshare_admin_view_model_assembler_tests
    tests/shared/admin_view_model_assembler_tests.cpp
  )
  target_link_libraries(lan_screenshare_admin_view_model_assembler_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_admin_view_model_assembler_tests COMMAND lan_screenshare_admin_view_model_assembler_tests)

  add_executable(lan_screenshare_diagnostics_view_model_assembler_tests
    tests/shared/diagnostics_view_model_assembler_tests.cpp
  )
  target_link_libraries(lan_screenshare_diagnostics_view_model_assembler_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_diagnostics_view_model_assembler_tests COMMAND lan_screenshare_diagnostics_view_model_assembler_tests)

  add_executable(lan_screenshare_network_diagnostics_policy_tests
    tests/shared/network_diagnostics_policy_tests.cpp
  )
  target_link_libraries(lan_screenshare_network_diagnostics_policy_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_network_diagnostics_policy_tests COMMAND lan_screenshare_network_diagnostics_policy_tests)

  add_executable(lan_screenshare_remote_probe_orchestrator_tests
    tests/shared/remote_probe_orchestrator_tests.cpp
  )
  target_link_libraries(lan_screenshare_remote_probe_orchestrator_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_remote_probe_orchestrator_tests COMMAND lan_screenshare_remote_probe_orchestrator_tests)

  add_executable(lan_screenshare_browser_smoke_tests
    tests/shared/browser_smoke_tests.cpp
  )
  target_link_libraries(lan_screenshare_browser_smoke_tests PRIVATE lan_screenshare_shared)
  target_compile_definitions(lan_screenshare_browser_smoke_tests PRIVATE LAN_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME lan_screenshare_browser_smoke_tests COMMAND lan_screenshare_browser_smoke_tests)

  add_executable(lan_screenshare_share_artifact_tests
    tests/shared/share_artifact_service_tests.cpp
  )
  target_link_libraries(lan_screenshare_share_artifact_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_share_artifact_tests COMMAND lan_screenshare_share_artifact_tests)

  add_executable(lan_screenshare_platform_facade_tests
    tests/shared/platform_service_facade_tests.cpp
  )
  target_link_libraries(lan_screenshare_platform_facade_tests PRIVATE
    lan_screenshare_platform_abstraction
    lan_screenshare_platform_default
  )
  add_test(NAME lan_screenshare_platform_facade_tests COMMAND lan_screenshare_platform_facade_tests)

  add_executable(lan_screenshare_native_shell_alert_coordinator_tests
    tests/shared/native_shell_alert_coordinator_tests.cpp
  )
  target_link_libraries(lan_screenshare_native_shell_alert_coordinator_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_native_shell_alert_coordinator_tests COMMAND lan_screenshare_native_shell_alert_coordinator_tests)

  add_executable(lan_screenshare_native_shell_status_tracker_tests
    tests/shared/native_shell_status_tracker_tests.cpp
  )
  target_link_libraries(lan_screenshare_native_shell_status_tracker_tests PRIVATE lan_screenshare_shared)
  add_test(NAME lan_screenshare_native_shell_status_tracker_tests COMMAND lan_screenshare_native_shell_status_tracker_tests)

  add_executable(lan_screenshare_native_shell_action_controller_tests
    tests/shared/native_shell_action_controller_tests.cpp
  )
  target_link_libraries(lan_screenshare_native_shell_action_controller_tests PRIVATE lan_screenshare_native_shell)
  add_test(NAME lan_screenshare_native_shell_action_controller_tests COMMAND lan_screenshare_native_shell_action_controller_tests)

  add_executable(lan_screenshare_native_shell_runtime_loop_tests
    tests/shared/native_shell_runtime_loop_tests.cpp
  )
  target_link_libraries(lan_screenshare_native_shell_runtime_loop_tests PRIVATE lan_screenshare_native_shell)
  add_test(NAME lan_screenshare_native_shell_runtime_loop_tests COMMAND lan_screenshare_native_shell_runtime_loop_tests)
endif()

if (UNIX AND NOT APPLE)
  file(GLOB ICON_SIZES "src/resources/icons/linux/hicolor/*/apps/*.png")
  foreach(ICON ${ICON_SIZES})
    string(REGEX MATCH "icons/linux/hicolor/([^/]+)/apps" MATCH_DIR "${ICON}")
    set(ICON_SIZE "${CMAKE_MATCH_1}")
    install(FILES "${ICON}" DESTINATION "share/icons/hicolor/${ICON_SIZE}/apps")
  endforeach()

  file(GLOB TRAY_ICONS "src/resources/icons/linux/tray/*.png")
  install(FILES ${TRAY_ICONS} DESTINATION "share/icons/hicolor/scalable/apps")

  install(DIRECTORY
    "src/resources/icons/linux/tray"
    "src/resources/icons/linux/tray_states"
    DESTINATION "share/lan_screenshare/icons/linux"
  )

  install(FILES "src/resources/icons/linux/desktop/lanscreenshare.desktop" DESTINATION "share/applications")
endif()
