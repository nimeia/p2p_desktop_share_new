#pragma once

#include <string>
#include <vector>

namespace lan::runtime {

struct RemoteProbeCandidateInput {
  std::wstring name;
  std::wstring ip;
  std::wstring type;
  bool recommended = false;
  bool selected = false;
  bool probeReady = false;
  std::wstring probeDetail;
};

struct RemoteProbeCandidateViewModel {
  std::wstring name;
  std::wstring ip;
  std::wstring type;
  bool recommended = false;
  bool selected = false;
  bool probeReady = false;
  std::wstring probeLabel;
  std::wstring probeDetail;
};

struct RemoteProbePlan {
  std::wstring label;
  std::wstring detail;
  std::wstring action;
  std::wstring suggestedIp;
  bool selectedReady = false;
  bool alternateReady = false;
  std::vector<RemoteProbeCandidateViewModel> candidates;
};

RemoteProbePlan BuildRemoteProbePlan(const std::vector<RemoteProbeCandidateInput>& candidates);

} // namespace lan::runtime
