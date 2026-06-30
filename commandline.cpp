#include "commandline.hpp"

template struct commandline<true>;
template struct commandline<false>;

#define INSTANTIATE_NUMERIC(Type) \
  template std::expected<Type, std::errc> commandline<true>::to_num<Type>(std::string_view) const; \
  template std::expected<Type, std::errc> commandline<false>::to_num<Type>(std::string_view) const; \
  template std::vector<std::expected<Type, std::errc>> commandline<true>::to_nums<Type>(std::string_view, char) const; \
  template std::vector<std::expected<Type, std::errc>> commandline<false>::to_nums<Type>(std::string_view, char) const;

INSTANTIATE_NUMERIC(int)
INSTANTIATE_NUMERIC(long)
INSTANTIATE_NUMERIC(long long)
INSTANTIATE_NUMERIC(unsigned int)
INSTANTIATE_NUMERIC(unsigned long)
INSTANTIATE_NUMERIC(unsigned long long)
INSTANTIATE_NUMERIC(float)
INSTANTIATE_NUMERIC(double)
