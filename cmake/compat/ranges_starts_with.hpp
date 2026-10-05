// Provides std::ranges::starts_with (C++23) for standard libraries that don't have it yet
// (e.g. libstdc++ 15). Force-included by CMake only when the library lacks it.

#pragma once

#if defined(__cplusplus) && __cplusplus >= 202002L

#include <algorithm>
#include <functional>
#include <iterator>
#include <ranges>

#if !defined(__cpp_lib_ranges_starts_ends_with)

namespace std::ranges {

struct __taiga_starts_with_fn {
  template <input_range R1, input_range R2, class Pred = ranges::equal_to,
            class Proj1 = identity, class Proj2 = identity>
  constexpr bool operator()(R1&& r1, R2&& r2, Pred pred = {}, Proj1 proj1 = {},
                            Proj2 proj2 = {}) const {
    auto it1 = ranges::begin(r1);
    const auto end1 = ranges::end(r1);
    auto it2 = ranges::begin(r2);
    const auto end2 = ranges::end(r2);
    for (; it2 != end2; ++it1, ++it2) {
      if (it1 == end1) return false;
      if (!std::invoke(pred, std::invoke(proj1, *it1), std::invoke(proj2, *it2))) return false;
    }
    return true;
  }
};

inline constexpr __taiga_starts_with_fn starts_with{};

}  // namespace std::ranges

#endif
#endif
