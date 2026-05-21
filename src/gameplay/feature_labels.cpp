// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/gameplay/feature_labels.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace aster {

namespace {

bool isAsciiAlnum(const unsigned char ch) {
  return std::isalnum(ch) != 0;
}

std::uint64_t commonLineageDepth(const FeatureLabel &lhs, const FeatureLabel &rhs) {
  const std::string_view a = lhs.path();
  const std::string_view b = rhs.path();
  std::size_t last_dot = std::string_view::npos;
  std::size_t index = 0u;
  while (index < a.size() && index < b.size() && a[index] == b[index]) {
    if (a[index] == '.') {
      last_dot = index;
    }
    ++index;
  }
  if (index == a.size() && index == b.size()) {
    return static_cast<std::uint64_t>(lhs.depth());
  }
  if (index == a.size() && index < b.size() && b[index] == '.') {
    return static_cast<std::uint64_t>(lhs.depth());
  }
  if (index == b.size() && index < a.size() && a[index] == '.') {
    return static_cast<std::uint64_t>(rhs.depth());
  }
  if (last_dot == std::string_view::npos) {
    return 0u;
  }
  return static_cast<std::uint64_t>(
      std::count(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(last_dot), '.') + 1);
}

void sortUniqueLabels(std::vector<FeatureLabel> &labels) {
  std::sort(labels.begin(), labels.end());
  labels.erase(std::unique(labels.begin(), labels.end()), labels.end());
}

} // namespace

FeatureLabel::FeatureLabel(std::string canonical_path)
    : path_(std::move(canonical_path)), stable_id_(stableFeatureLabelId(path_)) {
  if (path_.empty()) {
    stable_id_ = 0u;
  }
}

std::optional<FeatureLabel> FeatureLabel::parse(const std::string_view text) {
  return parseFeatureLabel(text);
}

bool FeatureLabel::valid() const noexcept {
  return !path_.empty();
}

std::string_view FeatureLabel::path() const noexcept {
  return path_;
}

std::uint64_t FeatureLabel::stableId() const noexcept {
  return stable_id_;
}

std::size_t FeatureLabel::depth() const {
  if (path_.empty()) {
    return 0u;
  }
  return static_cast<std::size_t>(std::count(path_.begin(), path_.end(), '.')) + 1u;
}

std::optional<FeatureLabel> FeatureLabel::parent() const {
  const std::size_t dot = path_.rfind('.');
  if (dot == std::string::npos) {
    return std::nullopt;
  }
  return FeatureLabel(path_.substr(0u, dot));
}

std::vector<FeatureLabel> FeatureLabel::lineage(const bool include_self) const {
  std::vector<FeatureLabel> result;
  if (!valid()) {
    return result;
  }
  std::string current = path_;
  if (include_self) {
    result.push_back(FeatureLabel(current));
  }
  while (true) {
    const std::size_t dot = current.rfind('.');
    if (dot == std::string::npos) {
      break;
    }
    current = current.substr(0u, dot);
    result.push_back(FeatureLabel(current));
  }
  std::reverse(result.begin(), result.end());
  return result;
}

bool FeatureLabel::matches(const FeatureLabel &query) const {
  if (!valid() || !query.valid()) {
    return false;
  }
  if (path_ == query.path_) {
    return true;
  }
  if (path_.size() <= query.path_.size()) {
    return false;
  }
  return path_.compare(0u, query.path_.size(), query.path_) == 0 &&
         path_[query.path_.size()] == '.';
}

bool FeatureLabel::isDirectChildOf(const FeatureLabel &parent_label) const {
  return matches(parent_label) && depth() == parent_label.depth() + 1u;
}

std::string canonicalizeFeatureLabel(const std::string_view text) {
  std::vector<std::string> segments;
  std::string segment;
  const auto flush_segment = [&]() -> bool {
    while (!segment.empty() && segment.front() == '_') {
      segment.erase(segment.begin());
    }
    while (!segment.empty() && segment.back() == '_') {
      segment.pop_back();
    }
    if (segment.empty()) {
      return true;
    }
    segments.push_back(segment);
    segment.clear();
    return true;
  };

  for (const char raw : text) {
    const auto ch = static_cast<unsigned char>(raw);
    if (ch == '.' || ch == '/' || ch == ':' || ch == '\\') {
      if (!flush_segment()) {
        return {};
      }
      continue;
    }
    if (std::isspace(ch) != 0 || ch == '-') {
      if (!segment.empty() && segment.back() != '_') {
        segment.push_back('_');
      }
      continue;
    }
    if (ch == '_') {
      if (!segment.empty() && segment.back() != '_') {
        segment.push_back('_');
      }
      continue;
    }
    if (isAsciiAlnum(ch)) {
      segment.push_back(static_cast<char>(std::tolower(ch)));
      continue;
    }
    return {};
  }
  if (!flush_segment()) {
    return {};
  }
  if (segments.empty()) {
    return {};
  }

  std::ostringstream out;
  for (std::size_t i = 0u; i < segments.size(); ++i) {
    if (i > 0u) {
      out << '.';
    }
    out << segments[i];
  }
  return out.str();
}

std::optional<FeatureLabel> parseFeatureLabel(const std::string_view text) {
  const std::string canonical = canonicalizeFeatureLabel(text);
  if (canonical.empty()) {
    return std::nullopt;
  }
  return FeatureLabel(canonical);
}

std::uint64_t stableFeatureLabelId(const std::string_view canonical_path) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const char ch : canonical_path) {
    hash ^= static_cast<unsigned char>(ch);
    hash *= 1099511628211ull;
  }
  return hash64Mix(hash);
}

const char *featureLabelMatchModeName(const FeatureLabelMatchMode mode) {
  switch (mode) {
  case FeatureLabelMatchMode::Exact:
    return "exact";
  case FeatureLabelMatchMode::IncludeDescendants:
    return "include_descendants";
  }
  return "unknown";
}

bool FeatureLabelSet::add(FeatureLabel label) {
  if (!label.valid() || contains(label, FeatureLabelMatchMode::Exact)) {
    return false;
  }
  labels_.push_back(std::move(label));
  sortUniqueLabels(labels_);
  return true;
}

bool FeatureLabelSet::add(const std::string_view label) {
  const std::optional<FeatureLabel> parsed = parseFeatureLabel(label);
  return parsed ? add(*parsed) : false;
}

bool FeatureLabelSet::remove(const FeatureLabel &label) {
  const auto found = std::find(labels_.begin(), labels_.end(), label);
  if (found == labels_.end()) {
    return false;
  }
  labels_.erase(found);
  return true;
}

void FeatureLabelSet::clear() {
  labels_.clear();
}

bool FeatureLabelSet::empty() const noexcept {
  return labels_.empty();
}

std::size_t FeatureLabelSet::size() const noexcept {
  return labels_.size();
}

bool FeatureLabelSet::contains(const FeatureLabel &label, const FeatureLabelMatchMode mode) const {
  if (!label.valid()) {
    return false;
  }
  for (const FeatureLabel &candidate : labels_) {
    if (mode == FeatureLabelMatchMode::Exact ? candidate == label : candidate.matches(label)) {
      return true;
    }
  }
  return false;
}

bool FeatureLabelSet::contains(const std::string_view label, const FeatureLabelMatchMode mode) const {
  const std::optional<FeatureLabel> parsed = parseFeatureLabel(label);
  return parsed ? contains(*parsed, mode) : false;
}

bool FeatureLabelSet::containsAll(const std::vector<FeatureLabel> &labels,
                                  const FeatureLabelMatchMode mode) const {
  for (const FeatureLabel &label : labels) {
    if (!contains(label, mode)) {
      return false;
    }
  }
  return true;
}

bool FeatureLabelSet::containsAny(const std::vector<FeatureLabel> &labels,
                                  const FeatureLabelMatchMode mode) const {
  for (const FeatureLabel &label : labels) {
    if (contains(label, mode)) {
      return true;
    }
  }
  return false;
}

std::vector<FeatureLabel> FeatureLabelSet::labels() const {
  return labels_;
}

std::uint64_t FeatureLabelSet::contractStamp() const {
  std::uint64_t stamp = 0xA57E1ABE10000001ull;
  for (const FeatureLabel &label : labels_) {
    stamp = hashCombine64(stamp, label.stableId());
  }
  return stamp;
}

float FeatureLabelSet::lineageAffinity(const FeatureLabelSet &other) const {
  if (labels_.empty() && other.labels_.empty()) {
    return 1.0f;
  }
  if (labels_.empty() || other.labels_.empty()) {
    return 0.0f;
  }

  float total = 0.0f;
  for (const FeatureLabel &lhs : labels_) {
    float best = 0.0f;
    for (const FeatureLabel &rhs : other.labels_) {
      const std::uint64_t common = commonLineageDepth(lhs, rhs);
      const float denom = static_cast<float>(std::max(lhs.depth(), rhs.depth()));
      best = std::max(best, denom > 0.0f ? static_cast<float>(common) / denom : 0.0f);
    }
    total += best;
  }
  return total / static_cast<float>(labels_.size());
}

bool FeatureLabelQuery::matches(const FeatureLabelSet &labels) const {
  if (!labels.containsAll(all, mode)) {
    return false;
  }
  if (!any.empty() && !labels.containsAny(any, mode)) {
    return false;
  }
  for (const FeatureLabel &blocked : none) {
    if (labels.contains(blocked, mode)) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> FeatureLabelQuery::explainMismatch(const FeatureLabelSet &labels) const {
  std::vector<std::string> diagnostics;
  for (const FeatureLabel &required : all) {
    if (!labels.contains(required, mode)) {
      diagnostics.push_back("missing required label: " + std::string(required.path()));
    }
  }
  if (!any.empty() && !labels.containsAny(any, mode)) {
    diagnostics.push_back("missing one-of label group");
  }
  for (const FeatureLabel &blocked : none) {
    if (labels.contains(blocked, mode)) {
      diagnostics.push_back("blocked by label: " + std::string(blocked.path()));
    }
  }
  return diagnostics;
}

bool FeatureLabelCatalog::declare(FeatureLabelRecord record) {
  if (!record.label.valid() || contains(record.label)) {
    return false;
  }
  if (record.display_name.empty()) {
    record.display_name = std::string(record.label.path());
  }
  record.declaration_sequence = next_declaration_sequence_++;
  records_.push_back(std::move(record));
  std::sort(records_.begin(), records_.end(),
            [](const FeatureLabelRecord &lhs, const FeatureLabelRecord &rhs) {
              return lhs.declaration_sequence < rhs.declaration_sequence;
            });
  return true;
}

bool FeatureLabelCatalog::declare(const std::string_view label, std::string display_name,
                                  std::string description, const float weight) {
  const std::optional<FeatureLabel> parsed = parseFeatureLabel(label);
  if (!parsed) {
    return false;
  }
  return declare({*parsed, std::move(display_name), std::move(description), weight});
}

bool FeatureLabelCatalog::addAlias(const std::string_view alias, const std::string_view target) {
  const std::string canonical_alias = canonicalizeFeatureLabel(alias);
  const std::optional<FeatureLabel> resolved_target = resolve(target);
  if (canonical_alias.empty() || !resolved_target) {
    return false;
  }
  aliases_.push_back({canonical_alias, *resolved_target});
  return true;
}

std::optional<FeatureLabel> FeatureLabelCatalog::resolve(const std::string_view text) const {
  const std::string canonical = canonicalizeFeatureLabel(text);
  if (canonical.empty()) {
    return std::nullopt;
  }
  for (const auto &[alias, target] : aliases_) {
    if (alias == canonical) {
      return target;
    }
  }
  return FeatureLabel(canonical);
}

const FeatureLabelRecord *FeatureLabelCatalog::find(const FeatureLabel &label) const {
  for (const FeatureLabelRecord &record : records_) {
    if (record.label == label) {
      return &record;
    }
  }
  return nullptr;
}

bool FeatureLabelCatalog::contains(const FeatureLabel &label) const {
  return find(label) != nullptr;
}

std::vector<FeatureLabelRecord> FeatureLabelCatalog::records() const {
  return records_;
}

void FeatureLabelCatalog::clear() {
  records_.clear();
  aliases_.clear();
  next_declaration_sequence_ = 1u;
}

} // namespace aster
