// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/pass_builder.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace aster::framegraph {

struct ResourceNode {
  ResourceHandle handle{};
  std::string name;
  ResourceDesc desc{};
};

class FrameGraph {
public:
  ResourceHandle addResource(std::string name, ResourceDesc desc);
  PassBuilder addPass(std::string name);

  [[nodiscard]] const std::vector<ResourceNode> &resources() const {
    return resources_;
  }

  [[nodiscard]] const std::vector<PassDesc> &passes() const {
    return passes_;
  }

  [[nodiscard]] const ResourceNode *resource(ResourceHandle handle) const;
  [[nodiscard]] const PassDesc *pass(std::size_t index) const;

private:
  friend class PassBuilder;

  void addRead(std::size_t pass_index, ResourceHandle resource);
  void addWrite(std::size_t pass_index, ResourceHandle resource);

  std::vector<ResourceNode> resources_;
  std::vector<PassDesc> passes_;
};

} // namespace aster::framegraph
