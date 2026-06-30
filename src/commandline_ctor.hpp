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
{

  // If enabled check the following constraints:
  //
  // An option character is one of 0-9, A-Z, a-z, or '-'.
  // An option string is a nonempty string of option characters that does not start with '-'.
  // A short option character is one of A-Z or a-z.
  // The elements of 'flags' are option strings.
  // The elements of 'options' are option strings.
  // The keys in 'aliases' are short option characters.
  // The values in 'aliases' are either in 'flags' or 'options'.
  // 'flags' and 'options' are disjoint.
  // 'sentinels' has no duplicates.

  auto is_option_character = [&](char c)
  { return std::isalnum(static_cast<unsigned char>(c)) || c == '-'; };

  auto is_short_option_character = [&](char c)
  { return std::isalpha(static_cast<unsigned char>(c)); };

  auto is_option_string = [&](std::string_view s)
  { return !s.empty() && s[0] != '-' &&  std::ranges::all_of(s, is_option_character); };

  if constexpr (Debug) {

    auto is_valid_alias = [&](std::string_view s)
    { return flags.contains(s) || options.contains(s); };

    auto verify = [&](auto&& range, auto predicate, std::string_view label, std::string_view text ) {
      auto invalid = range | std::views::filter(std::not_fn(predicate));
      if (!std::ranges::empty(invalid)) {
        ++nerrors;
        std::println(stderr,
          "\033[31mcommandline: violation of constructor constraints\n"
          "\033[36m"
	  "  {}: {}\n"
          "  {}\n"
          "\033[0m",
          label, invalid, text); 
      }
    };

    // The elements of 'flags' are option strings.
    verify
    (
      flags,
      is_option_string,
      "invalid options(s)",
      "constraint: must be alphanumeric (0-9, A-Z, a-z) or internal '-'"
      "\n  should you remove leading hyphens in the first constructor argument?"
      "\n  this POSIX consistent constraint improves command line readability"
    );

    // The elements of 'options' are option strings.
    verify
    (
      options,
      is_option_string,
      "invalid options(s)",
      "constraint: must be alphanumeric (0-9, A-Z, a-z) or internal '-'"
      "\n  should you remove leading hyphens in the second constructor argument?"
      "\n  this POSIX consistent constraint improves command line readability"
    );

    // The keys in 'aliases' are short option characters.
    verify
    (
      aliases | std::views::keys,
      is_short_option_character,
      "invalid alias keys",
      "constraint: short options must be single characters from A-Z or a-z"
      "\n  is there a typo in the third constructor argument?"
      "\n  this POSIX consistent constraint improves command line readability"
    );

    // The values in 'aliases' are either in 'flags' or 'options'.
    verify
    (
      aliases | std::views::values,
      is_valid_alias, 
      "invalid alias",
      "constraint: a short option must alias to either a long flag or long option"
      "\n  is there a typo in the third constructor argument?"
      "\n  this POSIX consistent constaint improves command line readability"
    );

    // 'flags' and 'options' are disjoint.
    verify
    (
     flags | std::views::filter([&](auto s) { return options.contains(s); }), 
     [&](std::string_view s) {return false;}, 
     "common in flags and options",
     "constraint: the flags and options containers cannot have any strings in common"
     "\n  is there a typo in the first or second constructor arguments?"
     "\n  this constraint unambiguously defines if a short option is a flag"
    );

    // 'sentinels' has no duplicates.
    auto sorted_sentinels = sentinels;
    std::ranges::sort(sorted_sentinels);
    auto duplicate_sentinels =
        sorted_sentinels 
      | std::views::chunk_by(std::equal_to{})
      | std::views::filter([](auto group) { return std::ranges::distance(group) > 1; })
      | std::views::transform([](auto group) { return group[0]; });
    verify
    (
      duplicate_sentinels,
      [](auto){ return false; },
      "duplicate sentinels",
      "constraint: there cannot be duplicate sentinels"
      "\n  this constraint unambiguously defines the sentinel priorities"
    );

    if (nerrors) {
      std::println
	(
	 stderr,
	 "\033[31mcommandline: construction aborted after {} error(s)\033[0m",
	 nerrors
	 );
      return;
    }
  }

  if (argv_sv.size() == 0) return;

  // Avoids multiple O(log N) time lookups of short options for a neglible fixed space cost.

  std::pair<const std::string_view*, const std::string_view*> 
    flags_bounds { &*flags.begin(), &*flags.begin() + flags.size() };
  std::pair<const std::string_view*, const std::string_view*>
    options_bounds { &*options.begin(), &*options.begin() + options.size() };

  std::array<const std::string_view*, 256> atab{};
  for (const auto& [c, s] : aliases) {
    auto it = flags.find(s);
    if (it != flags.end()) atab[c] = &*it;
  }
  for (const auto& [c, s] : aliases) {
    auto it = options.find(s);
    if (it != options.end()) atab[c] = &*it;
  }
  auto is_flag = [&atab, &flags_bounds](char c) 
  { return atab[c] >= flags_bounds.first && atab[c] < flags_bounds.second; };
  auto is_option = [&atab, &options_bounds](char c) 
  { return atab[c] >= options_bounds.first && atab[c] < options_bounds.second; };

  // Partition into leading [argv_sv.begin(), it1), option [it1,it2), and trailing
  // [it2, argv_sv.end()) arguments.

  std::vector<std::string_view>::iterator it1;
  std::vector<std::string_view>::iterator it2;

  it1 = std::ranges::find_if(argv_sv, [](auto s) { return s != "-" && s.starts_with('-'); });
  it2 = argv_sv.end();
  for(auto& sn : sentinels) {
    auto it = std::find(it1, argv_sv.end(), sn);
    if (it != argv_sv.end()) { it2 = it; break; }
  }

  // Reserve an upper bound for parsed, accounting for the short flag groups.

  {
    std::size_t nparsed = 0;
    for (auto& sv : std::span(it1, it2)) {
      if (sv.empty() || sv[0] != '-') ++nparsed;         // empty or possible non-option
      else if (sv.size() > 1 && sv[1] == '-') ++nparsed; // possible option
      else nparsed += sv.size() - 1;                     // possible short group
    }
    parsed.reserve(nparsed);
  }

  // Scan the option arguments and record the syntactically arguments in 'parsed'.

  std::size_t nflags = 0;
  std::size_t noptns = 0;
  std::size_t n = std::distance(it1, it2);
  auto it = it1;
  while (it != it2) {

    // If the argument *it is empty, does not start with '-', or is exactly '-' the put it in
    // parsed if its a designated non-option, else skip it.

    if (it->empty() || (*it)[0] != '-' || it->size() == 1) {
      if (!non_options || non_options->contains(*it))
	{ parsed.emplace_back(std::string(), *it); *it = std::string_view(); --n; }
      ++it; goto next;
    }
    
    // *it starts with a '-' and is not exactly '-'.
    // If it starts with "--" then it's a long flag if there is no = and a long option otherwise.

    if ((*it)[1] == '-') {
      size_t eqpos = it->find('=', 2);
      if (eqpos == std::string_view::npos) {
	std::string_view opt = it->substr(2);
	if (flags.contains(opt)) 
	  { parsed.emplace_back(opt, std::nullopt); *it = std::string_view(); --n; ++nflags; }
      }
      else {
	std::string_view opt = it->substr(2, eqpos-2); 
	if (options.contains(opt))
	  { parsed.emplace_back(opt, it->substr(eqpos+1)); *it = std::string_view(); --n; ++noptns; }
      }
      ++it; goto next;
    }

    // *it starts with exactly one '-' and is not exactly '-', so a short option group.
    // The end of the group is the end of *it or it is the first parameterized option
    // until the last flag. Advance through the group and resolve the aliases.

    for (size_t i = 1; i < it->size(); ++i) {
      char c = (*it)[i];
      if (is_flag(c)) { parsed.emplace_back(*atab[c], std::nullopt); ++nflags; continue; }
      if (is_option(c)) {
	++i;
	++noptns;
	if (i < it->size() && (*it)[i] == '=') i++;
	if (i == it->size()) {
	  *it = std::string_view(); --n; ++it;
	  if (it == it2) { parsed.emplace_back(*atab[c], std::string_view()); }
	  else { parsed.emplace_back(*atab[c], *it); *it = std::string_view(); ++it; --n;}
	}
	else {
	  parsed.emplace_back(*atab[c], it->substr(i)); 
	  *it = std::string_view(); ++it; --n; 
	}
	goto next;
      }
      ++it; goto next;
    }
    *it = std::string_view(); --n; ++it;
  next:
  }

  if (n == 0) {

    std::vector<std::string_view> loader1;
    std::vector<std::pair<std::string_view,std::string_view>> loader2;

    parsed.shrink_to_fit();
    leading.assign(argv_sv.begin(), it1);
    trailing.assign(it2, argv_sv.end());

    loader1.reserve(nflags);
    loader2.reserve(noptns);
    for (const auto& [opt, prm] : parsed)
      if (!prm.has_value()) loader1.emplace_back(opt);
      else if (!opt.empty()) loader2.emplace_back(opt, *prm); 

    std::sort(loader1.begin(), loader1.end());
    auto last1 = std::unique(loader1.begin(), loader1.end());
    loader1.erase(last1, loader1.end());
    parsed_flags.replace(std::move(loader1));

    std::stable_sort(loader2.begin(), loader2.end(), 
      [](const auto& a, const auto& b) { return a.first < b.first; });
    parsed_options = std::flat_multimap<std::string_view, std::string_view, std::less<>>
      ( std::sorted_equivalent, loader2.begin(), loader2.end() );

    return;
  }

  for (auto it = it1; it < it2; it++) {

    auto parse_error= [&]( std::string_view text )
      {
        std::println(stderr,
          "\033[31m\ncommandline: parse error"
          "\033[36m"
          "\n  while parsing {}"
          "{}\033[0m",
          *it,
          text
        ); 
      };

    if (it->empty()) continue;

    if (it->starts_with("--")) {

      auto eqpos = it->find_first_of('=');
      auto s = it->substr(2, eqpos-2);

      if (s.empty()) {
        parse_error
        (
	  "\n  unrecognized: is there a misstyped space before an option string?"
        );
        continue;
      }
      
      if (!is_option_string(s)) {
       parse_error
        (
          "\n  unrecognized: " + std::string(s) + " is an invalid option string"
	  "\n  must be alphanumeric (0-9, A-Z, a-z) or internal '-'"
        + "\n  " + nears(options, s) 
        );
       continue;
      }

      if (eqpos != std::string_view::npos) {

	auto is_flag = flags.contains(s);
	if (is_flag) {
	  parse_error
	    (
	     "\n  unrecognized: " + std::string(s) + " is not a declared option"
	     "\n  '"+ s + "' is a declared flag ... "
             "\n    ... did you intend to declare it as an option? is the '=' misplaced?"
	     );
	  continue;
	}

	auto ns  = nears(options, s);
	parse_error
	  (
	   "\n  unrecognized: " + std::string(s) + " is not a declared option"
	   + (ns.empty() ? "" : "\n  " + nears(options, s))
	   );
	continue;

      }
      
      else {

	auto is_option = options.contains(s);
	if (is_option) {
	  parse_error
	    (
	     "\n  unrecognized: " + std::string(s) + " is not a declared flag"
	     "\n  '" + s + "' is a declared option ... "
             "\n    ... did you intend to declare it as an flag? is the '=' missing?"
	     );
	  continue;
	}

	auto ns  = nears(flags, s);
	parse_error
	  (
	   "\n  unrecognized: " + std::string(s) + " is not a declared flag"
	   + (ns.empty() ? "" : "\n  " + nears(options, s))
	   );
	continue;
      }
    }

    if (it->starts_with("-")) {
      auto eqpos = it->find_first_of('=');
      auto s = it->substr(1, eqpos-1);

      if (s.empty()) {
        parse_error
	  (
	   "\n  unrecognized: not a designated non-argument nor a sentinel"
	   "\n  is there a misstyped space before a flag string?"
	   );
        continue;
      }
      
      parse_error
        (
	 "\n  unrecognized: short option group '" + std::string(s) + "' has invalid flags"
	 "\n  a short option group must be a string of short option flags"
	 "\n    ... possibly ending in a (parameterized) short option"
	 );
      continue;
    }

    if (non_options || !(*non_options).contains(*it)) {

      if (flags.contains(*it)) {
	{
	  parse_error
	    (
	     "\n  unrecognized: " + std::string(*it) + " is not a designated non-option"
	     "\n  '" + *it + "' is a declared flag ... did you forget a '--'?"
	     );
	  continue;
	}
      }

      {
	auto pos = (*it).find('=');
	auto s = (pos != std::string_view::npos) ? (*it).substr(0, pos) : *it;
	if (options.contains(s)) {
	  parse_error
	    (
	     "\n  unrecognized: " + std::string(*it) + " is not a designated non-option"
	     "\n  '" + s + "' is a declared option ... did you forget a '--'?"
	     );
	  continue;
	}
      }
    
      parse_error
	(
	 "\n  unrecognized: " + std::string(*it) + " is not a designated non-option"
	 );
      continue;
    }

    parse_error (""); // we should never be here

  }

  nerrors = n;
  return;
}

template <bool Debug>
commandline<Debug>::commandline
(
 int argc,
 char** argv,
 const std::flat_set<std::string_view, std::less<>>& flags,
 const std::flat_set<std::string_view, std::less<>>& options,
 const std::flat_map<char, std::string_view>& aliases,
 const std::vector<std::string_view>& sentinels,
 const std::optional<std::flat_set<std::string_view>>& non_options
)
:
commandline
(
  [](int argc_, char** argv_)
    { return std::vector<std::string_view>(std::from_range, std::views::counted(argv_, argc_)); }
    (argc, argv),
  flags,
  options,
  aliases,
  sentinels,
  non_options
)
{}

template <bool Debug>
commandline<Debug>::commandline
(
  std::vector<std::string_view>&& argv_sv,
  const std::flat_set<std::string_view, std::less<>>& flags,
  const std::flat_set<std::string_view, std::less<>>& options,
  const std::flat_map<char, std::string_view>& aliases
)
:
commandline
(
  std::move(argv_sv),
  flags,
  options,
  aliases,
  {"--"},
  std::flat_set<std::string_view>{}
) 
{}

template <bool Debug>
commandline<Debug>::commandline
(
  std::vector<std::string_view>&& argv_sv,
  const std::flat_set<std::string_view, std::less<>>& flags,
  const std::flat_set<std::string_view, std::less<>>& options,
  const std::flat_map<char, std::string_view>& aliases,
  const std::vector<std::string_view>& sentinels
)
: 
commandline
(
  std::move(argv_sv),
  flags,
  options,
  aliases,
  sentinels,
  std::flat_set<std::string_view>{}
) 
{}

template <bool Debug>
commandline<Debug>::commandline
(
  int argc,
  char** argv,
  const std::flat_set<std::string_view, std::less<>>& flags,
  const std::flat_set<std::string_view, std::less<>>& options,
  const std::flat_map<char, std::string_view>& aliases
)
:
commandline
(
  argc,
  argv,
  flags,
  options,
  aliases,
  {"--"},
  std::flat_set<std::string_view>{}
) 
{}

template <bool Debug>
commandline<Debug>::commandline
(
  int argc,
  char** argv,
  const std::flat_set<std::string_view, std::less<>>& flags,
  const std::flat_set<std::string_view, std::less<>>& options,
  const std::flat_map<char, std::string_view>& aliases,
  const std::vector<std::string_view>& sentinels
)
: 
commandline
(
  argc,
  argv,
  flags,
  options,
  aliases,
  sentinels,
  std::flat_set<std::string_view>{}
) 
{}
