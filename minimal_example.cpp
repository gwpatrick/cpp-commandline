#include <optional>
#include <print>
#include <span>

#include <commandline.hpp>

int main(int argc, char *argv[]) {

  commandline<true> CL(argc, argv, {"flag"}, {"option"}, {{'f',"flag"},{'o',"option"}});
  if (CL.nerrors) return 1;

  [[maybe_unused]] constexpr auto f  = commandline<true>::flag;
  [[maybe_unused]] constexpr auto uo = commandline<true>::unique_option;
  [[maybe_unused]] constexpr auto o  = commandline<true>::option;

  if (CL["flag", f]) std::println("'flag' is detected");
  else std::println("'flag' is not detected");

  std::optional<std::string_view> s = CL["option", uo];
  if (s) std::println("'option' has value {}", *s);
  else std::println("'option' does not have a value");

  std::span<const std::string_view> ss = CL["option", o];
  if (!ss.empty()) std::println("'option' values are {}", ss);
  else std::println("'option' does not have values");

}

// minimal_example --flag --option=42
// 'flag' is detected
// 'option' has value 42
// 'option' values are ["42"]

// minimal_example -o42 -o 24 -o=1 -f
// 'flag' is detected
// 'option' does not have a value
// 'option' values are ["42", "24", "1"]

