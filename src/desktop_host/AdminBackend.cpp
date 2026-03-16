#include "pch.h"
#include "AdminBackend.h"

#include <utility>

void AdminBackend::SetHandlers(Handlers handlers) {
    m_handlers = std::move(handlers);
}

AdminBackend::HandleResult AdminBackend::HandleMessage(std::wstring_view payload) const {
    return lan::runtime::HandleAdminShellMessage(
        lan::runtime::ParseShellBridgeInboundMessage(payload),
        m_handlers);
}

