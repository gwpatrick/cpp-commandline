# commandline

For in-silico experiments, rapid exploration and archival reproducability are often achieved by
staging the simulations in scripts (such as Python, MATLAB, or Maple), with the final scripts
serving as a living, executable records. Pipelines need to be developed flexibly and modified
quickly, making custom command-line controls indispensable. This workflow—which also occurs in
fields such as data analysis, and in small private projects—is impeded by the legacy getopt
loop. Class `commandline` eliminates that loop entirely: parsing occurs at instantiation, and
option processing is through access to standard containers by natural idiomatic C++
code.

Any CLI can be expressed; minimal syntactic constraints are imposed. Follows modern CLI layout
practices, greatly simplifing and speeding their implementation, maintenance, and
modification. Includes modern conveniences, such as simple access of options via indexing, and a
`from_chars` driven conversion to arithmetic types. Helpful error reporting includes
Damerau-Levenshtein detection of typos. Optional validation for the CLI can be turned on for
development and off thereafter. A rigourous model (below) assists CLI conceptualization and
development.

Indications: Lightweight: requires only built-in facilities within the formal C++
standard. Usage is to include a header and link a single object file. Compared to `getopt`, a
preliminary benchmark indicates that runtimes that are competative at moderate numbers of
options (sub-milliseconds) and 50 times faster at 1000 options, with crossover at about 100. The
object file is about .5MB.

Counter indications: Specific to modern C++.

### 🛠 Prerequisites & Installation

* **Compiler**: Requires a C++26 compliant compiler (tested with GCC 16).
* **Build System**: `make`
* **License**: Distributed under the MIT License.

### Minimal example

```cpp
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

// $minimal_example --flag --option=42
// 'flag' is detected
// 'option' has value 42
// 'option' values are ["42"]

// $minimal_example -o42 -o 24 -o=1 -f
// 'flag' is detected
// 'option' does not have a value
// 'option' values are ["42", "24", "1"]
```

## Model

`program [leading arguments] [option arguments] [sentinel] [trailing arguments]`

A leading argument, which does not start with `'-'`, is such as a subcommand; an option
argument, the first of which must start with `'-'`, is a program control; a sentinel marks
the start of the trailing arguments, which are unrestricted program data.  Option arguments such
as `--flag` and `--option=parameter` are analogous to the long arguments of
getopt_long. `parameter` is unrestricted; consequently, `--option=` has parameter the empty
string.  There can be no space before the `=`.  Options such as `-a`, `-a=parameter` and `-a
parameter` are analogous to the short arguments of getopt_long. Short options may be grouped,
such as `-abc`; the last in the group may be parameterized. 

Processing is by a single class constructed from `argc` and `argv`. All data and controls are
aggregated into (public) standard containers at construction. In the containers, the command
line order is lost, although duplicated parameterized options accumulate their parameters in
order. Access to the parsed options is by `operator[]` indexed the option. A standard container
of a 1-1 order-preserving map of `option arguments` enables arbitrary CLI positional design.

A single `'-'` may occur as a parameter (the only POSIX consistent interpretation is `stdin` or
`stdout` in the context where a parameter is a file name). A single `'-'` may also occur as a
leading or trailing argument.  Non-option arguments can be selectively aggregated or declared as
errors.

Short options may be glued, as `-f42`, or they may be as `-f=42`, or separated as `-f
42`. Passing an parameter starting with `'='` must be `-f==42` or `-f =42`. An empty string may
be passed as `-f`, or `-f=`, or `-f ""`, a design which favours passing an empty string over
passing a string starting with `'='` (the former use case is more probable, and without this
interpretation of `-f=param`, an empty string could only be passed as `-f ""`). The separated
form absorbs the next argument e.g. `-f --param` is as `-f=--param`.

Non-option arguments may be designated as errors, or they may be selectively (or all) valid (an
empty argument is treated as any other). Processing of non-option arguments is by scan of the
1-1 map.

## Implementation

```cpp
template <bool Debug>
commandline<Debug>::commandline
(
 std::vector<std::string_view>&&                       argv_sv,
 const std::flat_set<std::string_view, std::less<>>&   flags,
 const std::flat_set<std::string_view, std::less<>>&   options,
 const std::flat_map<char, std::string_view>&          aliases,
 const std::vector<std::string_view>&                  sentinels,
 const std::optional<std::flat_set<std::string_view>>& non_options
)
```
* In normal usage the container `argv_sv` is a `string_view` copy of the command
  line `argv`.
* An <dfn>option character</dfn> is one of `0`-`9`, `A`-`Z`, `a`-`z`, or `'-'`. An <dfn>option
  string</dfn> is a nonempty string of option characters that does not start with `'-'`.
* A <dfn>short option character</dfn> is one of `A`-`Z` or `a`-`z`.
* A <dfn>sentinel string</dfn> is any string. The <dfn>priority</dfn> of a sentinel is
  its index in `sentinels`.
* The elements of the container `flags` and `options` are option strings.
* The containers `flags` and `options` are disjoint.
* The elements of `non_options` are unrestricted.
* The keys in the container `aliases` are short option characters. The values are either
  flags or parameterized options.
* The elements of `sentinels` are unrestricted. The container `sentinels` has no duplicates.
* Any of the containers `flags`, `options`, `aliases`, `sentinels`, or `non_options` may be
  empty.

Overloads: An overload is provided with the usual `argc` and `argc` instead of `argv_sv`.
`sentinels` defaults to a container with the single element `"--"`. `non_options` defaults to
the singleton container of the empty string (the default is that non-option arguments are
errors).

_Definitions:_

* An <dfn>command line argument</dfn>, or <dfn>argument</dfn>, is a string in `argv_sv`.
* The <dfn>program string</dfn> is the first argument, if `argv_sv` is nonempty.
* A <dfn>flag</dfn>, <dfn>option</dfn>, <dfn>sentinel</dfn> is an element of `flags`, `options`,
  `sentinels`, respectively. For emphasis, elements of `options` may be referred to as
  <dfn>parameterized options</dfn>.
* An <dfn>long option</dfn> is either a flag or an option.
* A <dfn>short option</dfn> is a key in `aliases`; its <dfn>alias</dfn> is the
  corresponding value, which is an flag or option. A short option is a flag or is parameterized
  if its alias is, respectively.
* Depending on context, the term <dfn>option</dfn> may refer to any of flag, short option, long
  option, parameterized or not.
* A <dfn>designated non-option</dfn> a string in the container of `non_options` (there may be
  none), or it is any non-option string if there is no container.
* A <dfn>short option group</dfn> is a string of one or more short options, where the last
  may be parameterized, in which case the group is <dfn>parameterized</dfn>.
* A <dfn>parameter string</dfn> is any string; it could be empty.

The following definitions refer to the context `[option arguments]`:

* A <dfn>flag argument</dfn> is a string `"-"` followed by a flag.
* A <dfn>parameterized option argument</dfn> is a string `"--"`, followed by an
  option, followed by the character `=`.
* A <dfn>non-parameterized short group argument</dfn> is a string `"-"` followed by a
  non-parameterized short group.
* A <dfn>parameterized short group argument</dfn> is a string `"-"` followed by a parameterized
  short group, and then one of `=` or a parameter string (with no intervening space).
* A <dfn>option argument</dfn> is one of the above and a <dfn>non-option argument</dfn> is any
  argument not starting with the character '-'. An argument is <dfn>syntactic</dfn> if it is one
  of these two, and otherwise it is <dfn>invalid</dfn>.

_Processing:_

If `argv_sv` is empty then there is no command line and the constructor immediately returns
without error.

The beginning of `std::span<std::string_view> middle` is the first string in `argv_sv` starting
with `'-'`. The end of `middle` is determined by a search for the sentinels in priority
order. The constructor does a left-to-right pass of `middle`, moving the syntactic and
designated non-option arguments to the public container `parsed`, which will contain only flags,
options, parameters, and designated non-option arguments. If `middle` is emptied then `parsed`
is a 1-1 syntactically correct image of `[option arguments]`; otherwise the parser is considered
to be in an error state and what remains in `middle` drives the diagnostics.

Public container `std::vector<std::string_view> leading` is populated from the nonempty
`argv_sv` up to the beginning of `middle`. If a sentinel was found, public container
`std::vector<std::string_view> trailing` is populated from the sentinel to the end of `argv_sv`.
Public container `std::flat_set<std::string_view> parsed_flags` is populated from container
`parsed` (the command line order is lost and dupilcates are removed). Public container
`std::flat_multimap<std::string_view> parsed_options` is populated from container `parsed` (the
command line order of the distict options is lost, but the order of duplicates is
preserved). Access through `parsed_flags` `parsed_options` these containers with `operator[]` is
the normal design usaged. Any processing of non-option arguments must be by interrogation of
`parsed`.

The constructor complexity is O(N log N), and option lookups are log(N).
