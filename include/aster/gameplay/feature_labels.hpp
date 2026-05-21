// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class FeatureLabelMatchMode {
  Exact,
  IncludeDescendants,
};

class FeatureLabel {
public:
  FeatureLabel() = default;
  explicit FeatureLabel(std::string canonical_path);

  [[nodiscard]] static std::optional<FeatureLabel> parse(std::string_view text);

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] std::string_view path() const noexcept;
  [[nodiscard]] std::uint64_t stableId() const noexcept;
  [[nodiscard]] std::size_t depth() const;
  [[nodiscard]] std::optional<FeatureLabel> parent() const;
  [[nodiscard]] std::vector<FeatureLabel> lineage(bool include_self = true) const;
  [[nodiscard]] bool matches(const FeatureLabel &query) const;
  [[nodiscard]] bool isDirectChildOf(const FeatureLabel &parent_label) const;

  friend bool operator==(const FeatureLabel &lhs, const FeatureLabel &rhs) noexcept {
    return lhs.path_ == rhs.path_;
  }

  friend bool operator!=(const FeatureLabel &lhs, const FeatureLabel &rhs) noexcept {
    return !(lhs == rhs);
  }

  friend bool operator<(const FeatureLabel &lhs, const FeatureLabel &rhs) noexcept {
    return lhs.path_ < rhs.path_;
  }

private:
  std::string path_;
  std::uint64_t stable_id_ = 0u;
};

[[nodiscard]] std::string canonicalizeFeatureLabel(std::string_view text);
[[nodiscard]] std::optional<FeatureLabel> parseFeatureLabel(std::string_view text);
[[nodiscard]] std::uint64_t stableFeatureLabelId(std::string_view canonical_path);
[[nodiscard]] const char *featureLabelMatchModeName(FeatureLabelMatchMode mode);

class FeatureLabelSet {
public:
  bool add(FeatureLabel label);
  bool add(std::string_view label);
  bool remove(const FeatureLabel &label);
  void clear();

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool contains(const FeatureLabel &label,
                              FeatureLabelMatchMode mode = FeatureLabelMatchMode::IncludeDescendants) const;
  [[nodiscard]] bool contains(std::string_view label,
                              FeatureLabelMatchMode mode = FeatureLabelMatchMode::IncludeDescendants) const;
  [[nodiscard]] bool containsAll(const std::vector<FeatureLabel> &labels,
                                 FeatureLabelMatchMode mode = FeatureLabelMatchMode::IncludeDescendants) const;
  [[nodiscard]] bool containsAny(const std::vector<FeatureLabel> &labels,
                                 FeatureLabelMatchMode mode = FeatureLabelMatchMode::IncludeDescendants) const;
  [[nodiscard]] std::vector<FeatureLabel> labels() const;
  [[nodiscard]] std::uint64_t contractStamp() const;
  [[nodiscard]] float lineageAffinity(const FeatureLabelSet &other) const;

private:
  std::vector<FeatureLabel> labels_;
};

struct FeatureLabelQuery {
  std::vector<FeatureLabel> all;
  std::vector<FeatureLabel> any;
  std::vector<FeatureLabel> none;
  FeatureLabelMatchMode mode = FeatureLabelMatchMode::IncludeDescendants;

  [[nodiscard]] bool matches(const FeatureLabelSet &labels) const;
  [[nodiscard]] std::vector<std::string> explainMismatch(const FeatureLabelSet &labels) const;
};

struct FeatureLabelRecord {
  FeatureLabel label;
  std::string display_name;
  std::string description;
  float weight = 1.0f;
  std::uint64_t declaration_sequence = 0u;
};

class FeatureLabelCatalog {
public:
  bool declare(FeatureLabelRecord record);
  bool declare(std::string_view label, std::string display_name = {},
               std::string description = {}, float weight = 1.0f);
  bool addAlias(std::string_view alias, std::string_view target);

  [[nodiscard]] std::optional<FeatureLabel> resolve(std::string_view text) const;
  [[nodiscard]] const FeatureLabelRecord *find(const FeatureLabel &label) const;
  [[nodiscard]] bool contains(const FeatureLabel &label) const;
  [[nodiscard]] std::vector<FeatureLabelRecord> records() const;
  void clear();

private:
  std::vector<FeatureLabelRecord> records_;
  std::vector<std::pair<std::string, FeatureLabel>> aliases_;
  std::uint64_t next_declaration_sequence_ = 1u;
};

} // namespace aster
