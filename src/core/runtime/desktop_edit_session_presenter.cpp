#include "core/runtime/desktop_edit_session_presenter.h"

#include "core/runtime/runtime_controller.h"

#include <cwctype>
#include <sstream>

namespace lan::runtime {
namespace {

bool IsAllWhitespace(std::wstring_view text) {
  for (wchar_t ch : text) {
    if (!iswspace(ch)) return false;
  }
  return true;
}

std::wstring TrimCopy(std::wstring_view text) {
  std::size_t start = 0;
  std::size_t end = text.size();
  while (start < end && iswspace(text[start])) ++start;
  while (end > start && iswspace(text[end - 1])) --end;
  return std::wstring(text.substr(start, end - start));
}

bool TryParsePort(std::wstring_view text, int* value) {
  const std::wstring trimmed = TrimCopy(text);
  if (trimmed.empty()) {
    if (value) *value = 0;
    return true;
  }

  long long parsed = 0;
  for (wchar_t ch : trimmed) {
    if (ch < L'0' || ch > L'9') return false;
    parsed = parsed * 10 + static_cast<long long>(ch - L'0');
    if (parsed > 65535) return false;
  }
  if (parsed <= 0) return false;
  if (value) *value = static_cast<int>(parsed);
  return true;
}

bool ConfigEquals(const HostSessionConfig& lhs, const HostSessionConfig& rhs) {
  return lhs.bindAddress == rhs.bindAddress && lhs.port == rhs.port && lhs.room == rhs.room && lhs.token == rhs.token;
}

HostSessionState BuildTemplateState(const HostSessionState& baseState, int templateIndex) {
  switch (static_cast<DesktopSessionTemplateKind>(templateIndex)) {
    case DesktopSessionTemplateKind::FixedRoom:
      return ApplyHostSessionConfig(baseState,
                                    L"meeting-room",
                                    L"persistent-token",
                                    baseState.config.bindAddress,
                                    baseState.config.port)
          .state;
    case DesktopSessionTemplateKind::DemoMode:
      return ApplyHostSessionConfig(baseState,
                                    L"demo",
                                    L"demo-view",
                                    baseState.config.bindAddress,
                                    baseState.config.port)
          .state;
    case DesktopSessionTemplateKind::QuickShare:
    default:
      return GenerateHostSessionCredentials(baseState).state;
  }
}

std::wstring BuildSummaryText(const HostSessionState& state,
                              std::wstring_view hostIp,
                              std::wstring_view outputDir,
                              std::wstring_view shareBundleDir) {
  RuntimeSessionState session;
  session.hostIp = std::wstring(hostIp);
  session.port = state.config.port;
  session.room = state.config.room;
  session.token = state.config.token;

  std::wstringstream ss;
  ss << L"Host URL\r\n" << BuildHostUrl(session) << L"\r\n\r\n";
  ss << L"Viewer URL\r\n" << BuildViewerUrl(session) << L"\r\n\r\n";
  ss << L"Output Dir\r\n" << outputDir << L"\r\n\r\n";
  ss << L"Share Bundle\r\n" << shareBundleDir;
  return ss.str();
}

} // namespace

DesktopEditSessionDraft BuildDesktopEditSessionDraft(const HostSessionState& state, int templateIndex) {
  const HostSessionState normalized = NormalizeHostSessionState(state);
  DesktopEditSessionDraft draft;
  draft.bindAddress = normalized.config.bindAddress;
  draft.portText = std::to_wstring(normalized.config.port);
  draft.room = normalized.config.room;
  draft.token = normalized.config.token;
  draft.templateIndex = templateIndex;
  return draft;
}

HostSessionMutationResult ApplyDesktopEditSessionDraft(const HostSessionState& appliedState,
                                                       const DesktopEditSessionDraft& draft,
                                                       bool resetDeliveryState,
                                                       bool* portValid) {
  HostSessionState next = NormalizeHostSessionState(appliedState);
  next.config.bindAddress = TrimCopy(draft.bindAddress);
  next.config.room = TrimCopy(draft.room);
  next.config.token = TrimCopy(draft.token);

  bool parsedPortValid = true;
  int parsedPort = 0;
  if (!TryParsePort(draft.portText, &parsedPort)) {
    parsedPort = 0;
    parsedPortValid = false;
  }
  next.config.port = parsedPort;
  next = NormalizeHostSessionState(next);

  if (portValid) *portValid = parsedPortValid;

  HostSessionMutationResult result;
  result.state = std::move(next);
  result.configChanged = !ConfigEquals(NormalizeHostSessionState(appliedState).config, result.state.config) || !parsedPortValid;
  result.generatedRoom = false;
  result.generatedToken = false;
  if (resetDeliveryState && result.configChanged) {
    result.state.flags.viewerUrlCopied = false;
    result.state.flags.shareBundleExported = false;
    result.state.flags.shareWizardOpened = false;
    result.state.flags.handoffStarted = false;
    result.state.flags.handoffDelivered = false;
    result.resetDeliveryStateApplied = true;
  }
  return result;
}

DesktopEditSessionDraft ApplyDesktopSessionTemplate(const HostSessionState& baseState,
                                                    int templateIndex,
                                                    int preserveTemplateIndex) {
  const HostSessionState normalized = NormalizeHostSessionState(baseState);
  const HostSessionState templated = BuildTemplateState(normalized, templateIndex);
  return BuildDesktopEditSessionDraft(templated,
                                      preserveTemplateIndex >= 0 ? preserveTemplateIndex : templateIndex);
}

DesktopEditSessionViewModel BuildDesktopEditSessionViewModel(const DesktopEditSessionInput& input) {
  DesktopEditSessionViewModel viewModel;
  bool portValid = true;
  auto mutation = ApplyDesktopEditSessionDraft(input.appliedState, input.draft, false, &portValid);

  viewModel.draftState = std::move(mutation.state);
  viewModel.portValid = portValid;
  viewModel.dirty = mutation.configChanged;
  viewModel.pendingApply = input.serverRunning && viewModel.dirty;

  if (!viewModel.portValid) {
    viewModel.statusText = L"Port is outside 1-65535. Restart and start actions will fall back to the default port when applied.";
  } else if (viewModel.pendingApply) {
    viewModel.statusText = L"Pending apply: the current form differs from the live session. Restart sharing to push the updated bind/port/room/token.";
  } else if (viewModel.dirty) {
    viewModel.statusText = L"Pending edits: these values will be applied on the next start.";
  } else if (input.serverRunning) {
    viewModel.statusText = L"Live session matches the current form.";
  } else {
    viewModel.statusText = L"Current form is ready for the next start.";
  }

  viewModel.sessionSummaryLabel = viewModel.dirty
      ? (viewModel.pendingApply ? L"Pending session summary after restart/apply:" : L"Pending session summary before next start:")
      : L"Live session summary before launch:";
  viewModel.sessionSummaryBody = BuildSummaryText(viewModel.draftState,
                                                  input.hostIp,
                                                  input.outputDir,
                                                  input.shareBundleDir);

  viewModel.startButtonLabel = viewModel.dirty ? L"Start (Apply Edits)" : L"Start";
  viewModel.restartButtonLabel = viewModel.pendingApply ? L"Restart To Apply" : L"Restart";
  viewModel.serviceOnlyButtonLabel = viewModel.dirty ? L"Service Only (Apply)" : L"Service Only";
  viewModel.startAndOpenHostButtonLabel = viewModel.pendingApply ? L"Restart + Open Host" :
                                          (viewModel.dirty ? L"Start + Open Host (Apply)" : L"Start + Open Host");

  viewModel.buttonPolicy.generateEnabled = true;
  viewModel.buttonPolicy.restartEnabled = input.serverRunning || viewModel.dirty;
  viewModel.buttonPolicy.serviceOnlyEnabled = !viewModel.pendingApply;
  viewModel.buttonPolicy.startAndOpenHostEnabled = !viewModel.pendingApply;
  return viewModel;
}

} // namespace lan::runtime
