#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/runtime/admin_shell_coordinator.h"
#include "core/runtime/shell_bridge_presenter.h"

class AdminBackend {
public:
    using Handlers = lan::runtime::AdminShellCoordinatorHooks;

    using HandleResult = lan::runtime::AdminShellCoordinatorResult;

    void SetHandlers(Handlers handlers);
    HandleResult HandleMessage(std::wstring_view payload) const;

private:
    Handlers m_handlers;
};
