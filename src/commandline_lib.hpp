// commandline_lib.inl
// Library for manipulating option parameters eg conversion to numeric.

// to_num: converts a string_view "s" to an arithmetic type (floating points, signed integers,
// unsigned integers). "s" can be as decimal or hexadecimal floating point as in
// std::from_chars, with extensions as follows: both '+', "0x", and "0X" are
// recognized. Surrounding white space is allowed. Returns the value as std::expected eg
// conversion to bool is true if there was no error and the value may be obtained by operator *.
// to_nums splits a string on a delimiter and returns a vector obtained by applying to_num to
// each substring.
//
// std::from_chars does not allow "+" and it handles hexidecimals by function call parameters.
// to_num steps over '+' and '-' and the hex prefixes, calls std::from_chars to obtain the
// absolute value, and then replaces the sign. In the case of integers, returns
// std::errc::result_out_of_range if the cast to signed results in an overflow.

// 2026-06-07 GWP

template <bool D>
template <typename T>
requires std::is_arithmetic_v<T>
std::expected<T, std::errc> commandline<D>::to_num(std::string_view s) const {
  T x;
  std::from_chars_result res;
  bool negative = false;

  {
    auto i = s.find_first_not_of(" \t\n\r");
    if (i == std::string_view::npos) return std::unexpected(std::errc::invalid_argument);
    auto j = s.find_last_not_of(" \t\n\r");
    s = s.substr(i, j - i + 1);
  }

  if (s[0] == '-') {
    if constexpr (std::is_unsigned_v<T>) [[unlikely]] 
      return std::unexpected(std::errc::invalid_argument);
    negative = true;
    s.remove_prefix(1); 
  } 
  else if (s[0] == '+') s.remove_prefix(1);

  bool hex = false;
  if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    hex = true;
    s.remove_prefix(2);
    if (s.empty()) [[unlikely]] return std::unexpected(std::errc::invalid_argument);
  }

  if constexpr (std::is_floating_point_v<T>) {
    auto fmt =  hex ? std::chars_format::hex : std::chars_format::general;
    res = std::from_chars(s.data(), s.data() + s.size(), x, fmt);
    if (res.ec != std::errc{}) [[unlikely]] return std::unexpected(res.ec);
    if (negative) x = -x;
  }

  else {
    using UT = std::make_unsigned_t<T>;
    UT ux;
    res = std::from_chars(s.data(), s.data() + s.size(), ux, hex ? 16 : 10);
    if (res.ec != std::errc{}) [[unlikely]] return std::unexpected(res.ec);
    if constexpr (std::is_unsigned_v<T>) x = ux;
    else {
      if (negative) {
	constexpr UT max_neg = static_cast<UT>(std::numeric_limits<T>::max()) + 1;
	if (ux > max_neg) return std::unexpected(std::errc::result_out_of_range);
	x = (ux == max_neg) ? std::numeric_limits<T>::min() : -static_cast<T>(ux);
      }
      else {
	if (ux > static_cast<UT>(std::numeric_limits<T>::max()))
	  return std::unexpected(std::errc::result_out_of_range);
	x =  static_cast<T>(ux);
      }
    }
  }
  if (res.ptr != s.data() + s.size()) [[unlikely]] 
    return std::unexpected(std::errc::invalid_argument);
  return x;
}

template <bool D>
template <typename T>
requires std::is_arithmetic_v<T>
std::vector<std::expected<T, std::errc>> commandline<D>::to_nums(std::string_view sv, char delim) const {
  return sv
    | std::views::split(delim)
    | std::views::transform([this](auto&& r) { 
        return to_num<T>(std::string_view(&*r.begin(), std::ranges::distance(r))); 
      })
    | std::ranges::to<std::vector<std::expected<T, std::errc>>>();
}

template <bool D>
std::string commandline<D>::nears(std::ranges::input_range auto&& targets, std::string_view s)
{

  auto distance = [](std::string_view s1, std::string_view s2) { // Damerau-Levenshtein distance.
    int n = s1.length();
    int m = s2.length();
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i) {
      for (int j = 1; j <= m; ++j) {
	int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
	dp[i][j] = std::min({dp[i-1][j]+1, dp[i][j-1]+1, dp[i-1][j-1]+cost});
	if (i>1 && j>1 && s1[i-1] == s2[j-2] && s1[i-2] == s2[j-1]) {
	  dp[i][j] = std::min(dp[i][j], dp[i-2][j-2]+1);
	}
      }
    }
    return dp[n][m];
  };

  std::string possibles;
  for (const auto& target : targets) {
    int d = distance(target,s);
    int threshold = (target.length() <= 4) ? 1 : 2;
    if (d > 0 && d <= threshold) possibles += " " + std::string(target) + "?";
  }
  if (!possibles.empty()) possibles = std::string("did you mean") + possibles;
  return possibles;
}
