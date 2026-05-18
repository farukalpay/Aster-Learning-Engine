// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct ShaderLibraryDiagnostic {
  std::filesystem::path source_path;
  std::string message;
};

struct ShaderLibraryModule {
  std::string name;
  std::filesystem::path path;
  std::string source;
};

class ShaderLibrary {
public:
  bool addModule(std::string name, std::string source, std::filesystem::path path = {});
  [[nodiscard]] const ShaderLibraryModule *find(std::string_view name) const;
  [[nodiscard]] std::string compose(const std::vector<std::string> &modules) const;
  [[nodiscard]] const std::map<std::string, ShaderLibraryModule> &modules() const;

private:
  std::map<std::string, ShaderLibraryModule> modules_;
};

struct ShaderLibraryLoadResult {
  ShaderLibrary library;
  std::vector<ShaderLibraryDiagnostic> diagnostics;
  [[nodiscard]] bool ok() const;
};

[[nodiscard]] ShaderLibraryLoadResult loadShaderLibrary(const std::filesystem::path &directory);

} // namespace aster
