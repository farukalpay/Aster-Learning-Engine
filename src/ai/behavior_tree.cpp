// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ai/behavior_tree.hpp"

#include <utility>

namespace aster {
namespace {

void recordTrace(BehaviorTrace *trace, const BehaviorBranch &branch, const BehaviorStatus status) {
  if (trace != nullptr) {
    trace->events.push_back({branch.name, status});
  }
}

} // namespace

BehaviorStatus tickSequence(const std::initializer_list<BehaviorBranch> branches,
                            BehaviorTrace *trace) {
  for (const BehaviorBranch &branch : branches) {
    const BehaviorStatus status = branch.tick != nullptr ? branch.tick() : BehaviorStatus::Failure;
    recordTrace(trace, branch, status);
    if (status != BehaviorStatus::Success) {
      return status;
    }
  }
  return BehaviorStatus::Success;
}

BehaviorStatus tickSelector(const std::initializer_list<BehaviorBranch> branches,
                            BehaviorTrace *trace) {
  for (const BehaviorBranch &branch : branches) {
    const BehaviorStatus status = branch.tick != nullptr ? branch.tick() : BehaviorStatus::Failure;
    recordTrace(trace, branch, status);
    if (status != BehaviorStatus::Failure) {
      return status;
    }
  }
  return BehaviorStatus::Failure;
}

BehaviorBranch behaviorCondition(const std::string_view name, const bool value) {
  return {name, [value] { return value ? BehaviorStatus::Success : BehaviorStatus::Failure; }};
}

BehaviorBranch behaviorAction(const std::string_view name, std::function<BehaviorStatus()> action) {
  return {name, std::move(action)};
}

} // namespace aster
