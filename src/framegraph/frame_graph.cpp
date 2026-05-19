// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/framegraph/frame_graph.hpp"

#include <algorithm>

namespace aster::framegraph {

PassBuilder::PassBuilder(FrameGraph &graph, const std::size_t pass_index)
    : graph_(&graph), pass_index_(pass_index) {}

PassBuilder &PassBuilder::reads(const ResourceHandle resource) {
  if (graph_ != nullptr) {
    graph_->addRead(pass_index_, resource);
  }
  return *this;
}

PassBuilder &PassBuilder::writes(const ResourceHandle resource) {
  if (graph_ != nullptr) {
    graph_->addWrite(pass_index_, resource);
  }
  return *this;
}

PassBuilder &PassBuilder::queue(const rhi::QueueKind queue) {
  if (graph_ != nullptr && pass_index_ < graph_->passes_.size()) {
    graph_->passes_[pass_index_].queue = queue;
  }
  return *this;
}

ResourceHandle FrameGraph::addResource(std::string name, ResourceDesc desc) {
  const ResourceHandle handle{static_cast<std::uint32_t>(resources_.size()), 1u};
  resources_.push_back({handle, std::move(name), desc});
  return handle;
}

PassBuilder FrameGraph::addPass(std::string name) {
  passes_.push_back({.name = std::move(name)});
  return PassBuilder(*this, passes_.size() - 1u);
}

const ResourceNode *FrameGraph::resource(const ResourceHandle handle) const {
  if (!handle.valid() || handle.index >= resources_.size()) {
    return nullptr;
  }
  const ResourceNode &node = resources_[handle.index];
  return node.handle == handle ? &node : nullptr;
}

const PassDesc *FrameGraph::pass(const std::size_t index) const {
  if (index >= passes_.size()) {
    return nullptr;
  }
  return &passes_[index];
}

void FrameGraph::addRead(const std::size_t pass_index, const ResourceHandle resource) {
  if (pass_index >= passes_.size()) {
    return;
  }
  auto &reads = passes_[pass_index].reads;
  if (std::find(reads.begin(), reads.end(), resource) == reads.end()) {
    reads.push_back(resource);
  }
}

void FrameGraph::addWrite(const std::size_t pass_index, const ResourceHandle resource) {
  if (pass_index >= passes_.size()) {
    return;
  }
  auto &writes = passes_[pass_index].writes;
  if (std::find(writes.begin(), writes.end(), resource) == writes.end()) {
    writes.push_back(resource);
  }
}

} // namespace aster::framegraph
