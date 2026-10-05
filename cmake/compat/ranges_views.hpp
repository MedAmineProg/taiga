// Provides std::views::adjacent and std::views::enumerate (C++23) for standard libraries that
// don't have them yet (e.g. libc++ up to at least 21). Force-included by CMake only when the
// library lacks them. Only random-access, sized ranges are supported, which is all that the
// bundled dependencies need.

#pragma once

#if defined(__cplusplus) && __cplusplus >= 202002L

#include <cstddef>
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>
#include <version>

namespace std::ranges {

template <class V>
concept __taiga_window_range = view<V> && random_access_range<V> && sized_range<V>;

#if !defined(__cpp_lib_ranges_zip)

template <class T, size_t N, class... Ts>
struct __taiga_repeat_tuple : __taiga_repeat_tuple<T, N - 1, T, Ts...> {};
template <class T, class... Ts>
struct __taiga_repeat_tuple<T, 0, Ts...> {
  using type = std::tuple<Ts...>;
};
template <class T, size_t N>
using __taiga_repeat_tuple_t = typename __taiga_repeat_tuple<T, N>::type;

template <__taiga_window_range V, size_t N>
  requires(N > 0)
class __taiga_adjacent_view : public view_interface<__taiga_adjacent_view<V, N>> {
public:
  class iterator {
  public:
    using base_iterator = iterator_t<V>;
    using difference_type = range_difference_t<V>;
    using value_type = __taiga_repeat_tuple_t<range_reference_t<V>, N>;
    using iterator_concept = forward_iterator_tag;
    using iterator_category = forward_iterator_tag;

    iterator() = default;
    constexpr explicit iterator(base_iterator it) : it_(std::move(it)) {}

    constexpr value_type operator*() const {
      return [this]<size_t... I>(index_sequence<I...>) {
        return value_type{it_[static_cast<difference_type>(I)]...};
      }(make_index_sequence<N>{});
    }

    constexpr iterator& operator++() {
      ++it_;
      return *this;
    }
    constexpr iterator operator++(int) {
      auto copy = *this;
      ++it_;
      return copy;
    }

    friend constexpr bool operator==(const iterator&, const iterator&) = default;

  private:
    base_iterator it_{};
  };

  __taiga_adjacent_view() = default;
  constexpr explicit __taiga_adjacent_view(V base) : base_(std::move(base)) {}

  constexpr iterator begin() {
    return iterator{ranges::begin(base_)};
  }
  constexpr iterator end() {
    return iterator{ranges::begin(base_) + size_()};
  }
  constexpr auto size() {
    return static_cast<range_size_t<V>>(size_());
  }

private:
  constexpr range_difference_t<V> size_() {
    const auto n = static_cast<range_difference_t<V>>(ranges::size(base_));
    const auto m = static_cast<range_difference_t<V>>(N - 1);
    return n > m ? n - m : 0;
  }

  V base_{};
};

template <size_t N>
struct __taiga_adjacent_fn {
  template <viewable_range R>
    requires __taiga_window_range<views::all_t<R>>
  constexpr auto operator()(R&& r) const {
    return __taiga_adjacent_view<views::all_t<R>, N>{views::all(std::forward<R>(r))};
  }

  template <viewable_range R>
    requires __taiga_window_range<views::all_t<R>>
  friend constexpr auto operator|(R&& r, const __taiga_adjacent_fn& fn) {
    return fn(std::forward<R>(r));
  }
};

namespace views {
template <size_t N>
inline constexpr __taiga_adjacent_fn<N> adjacent{};
}  // namespace views

#endif  // !__cpp_lib_ranges_zip

#if !defined(__cpp_lib_ranges_enumerate)

template <__taiga_window_range V>
class __taiga_enumerate_view : public view_interface<__taiga_enumerate_view<V>> {
public:
  class iterator {
  public:
    using base_iterator = iterator_t<V>;
    using difference_type = range_difference_t<V>;
    using value_type = std::tuple<difference_type, range_reference_t<V>>;
    using iterator_concept = forward_iterator_tag;
    using iterator_category = forward_iterator_tag;

    iterator() = default;
    constexpr iterator(base_iterator it, difference_type index)
        : it_(std::move(it)), index_(index) {}

    constexpr value_type operator*() const {
      return value_type{index_, *it_};
    }

    constexpr iterator& operator++() {
      ++it_;
      ++index_;
      return *this;
    }
    constexpr iterator operator++(int) {
      auto copy = *this;
      ++*this;
      return copy;
    }

    friend constexpr bool operator==(const iterator& a, const iterator& b) {
      return a.it_ == b.it_;
    }

  private:
    base_iterator it_{};
    difference_type index_ = 0;
  };

  __taiga_enumerate_view() = default;
  constexpr explicit __taiga_enumerate_view(V base) : base_(std::move(base)) {}

  constexpr iterator begin() {
    return iterator{ranges::begin(base_), 0};
  }
  constexpr iterator end() {
    const auto n = static_cast<range_difference_t<V>>(ranges::size(base_));
    return iterator{ranges::begin(base_) + n, n};
  }
  constexpr auto size() {
    return ranges::size(base_);
  }

private:
  V base_{};
};

struct __taiga_enumerate_fn {
  template <viewable_range R>
    requires __taiga_window_range<views::all_t<R>>
  constexpr auto operator()(R&& r) const {
    return __taiga_enumerate_view<views::all_t<R>>{views::all(std::forward<R>(r))};
  }

  template <viewable_range R>
    requires __taiga_window_range<views::all_t<R>>
  friend constexpr auto operator|(R&& r, const __taiga_enumerate_fn& fn) {
    return fn(std::forward<R>(r));
  }
};

namespace views {
inline constexpr __taiga_enumerate_fn enumerate{};
}  // namespace views

#endif  // !__cpp_lib_ranges_enumerate

}  // namespace std::ranges

#endif
