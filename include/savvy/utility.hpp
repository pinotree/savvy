#ifndef LIBSAVVY_UTILITY_HPP
#define LIBSAVVY_UTILITY_HPP

#include <string>
#include <functional>
#include <memory>

namespace savvy
{
//  template<typename T, typename... Args>
//  std::unique_ptr<T> make_unique(Args&&... args)
//  {
//    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
//  }

  static const std::string version = SAVVY_VERSION;

  std::string parse_header_id(std::string header_value);

  namespace detail
  {
    template<typename F, typename Tuple, std::size_t... S>
    decltype(auto) apply_impl(F&& fn, Tuple&& t, std::index_sequence<S...>)
    {
      return std::forward<F>(fn)(std::get<S>(std::forward<Tuple>(t))...);
    }

    template<typename F, typename Tuple>
    decltype(auto) apply(F&& fn, Tuple&& t)
    {
      std::size_t constexpr tuple_size
        = std::tuple_size<typename std::remove_reference<Tuple>::type>::value;
      return apply_impl(std::forward<F>(fn),
        std::forward<Tuple>(t),
        std::make_index_sequence<tuple_size>());
    }
  }
}

#endif // LIBSAVVY_UTILITY_HPP