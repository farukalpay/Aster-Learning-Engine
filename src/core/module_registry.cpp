// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/module_registry.hpp"

#include <algorithm>
#include <sstream>

namespace aster {

namespace {

bool containsString(const std::vector<std::string> &values, const std::string_view value) {
  return std::any_of(values.begin(), values.end(),
                     [&](const std::string &candidate) { return candidate == value; });
}

} // namespace

void ModuleContext::addDiagnostic(std::string diagnostic) {
  diagnostics.push_back(std::move(diagnostic));
}

bool ModuleRegistry::registerModule(ModuleDescriptor descriptor) {
  if (descriptor.id.empty() || modules_.find(descriptor.id) != modules_.end()) {
    return false;
  }
  if (descriptor.display_name.empty()) {
    descriptor.display_name = descriptor.id;
  }

  ModuleStatus status;
  status.id = descriptor.id;
  status.display_name = descriptor.display_name;
  status.dependencies = descriptor.dependencies;
  status.registration_sequence = next_registration_sequence_++;
  const std::string id = descriptor.id;
  modules_.emplace(id, ModuleRecord{std::move(descriptor), std::move(status)});
  emitChange(id, ModuleChangeKind::Registered, ModuleLifecycleState::Registered);
  return true;
}

bool ModuleRegistry::contains(const std::string_view id) const {
  return findRecord(id) != nullptr;
}

const ModuleStatus *ModuleRegistry::status(const std::string_view id) const {
  const ModuleRecord *record = findRecord(id);
  return record ? &record->status : nullptr;
}

std::vector<ModuleStatus> ModuleRegistry::statuses() const {
  std::vector<ModuleStatus> result;
  result.reserve(modules_.size());
  for (const auto &[id, record] : modules_) {
    (void)id;
    result.push_back(record.status);
  }
  std::sort(result.begin(), result.end(), [](const ModuleStatus &lhs, const ModuleStatus &rhs) {
    return lhs.registration_sequence < rhs.registration_sequence;
  });
  return result;
}

std::vector<std::string> ModuleRegistry::loadedOrder() const {
  return loaded_order_;
}

bool ModuleRegistry::loadModule(const std::string_view id) {
  std::vector<std::string> stack;
  return loadModuleInternal(id, stack);
}

bool ModuleRegistry::loadAll() {
  bool ok = true;
  std::vector<ModuleStatus> ordered = statuses();
  for (const ModuleStatus &status_record : ordered) {
    const ModuleRecord *record = findRecord(status_record.id);
    if (record && record->descriptor.auto_load) {
      ok = loadModule(status_record.id) && ok;
    }
  }
  return ok;
}

bool ModuleRegistry::unloadModule(const std::string_view id) {
  ModuleRecord *record = findMutable(id);
  if (!record) {
    return false;
  }
  if (record->status.state == ModuleLifecycleState::Registered ||
      record->status.state == ModuleLifecycleState::Unloaded) {
    return true;
  }
  if (record->status.state != ModuleLifecycleState::Loaded &&
      record->status.state != ModuleLifecycleState::Failed) {
    return false;
  }

  std::vector<std::string> dependents;
  for (const auto &[candidate_id, candidate] : modules_) {
    if (candidate.status.state == ModuleLifecycleState::Loaded &&
        containsString(candidate.status.dependencies, id)) {
      dependents.push_back(candidate_id);
    }
  }
  for (const std::string &dependent : dependents) {
    unloadModule(dependent);
  }

  record = findMutable(id);
  if (!record) {
    return false;
  }
  record->status.state = ModuleLifecycleState::Unloading;
  ModuleContext context;
  context.module_id = record->descriptor.id;
  bool ok = true;
  if (record->descriptor.shutdown) {
    ok = record->descriptor.shutdown(context);
  }
  record->status.diagnostics.insert(record->status.diagnostics.end(), context.diagnostics.begin(),
                                    context.diagnostics.end());
  record->status.state = ok ? ModuleLifecycleState::Unloaded : ModuleLifecycleState::Failed;
  record->status.load_sequence = 0u;
  rebuildLoadedOrder();
  emitChange(record->status.id, ok ? ModuleChangeKind::Unloaded : ModuleChangeKind::Failed,
             record->status.state);
  return ok;
}

void ModuleRegistry::unloadAll() {
  const std::vector<std::string> order = loaded_order_;
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    unloadModule(*it);
  }
}

void ModuleRegistry::clear() {
  modules_.clear();
  loaded_order_.clear();
  next_registration_sequence_ = 1u;
  next_load_sequence_ = 1u;
}

ModuleRegistry::ModuleRecord *ModuleRegistry::findMutable(const std::string_view id) {
  const auto found = modules_.find(std::string(id));
  return found == modules_.end() ? nullptr : &found->second;
}

const ModuleRegistry::ModuleRecord *ModuleRegistry::findRecord(const std::string_view id) const {
  const auto found = modules_.find(std::string(id));
  return found == modules_.end() ? nullptr : &found->second;
}

bool ModuleRegistry::loadModuleInternal(const std::string_view id, std::vector<std::string> &stack) {
  ModuleRecord *record = findMutable(id);
  if (!record) {
    return false;
  }
  if (record->status.state == ModuleLifecycleState::Loaded) {
    return true;
  }
  if (record->status.state == ModuleLifecycleState::Loading || containsString(stack, id)) {
    record->status.state = ModuleLifecycleState::Failed;
    record->status.diagnostics.push_back("module dependency cycle detected");
    emitChange(record->status.id, ModuleChangeKind::Failed, record->status.state,
               "dependency cycle");
    return false;
  }

  stack.push_back(record->status.id);
  for (const std::string &dependency : record->descriptor.dependencies) {
    if (!loadModuleInternal(dependency, stack)) {
      record = findMutable(id);
      if (record) {
        record->status.state = ModuleLifecycleState::Failed;
        record->status.diagnostics.push_back("failed to load dependency: " + dependency);
        emitChange(record->status.id, ModuleChangeKind::Failed, record->status.state,
                   "dependency failed");
      }
      stack.pop_back();
      return false;
    }
  }
  stack.pop_back();

  record = findMutable(id);
  if (!record) {
    return false;
  }
  record->status.state = ModuleLifecycleState::Loading;
  ModuleContext context;
  context.module_id = record->descriptor.id;
  bool ok = true;
  if (record->descriptor.initialize) {
    ok = record->descriptor.initialize(context);
  }
  record->status.diagnostics.insert(record->status.diagnostics.end(), context.diagnostics.begin(),
                                    context.diagnostics.end());
  record->status.state = ok ? ModuleLifecycleState::Loaded : ModuleLifecycleState::Failed;
  if (ok) {
    record->status.load_sequence = next_load_sequence_++;
    rebuildLoadedOrder();
    emitChange(record->status.id, ModuleChangeKind::Loaded, record->status.state);
  } else {
    emitChange(record->status.id, ModuleChangeKind::Failed, record->status.state,
               "initialize returned false");
  }
  return ok;
}

void ModuleRegistry::emitChange(std::string id, const ModuleChangeKind kind,
                                const ModuleLifecycleState state, std::string message) {
  changes_.emit(ModuleChangeEvent{std::move(id), kind, state, std::move(message)});
}

void ModuleRegistry::rebuildLoadedOrder() {
  loaded_order_.clear();
  std::vector<ModuleStatus> loaded;
  for (const auto &[id, record] : modules_) {
    (void)id;
    if (record.status.state == ModuleLifecycleState::Loaded) {
      loaded.push_back(record.status);
    }
  }
  std::sort(loaded.begin(), loaded.end(), [](const ModuleStatus &lhs, const ModuleStatus &rhs) {
    return lhs.load_sequence < rhs.load_sequence;
  });
  for (const ModuleStatus &status_record : loaded) {
    loaded_order_.push_back(status_record.id);
  }
}

const char *moduleLifecycleStateName(const ModuleLifecycleState state) {
  switch (state) {
  case ModuleLifecycleState::Registered:
    return "registered";
  case ModuleLifecycleState::Loading:
    return "loading";
  case ModuleLifecycleState::Loaded:
    return "loaded";
  case ModuleLifecycleState::Failed:
    return "failed";
  case ModuleLifecycleState::Unloading:
    return "unloading";
  case ModuleLifecycleState::Unloaded:
    return "unloaded";
  }
  return "unknown";
}

const char *moduleChangeKindName(const ModuleChangeKind kind) {
  switch (kind) {
  case ModuleChangeKind::Registered:
    return "registered";
  case ModuleChangeKind::Loaded:
    return "loaded";
  case ModuleChangeKind::Unloaded:
    return "unloaded";
  case ModuleChangeKind::Failed:
    return "failed";
  }
  return "unknown";
}

} // namespace aster
