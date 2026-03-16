#include "core/runtime/remote_probe_orchestrator.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "remote probe orchestrator test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  std::vector<RemoteProbeCandidateInput> candidates;
  candidates.push_back({L"Ethernet", L"192.168.1.8", L"ethernet", true, false, true, L"/health answered"});
  candidates.push_back({L"Wi-Fi", L"10.0.0.8", L"wifi", false, true, false, L"timeout"});

  const auto alternatePlan = BuildRemoteProbePlan(candidates);
  Expect(alternatePlan.label.find(L"Recommended") != std::wstring::npos,
         "alternate plan should recommend the healthy adapter");
  Expect(alternatePlan.suggestedIp == L"192.168.1.8", "alternate plan should surface suggested ip");
  Expect(alternatePlan.action.find(L"192.168.1.8") != std::wstring::npos,
         "alternate plan action should mention suggested ip");
  Expect(alternatePlan.candidates.size() == 2, "candidate plan should preserve all candidates");
  Expect(alternatePlan.candidates.front().probeReady, "first candidate should be marked ready");

  std::vector<RemoteProbeCandidateInput> selectedHealthy;
  selectedHealthy.push_back({L"Wi-Fi", L"192.168.50.10", L"wifi", true, true, true, L"ok"});
  selectedHealthy.push_back({L"Ethernet", L"192.168.50.20", L"ethernet", false, false, true, L"ok"});
  const auto selectedPlan = BuildRemoteProbePlan(selectedHealthy);
  Expect(selectedPlan.selectedReady, "selected plan should mark selected ready");
  Expect(selectedPlan.label.find(L"confirmed") != std::wstring::npos ||
             selectedPlan.label.find(L"Confirmed") != std::wstring::npos,
         "selected plan should confirm selected adapter");

  std::vector<RemoteProbeCandidateInput> noneReady;
  noneReady.push_back({L"Wi-Fi", L"192.168.10.2", L"wifi", true, true, false, L"timeout"});
  const auto failedPlan = BuildRemoteProbePlan(noneReady);
  Expect(!failedPlan.selectedReady, "failed plan should not mark selected ready");
  Expect(failedPlan.label.find(L"No adapter") != std::wstring::npos,
         "failed plan should surface no adapter answered state");

  std::cout << "remote probe orchestrator tests passed\n";
  return 0;
}
