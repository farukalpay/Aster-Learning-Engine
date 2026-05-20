// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/shader/shader_preprocessor.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>

namespace aster {
namespace {

struct ResolvedInclude {
  std::string logical_path;
  std::filesystem::path resolved_path;
  std::string source;
};

[[nodiscard]] std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::string trimmedLeft(const std::string_view text) {
  std::size_t cursor = 0u;
  while (cursor < text.size() &&
         (text[cursor] == ' ' || text[cursor] == '\t' || text[cursor] == '\r')) {
    ++cursor;
  }
  return std::string(text.substr(cursor));
}

[[nodiscard]] bool startsWith(const std::string_view text, const std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0u, prefix.size()) == prefix;
}

void replaceAll(std::string &text, const std::string &from, const std::string &to) {
  if (from.empty()) {
    return;
  }
  std::size_t cursor = 0u;
  while ((cursor = text.find(from, cursor)) != std::string::npos) {
    text.replace(cursor, from.size(), to);
    cursor += to.size();
  }
}

[[nodiscard]] std::string lowerAstslForBackend(std::string source,
                                               const std::string_view backend_name) {
  if (backend_name == "d3d12-hlsl") {
    replaceAll(source, "mix(", "lerp(");
  }
  return source;
}

[[nodiscard]] std::optional<std::string> parseIncludeName(const std::string_view line) {
  const std::string trimmed = trimmedLeft(line);
  if (!startsWith(trimmed, "#include")) {
    return std::nullopt;
  }
  const std::size_t start = trimmed.find_first_of("\"<");
  if (start == std::string::npos) {
    return std::nullopt;
  }
  const char close = trimmed[start] == '<' ? '>' : '"';
  const std::size_t end = trimmed.find(close, start + 1u);
  if (end == std::string::npos || end <= start + 1u) {
    return std::nullopt;
  }
  return trimmed.substr(start + 1u, end - start - 1u);
}

[[nodiscard]] std::string moduleNameForInclude(const std::string_view include_name) {
  std::filesystem::path path(include_name);
  if (path.extension() == ".astsl") {
    return path.stem().generic_string();
  }
  return std::string(include_name);
}

[[nodiscard]] std::optional<ResolvedInclude>
resolveInclude(const ShaderLibrary &library, const ShaderPreprocessRequest &request,
               const std::string_view include_name) {
  const std::string exact(include_name);
  if (const auto found = request.in_memory_sources.find(exact);
      found != request.in_memory_sources.end()) {
    return ResolvedInclude{.logical_path = exact,
                           .resolved_path = std::filesystem::path("memory:") / exact,
                           .source = found->second};
  }

  const std::string module_name = moduleNameForInclude(include_name);
  if (const ShaderLibraryModule *module = library.find(module_name)) {
    return ResolvedInclude{.logical_path = module->path.empty() ? module->name
                                                                : module->path.filename().string(),
                           .resolved_path = module->path.empty()
                                                ? std::filesystem::path("library:") / module->name
                                                : module->path,
                           .source = module->source};
  }

  if (const ShaderLibraryModule *module = library.find(exact)) {
    return ResolvedInclude{.logical_path = module->path.empty() ? module->name
                                                                : module->path.filename().string(),
                           .resolved_path = module->path.empty()
                                                ? std::filesystem::path("library:") / module->name
                                                : module->path,
                           .source = module->source};
  }

  for (const std::filesystem::path &root : request.include_roots) {
    const std::filesystem::path candidate = root / exact;
    if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
      return ResolvedInclude{.logical_path = exact,
                             .resolved_path = candidate,
                             .source = readText(candidate)};
    }
    if (std::filesystem::path(exact).extension().empty()) {
      const std::filesystem::path with_extension = root / (exact + ".astsl");
      if (std::filesystem::exists(with_extension) &&
          std::filesystem::is_regular_file(with_extension)) {
        return ResolvedInclude{.logical_path = exact + ".astsl",
                               .resolved_path = with_extension,
                               .source = readText(with_extension)};
      }
    }
  }
  return std::nullopt;
}

class PreprocessContext {
public:
  PreprocessContext(const ShaderLibrary &library, const ShaderPreprocessRequest &request)
      : library_(library), request_(request) {}

  [[nodiscard]] ShaderPreprocessResult run() {
    ShaderPreprocessResult result;
    std::ostringstream root;
    std::map<std::string, std::string> defines;
    for (const ShaderPermutationDefine &define : request_.defines) {
      defines[define.name] = define.value;
    }
    for (const auto &[name, value] : defines) {
      root << "#define " << name << ' ' << value << '\n';
    }
    for (const std::string &module : request_.modules) {
      root << "#include \"" << module << "\"\n";
    }
    root << request_.source;
    if (!request_.source.empty() && request_.source.back() != '\n') {
      root << '\n';
    }
    std::vector<std::string> include_stack;
    result.source = processSource(request_.source_name, root.str(), include_stack);
    result.lowered_source = lowerAstslForBackend(result.source, request_.backend_name);
    result.source_hash = stableShaderHash(result.lowered_source);
    result.diagnostics = diagnostics_;
    for (const auto &[logical_path, dependency] : dependencies_) {
      (void)logical_path;
      result.dependencies.push_back(dependency);
    }
    std::sort(result.dependencies.begin(), result.dependencies.end(),
              [](const ShaderPreprocessDependency &lhs,
                 const ShaderPreprocessDependency &rhs) {
                return lhs.logical_path < rhs.logical_path;
              });
    result.success = result.diagnostics.empty();
    return result;
  }

private:
  [[nodiscard]] std::string processSource(const std::string &source_name,
                                          const std::string &source,
                                          std::vector<std::string> &include_stack) {
    std::ostringstream output;
    std::istringstream input(source);
    std::string line;
    std::size_t line_number = 0u;
    while (std::getline(input, line)) {
      ++line_number;
      const std::optional<std::string> include_name = parseIncludeName(line);
      if (!include_name.has_value()) {
        output << line << '\n';
        continue;
      }
      const std::optional<ResolvedInclude> resolved =
          resolveInclude(library_, request_, *include_name);
      if (!resolved.has_value()) {
        diagnostics_.push_back(
            {.kind = ShaderPreprocessDiagnosticKind::MissingInclude,
             .source_name = source_name,
             .line = line_number,
             .include_name = *include_name,
             .message = "shader include '" + *include_name + "' could not be resolved"});
        output << "// missing include: " << *include_name << '\n';
        continue;
      }
      if (std::find(include_stack.begin(), include_stack.end(), resolved->logical_path) !=
          include_stack.end()) {
        diagnostics_.push_back(
            {.kind = ShaderPreprocessDiagnosticKind::CyclicInclude,
             .source_name = source_name,
             .line = line_number,
             .include_name = resolved->logical_path,
             .message = "shader include cycle detected at '" + resolved->logical_path + "'"});
        output << "// cyclic include: " << resolved->logical_path << '\n';
        continue;
      }

      dependencies_.emplace(resolved->logical_path,
                            ShaderPreprocessDependency{.logical_path = resolved->logical_path,
                                                       .resolved_path = resolved->resolved_path,
                                                       .source_hash =
                                                           stableShaderHash(resolved->source)});
      include_stack.push_back(resolved->logical_path);
      output << "// begin include: " << resolved->logical_path << '\n';
      output << processSource(resolved->logical_path, resolved->source, include_stack);
      output << "// end include: " << resolved->logical_path << '\n';
      include_stack.pop_back();
    }
    return output.str();
  }

  const ShaderLibrary &library_;
  const ShaderPreprocessRequest &request_;
  std::map<std::string, ShaderPreprocessDependency> dependencies_;
  std::vector<ShaderPreprocessDiagnostic> diagnostics_;
};

} // namespace

std::string_view
shaderPreprocessDiagnosticKindName(const ShaderPreprocessDiagnosticKind kind) {
  switch (kind) {
  case ShaderPreprocessDiagnosticKind::MissingInclude:
    return "missing-include";
  case ShaderPreprocessDiagnosticKind::CyclicInclude:
    return "cyclic-include";
  case ShaderPreprocessDiagnosticKind::IoError:
    return "io-error";
  case ShaderPreprocessDiagnosticKind::InvalidDirective:
    return "invalid-directive";
  }
  return "invalid-directive";
}

ShaderPreprocessResult preprocessShaderSource(const ShaderLibrary &library,
                                              const ShaderPreprocessRequest &request) {
  return PreprocessContext(library, request).run();
}

} // namespace aster
