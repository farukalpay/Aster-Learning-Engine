// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <functional>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace aster {

enum class BehaviorStatus {
  Failure,
  Running,
  Success,
};

struct BehaviorTraceEvent {
  std::string_view name;
  BehaviorStatus status = BehaviorStatus::Failure;
};

struct BehaviorTrace {
  std::vector<BehaviorTraceEvent> events;
};

struct BehaviorBranch {
  std::string_view name;
  std::function<BehaviorStatus()> tick;
};

[[nodiscard]] BehaviorStatus tickSequence(std::initializer_list<BehaviorBranch> branches,
                                          BehaviorTrace *trace = nullptr);
[[nodiscard]] BehaviorStatus tickSelector(std::initializer_list<BehaviorBranch> branches,
                                          BehaviorTrace *trace = nullptr);
[[nodiscard]] BehaviorBranch behaviorCondition(std::string_view name, bool value);
[[nodiscard]] BehaviorBranch behaviorAction(std::string_view name,
                                            std::function<BehaviorStatus()> action);

} // namespace aster
