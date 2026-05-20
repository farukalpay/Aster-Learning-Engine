// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/core/signal.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class ModuleLifecycleState {
  Registered,
  Loading,
  Loaded,
  Failed,
  Unloading,
  Unloaded,
};

enum class ModuleChangeKind {
  Registered,
  Loaded,
  Unloaded,
  Failed,
};

struct ModuleContext {
  std::string module_id;
  std::vector<std::string> diagnostics;

  void addDiagnostic(std::string diagnostic);
};

using ModuleLifecycleFn = std::function<bool(ModuleContext &)>;

struct ModuleDescriptor {
  std::string id;
  std::string display_name;
  std::vector<std::string> dependencies;
  ModuleLifecycleFn initialize;
  ModuleLifecycleFn shutdown;
  bool auto_load = true;
};

struct ModuleStatus {
  std::string id;
  std::string display_name;
  ModuleLifecycleState state = ModuleLifecycleState::Registered;
  std::vector<std::string> dependencies;
  std::vector<std::string> diagnostics;
  std::uint64_t registration_sequence = 0u;
  std::uint64_t load_sequence = 0u;
};

struct ModuleChangeEvent {
  std::string id;
  ModuleChangeKind kind = ModuleChangeKind::Registered;
  ModuleLifecycleState state = ModuleLifecycleState::Registered;
  std::string message;
};

class ModuleRegistry {
public:
  bool registerModule(ModuleDescriptor descriptor);
  bool contains(std::string_view id) const;

  [[nodiscard]] const ModuleStatus *status(std::string_view id) const;
  [[nodiscard]] std::vector<ModuleStatus> statuses() const;
  [[nodiscard]] std::vector<std::string> loadedOrder() const;

  bool loadModule(std::string_view id);
  bool loadAll();
  bool unloadModule(std::string_view id);
  void unloadAll();
  void clear();

  [[nodiscard]] Signal<const ModuleChangeEvent &> &changes() {
    return changes_;
  }

  [[nodiscard]] const Signal<const ModuleChangeEvent &> &changes() const {
    return changes_;
  }

private:
  struct ModuleRecord {
    ModuleDescriptor descriptor;
    ModuleStatus status;
  };

  [[nodiscard]] ModuleRecord *findMutable(std::string_view id);
  [[nodiscard]] const ModuleRecord *findRecord(std::string_view id) const;
  bool loadModuleInternal(std::string_view id, std::vector<std::string> &stack);
  void emitChange(std::string id, ModuleChangeKind kind, ModuleLifecycleState state,
                  std::string message = {});
  void rebuildLoadedOrder();

  std::map<std::string, ModuleRecord> modules_;
  std::vector<std::string> loaded_order_;
  std::uint64_t next_registration_sequence_ = 1u;
  std::uint64_t next_load_sequence_ = 1u;
  Signal<const ModuleChangeEvent &> changes_;
};

[[nodiscard]] const char *moduleLifecycleStateName(ModuleLifecycleState state);
[[nodiscard]] const char *moduleChangeKindName(ModuleChangeKind kind);

} // namespace aster
