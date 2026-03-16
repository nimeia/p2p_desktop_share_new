#include "core/runtime/remote_probe_orchestrator.h"

#include <algorithm>

namespace lan::runtime {
namespace {

const RemoteProbeCandidateInput* FindFirstReady(const std::vector<RemoteProbeCandidateInput>& candidates,
                                                bool requireRecommended,
                                                bool requireAlternate) {
  for (const auto& candidate : candidates) {
    if (!candidate.probeReady) continue;
    if (requireRecommended && !candidate.recommended) continue;
    if (requireAlternate && candidate.selected) continue;
    return &candidate;
  }
  return nullptr;
}

const RemoteProbeCandidateInput* FindSelected(const std::vector<RemoteProbeCandidateInput>& candidates) {
  const auto it = std::find_if(candidates.begin(), candidates.end(), [](const auto& candidate) {
    return candidate.selected;
  });
  return it == candidates.end() ? nullptr : &(*it);
}

std::wstring DefaultProbeDetail(const RemoteProbeCandidateInput& candidate) {
  if (!candidate.probeDetail.empty()) return candidate.probeDetail;
  return candidate.probeReady
      ? L"The LAN /health probe succeeded for this address."
      : L"The LAN /health probe did not return a usable response for this address.";
}

} // namespace

RemoteProbePlan BuildRemoteProbePlan(const std::vector<RemoteProbeCandidateInput>& candidates) {
  RemoteProbePlan plan;
  plan.candidates.reserve(candidates.size());

  int readyCount = 0;
  for (const auto& candidate : candidates) {
    RemoteProbeCandidateViewModel view;
    view.name = candidate.name;
    view.ip = candidate.ip;
    view.type = candidate.type;
    view.recommended = candidate.recommended;
    view.selected = candidate.selected;
    view.probeReady = candidate.probeReady;
    view.probeLabel = candidate.probeReady ? L"LAN /health ok" : L"LAN /health failed";
    view.probeDetail = DefaultProbeDetail(candidate);
    plan.candidates.push_back(std::move(view));
    if (candidate.probeReady) ++readyCount;
  }

  if (candidates.empty()) {
    plan.label = L"No LAN candidates";
    plan.detail = L"No active IPv4 adapter candidate is available yet, so there is nothing to probe from the current host.";
    plan.action = L"Refresh adapter detection or connect the host to the intended LAN / hotspot first.";
    return plan;
  }

  const auto* selected = FindSelected(candidates);
  const auto* recommendedReady = FindFirstReady(candidates, true, false);
  const auto* alternateReady = FindFirstReady(candidates, false, true);
  const auto* anyReady = FindFirstReady(candidates, false, false);

  if (selected && selected->probeReady) {
    plan.selectedReady = true;
    plan.label = L"Selected adapter confirmed";
    plan.detail = L"The current host IP " + selected->ip + L" answered the LAN /health probe.";
    if (readyCount > 1 && alternateReady) {
      plan.alternateReady = true;
      plan.detail += L" Another active adapter also answered, so more than one LAN path looks viable right now.";
    }
    plan.action = L"Keep using the current Viewer URL, or re-export share material after switching adapters if you want a different LAN entry.";
    return plan;
  }

  if (selected && recommendedReady && recommendedReady->ip != selected->ip) {
    plan.alternateReady = true;
    plan.suggestedIp = recommendedReady->ip;
    plan.label = L"Recommended adapter answered";
    plan.detail = L"The selected host IP " + selected->ip + L" did not answer the LAN /health probe, but the recommended adapter " +
                  recommendedReady->ip + L" did answer.";
    plan.action = L"Switch the main host IP to " + recommendedReady->ip + L", then refresh the Viewer URL / offline bundle before handing it off.";
    return plan;
  }

  if (!selected && recommendedReady) {
    plan.alternateReady = true;
    plan.suggestedIp = recommendedReady->ip;
    plan.label = L"Recommended adapter is ready";
    plan.detail = L"No main host IP is selected yet, but the recommended LAN adapter " + recommendedReady->ip +
                  L" already answered the local /health probe.";
    plan.action = L"Select " + recommendedReady->ip + L" as the main host IP before copying or exporting the Viewer URL.";
    return plan;
  }

  if (selected && alternateReady) {
    plan.alternateReady = true;
    plan.suggestedIp = alternateReady->ip;
    plan.label = L"Alternate adapter answered";
    plan.detail = L"The current host IP " + selected->ip + L" did not answer, but another active LAN address " + alternateReady->ip +
                  L" did answer the local /health probe.";
    plan.action = L"Switch to the responding adapter or disable the unrelated adapter before trying the viewer handoff again.";
    return plan;
  }

  if (anyReady) {
    plan.alternateReady = true;
    plan.suggestedIp = anyReady->ip;
    plan.label = L"At least one LAN path is reachable";
    plan.detail = L"The adapter " + anyReady->ip + L" answered the local /health probe, but the current host selection still needs cleanup.";
    plan.action = L"Use the responding LAN adapter as the primary share entry before exporting or copying the Viewer URL again.";
    return plan;
  }

  plan.label = L"No adapter answered";
  plan.detail = L"None of the active LAN adapter candidates answered the local /health probe.";
  plan.action = L"Verify the bind address, Windows Firewall, and same-LAN path first. Then retry adapter detection or collect a remote probe guide.";
  return plan;
}

} // namespace lan::runtime
