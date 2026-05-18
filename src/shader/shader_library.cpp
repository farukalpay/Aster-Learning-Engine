// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/shader/shader_library.hpp"

#include <fstream>
#include <iterator>

namespace aster {
namespace {

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

bool ShaderLibrary::addModule(std::string name, std::string source, std::filesystem::path path) {
  if (name.empty()) {
    return false;
  }
  modules_[name] = {.name = std::move(name), .path = std::move(path), .source = std::move(source)};
  return true;
}

const ShaderLibraryModule *ShaderLibrary::find(const std::string_view name) const {
  const auto it = modules_.find(std::string(name));
  return it == modules_.end() ? nullptr : &it->second;
}

std::string ShaderLibrary::compose(const std::vector<std::string> &modules) const {
  std::string source;
  for (const std::string &name : modules) {
    if (const ShaderLibraryModule *module = find(name)) {
      source += "// module: ";
      source += module->name;
      source += "\n";
      source += module->source;
      if (!source.empty() && source.back() != '\n') {
        source += '\n';
      }
    }
  }
  return source;
}

const std::map<std::string, ShaderLibraryModule> &ShaderLibrary::modules() const {
  return modules_;
}

bool ShaderLibraryLoadResult::ok() const {
  return diagnostics.empty();
}

ShaderLibraryLoadResult loadShaderLibrary(const std::filesystem::path &directory) {
  ShaderLibraryLoadResult result;
  if (!std::filesystem::exists(directory)) {
    result.diagnostics.push_back({.source_path = directory, .message = "shader library directory is missing"});
    return result;
  }
  for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".astsl") {
      continue;
    }
    std::ifstream probe(entry.path(), std::ios::binary);
    if (!probe) {
      result.diagnostics.push_back({.source_path = entry.path(), .message = "could not read shader module"});
      continue;
    }
    std::string name = entry.path().stem().string();
    result.library.addModule(std::move(name), readText(entry.path()), entry.path());
  }
  return result;
}

} // namespace aster
