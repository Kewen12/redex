/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <set>
#include <vector>

#include <boost/optional.hpp>

#include "ClassHierarchy.h"
#include "CrossDexRefMinimizer.h"
#include "NormalizeConstructor.h"
#include "Trace.h"

class DexType;

/**
 * We can have multiple merging strategies for classes that have the same shape
 * and same interdex-group.
 */

namespace class_merging {
namespace strategy {

enum Strategy {
  // Starts a new group when a configurable number of merged classes is exceeded
  BY_CLASS_COUNT = 0,

  // Starts a new group when merged (virtual) methods become large
  BY_CODE_SIZE = 1,

  // Aggregates classes by shared references, and starts a new group when the
  // combined number of references becomes large, or when merged (virtual)
  // methods become large
  BY_REFS = 2,
};

template <typename WalkerFn>
void group_by_cls_count(const TypeSet& mergeable_types,
                        size_t min_mergeables_count,
                        const boost::optional<size_t>& opt_max_mergeables_count,
                        WalkerFn walker) {
  size_t max_mergeables_count = opt_max_mergeables_count
                                    ? *opt_max_mergeables_count
                                    : std::numeric_limits<size_t>::max();
  redex_assert(min_mergeables_count <= max_mergeables_count &&
               min_mergeables_count >= 2);

  size_t remaining_count = mergeable_types.size();

  auto it = mergeable_types.begin();
  for (; remaining_count >= max_mergeables_count;
       remaining_count -= max_mergeables_count) {
    auto next = std::next(it, max_mergeables_count);
    std::vector<const DexType*> curr_group(it, next);
    walker(curr_group);
    it = next;
  }
  if (remaining_count >= min_mergeables_count) {
    std::vector<const DexType*> curr_group(it, mergeable_types.end());
    walker(curr_group);
  }
}

size_t estimate_vmethods_code_size(const DexClass* cls);

/**
 * Note it does only check the virtual methods code size on the classes and it
 * is not aware of how later optimizations would change the code.
 */
template <typename WalkerFn>
void group_by_code_size(const TypeSet& mergeable_types, WalkerFn walker) {
  constexpr size_t max_instruction_size = 1 << 15;

  std::vector<const DexType*> current_group;

  size_t estimated_merged_code_size = 0;
  for (auto type : mergeable_types) {
    // Only check the code size of vmethods because these vmethods will be
    // merged into a large dispatch, dmethods will be relocated.
    auto vmethod_code_size = estimate_vmethods_code_size(type_class(type));
    if (vmethod_code_size > max_instruction_size) {
      // This class will never make it into any group; skip it
      continue;
    }
    if (estimated_merged_code_size + vmethod_code_size > max_instruction_size) {
      TRACE(CLMG, 9, "\tgroup_by_code_size %zu classes", current_group.size());
      if (current_group.size() > 1) {
        walker(current_group);
      }
      current_group.clear();
      estimated_merged_code_size = 0;
    }
    current_group.push_back(type);
    estimated_merged_code_size += vmethod_code_size;
  }
  if (current_group.size() > 1) {
    TRACE(CLMG, 9, "\tgroup_by_code_size %zu classes at the end",
          current_group.size());
    walker(current_group);
  }
}

template <typename WalkerFn>
void group_by_refs(const TypeSet& mergeable_types, WalkerFn walker) {
  constexpr size_t max_instruction_size = 1 << 15;
  // TODO: Consider making this configurable. It represents the maximum number
  // of non-trivial references (fields, methods, etc.) a group can have before
  // being closed.
  constexpr size_t max_applied_refs = 75;

  std::vector<const DexType*> current_group;

  cross_dex_ref_minimizer::CrossDexRefMinimizer cross_dex_ref_minimizer({});
  for (auto type : mergeable_types) {
    cross_dex_ref_minimizer.sample(type_class(type));
  }
  for (auto type : mergeable_types) {
    cross_dex_ref_minimizer.insert(type_class(type));
  }
  size_t estimated_merged_code_size = 0;
  while (!cross_dex_ref_minimizer.empty()) {
    auto cls = current_group.empty() ? cross_dex_ref_minimizer.worst()
                                     : cross_dex_ref_minimizer.front();
    // Only check the code size of vmethods because these vmethods will be
    // merged into a large dispatch, dmethods will be relocated.
    auto vmethod_code_size = estimate_vmethods_code_size(cls);
    if (vmethod_code_size > max_instruction_size) {
      // This class will never make it into any group; skip it
      cross_dex_ref_minimizer.erase(cls, /* emitted */ false,
                                    /* reset */ false);
      continue;
    }
    bool reset = false;
    if (estimated_merged_code_size + vmethod_code_size > max_instruction_size ||
        cross_dex_ref_minimizer.get_applied_refs() > max_applied_refs) {
      TRACE(CLMG, 9, "\tgroup_by_refs %zu classes", current_group.size());
      if (current_group.size() > 1) {
        walker(current_group);
      }
      current_group.clear();
      estimated_merged_code_size = 0;
      reset = true;
    }
    current_group.push_back(cls->get_type());
    estimated_merged_code_size += vmethod_code_size;
    cross_dex_ref_minimizer.erase(cls, /* emitted */ true, reset);
  }
  if (current_group.size() > 1) {
    TRACE(CLMG, 9, "\tgroup_by_refs %zu classes at the end",
          current_group.size());
    walker(current_group);
  }
}

template <typename WalkerFn>
void apply_grouping(const Strategy strategy,
                    const TypeSet& mergeable_types,
                    size_t min_mergeables_count,
                    const boost::optional<size_t>& max_mergeables_count,
                    WalkerFn walker) {
  switch (strategy) {
  case BY_CLASS_COUNT:
    group_by_cls_count(mergeable_types, min_mergeables_count,
                       max_mergeables_count, walker);
    break;
  case BY_CODE_SIZE:
    group_by_code_size(mergeable_types, walker);
    break;
  case BY_REFS:
    group_by_refs(mergeable_types, walker);
    break;
  default:
    not_reached();
  }
}

} // namespace strategy
} // namespace class_merging
