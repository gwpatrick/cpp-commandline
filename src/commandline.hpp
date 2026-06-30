#pragma once

#include <algorithm>
#include <cstring>
#include <expected>
#include <flat_map>
#include <flat_set>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

template <bool Debug = false>
struct commandline {

  explicit commandline
  (
    std::vector<std::string_view>&&,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_map<char, std::string_view>&,
    const std::vector<std::string_view>&,
    const std::optional<std::flat_set<std::string_view>>&
  );

  explicit commandline
  ( int argc,
    char**,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_map<char, std::string_view>&,
    const std::vector<std::string_view>&,
    const std::optional<std::flat_set<std::string_view>>&
  );

  std::vector<std::string_view> leading;
  std::vector<std::string_view> trailing;
  std::vector<std::pair<std::string_view,std::optional<std::string_view>>> parsed;
  std::flat_set<std::string_view, std::less<>> parsed_flags;
  std::flat_multimap<std::string_view, std::string_view, std::less<>> parsed_options;

  size_t nerrors = 0;

  struct flag_t { explicit flag_t() = default; }; // tag dispatching
  static inline constexpr flag_t flag{};

  [[nodiscard]]
  bool operator[](std::string_view f, flag_t) const noexcept
  { return parsed_flags.contains(f); }
  
  struct unique_option_t { explicit unique_option_t() = default; };
  static inline constexpr unique_option_t unique_option{};

  [[nodiscard]]
  std::optional<std::string_view>
  operator[](std::string_view opt, unique_option_t) const noexcept 
  {
    auto [beg, end] = parsed_options.equal_range(opt);
    if (beg == end || std::next(beg) != end) return std::nullopt;
    return beg->second;
  }

  struct option_t { explicit option_t() = default; };
  static inline constexpr option_t option{};

  [[nodiscard]]
  std::span<const std::string_view>
  operator[](std::string_view opt, option_t) const noexcept 
  {
    auto [beg, end] = parsed_options.equal_range(opt);
    if (beg == end) return {};
    auto offset_beg = static_cast<size_t>(std::distance(parsed_options.begin(), beg));
    auto offset_end = static_cast<size_t>(std::distance(parsed_options.begin(), end));
    const auto& vals = parsed_options.values();
    return std::span<const std::string_view>(&vals[offset_beg], offset_end - offset_beg);
  }

  std::string nears(std::ranges::input_range auto&&, std::string_view);

  template <typename T>
  requires std::is_arithmetic_v<T>
  std::expected<T, std::errc> to_num(std::string_view) const;

  template <typename T>
  requires std::is_arithmetic_v<T>
  std::vector<std::expected<T, std::errc>> to_nums(std::string_view, char) const;

  explicit commandline
  (
    std::vector<std::string_view>&&,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_map<char, std::string_view>&
  );

  explicit commandline
  (
    std::vector<std::string_view>&&,
    const std::flat_set<std::string_view, std::less<>> &,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_map<char, std::string_view>&,
    const std::vector<std::string_view>&
  );

  explicit commandline
  (
    int,
    char**,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_map<char, std::string_view>&
  );

  explicit commandline
  (
    int,
    char**,
    const std::flat_set<std::string_view, std::less<>> &,
    const std::flat_set<std::string_view, std::less<>>&,
    const std::flat_map<char, std::string_view>&,
    const std::vector<std::string_view>&
  );

};

#include "commandline_ctor.hpp"
#include "commandline_lib.hpp"

extern template struct commandline<true>;
extern template struct commandline<false>;
