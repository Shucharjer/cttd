#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <meta>
#include <string_view>
#include <type_traits>
#include <utility>

namespace cttd {

namespace detail {

template <typename Base>
struct invokers; // for no macro define derived type list

template <typename... Args> struct type_list; // NOTE: incomplete type is enough

constexpr std::string_view type_list_name = "type_list";

constexpr std::uint32_t npos = static_cast<std::uint32_t>(-1);

} // namespace detail

template <typename Base> struct enable_dispatch {
  std::uint32_t index = 0;
};

// define a special "type list" for vtable search
template <typename Base, typename... Derived> consteval void define_invokers() {
  using namespace std::meta;

  using plist_t = detail::type_list<Derived...> *;
  data_member_options options{.name = detail::type_list_name};
  auto type_list = data_member_spec(^^plist_t, options);
  auto members = {type_list};
  define_aggregate(^^detail::invokers<Base>, std::move(members));
}

// has public identifier
template <typename T>
concept eligible_for_dispatch = std::derived_from<T, enable_dispatch<T>>;

namespace detail {

template <typename Invokers>
[[nodiscard]] consteval std::meta::info type_list_of() {
  using namespace std::meta;
  auto context = access_context::unchecked();
  for (auto member : nonstatic_data_members_of(^^Invokers, context)) {
    if (has_identifier(member) &&
        identifier_of(member) == detail::type_list_name) {
      return remove_pointer(type_of(member));
    }
  }
  return ^^void;
}

template <typename T, typename Invokers> struct position_of {
  static constexpr std::uint32_t value = [] consteval -> std::uint32_t {
    using namespace std::meta;
    auto type_list = type_list_of<Invokers>();
    if (is_same_type(^^void, type_list)) {
      return npos;
    }

    auto types = template_arguments_of(type_list);
    std::uint32_t pos = 0;
    for (auto inf : types) {
      if (is_same_type(remove_pointer(inf), ^^T)) {
        return pos;
      }
      ++pos;
    }

    return npos;
  }();
};

} // namespace detail

// set index
template <typename Base, typename Derived>
  requires eligible_for_dispatch<Base> && std::derived_from<Derived, Base>
constexpr void set_invoker(Derived *self) noexcept {
  using namespace detail;
  constexpr auto pos = position_of<Derived, invokers<Base>>::value;
  static_assert(pos != npos, "Derived is not present in invokers<Base>.");
  static_cast<enable_dispatch<Base> *>(self)->index = pos + 1;
}

namespace detail {

using namespace std::meta;

template <typename Base> consteval auto invoker_list_of() {
  auto list = type_list_of<invokers<Base>>();
  return std::define_static_array(template_arguments_of(list));
}

template <typename Base> consteval std::uint32_t invoker_count() {
  auto list = type_list_of<invokers<Base>>();
  return static_cast<std::uint32_t>(template_arguments_of(list).size());
}

template <info Current> consteval info named_function_of() {
  if constexpr (has_identifier(Current)) {
    return Current;
  } else if constexpr (has_template_arguments(Current) &&
                       has_identifier(template_of(Current))) {
    return template_of(Current);
  } else {
    return ^^void;
  }
}

template <info FnInfo> using return_type_of_t = [:return_type_of(FnInfo):];

template <info Member, typename Derived, typename Ret, typename... Args>
concept reflected_member_invocable = requires(Derived *self, Args... args) {
  { self->[:Member:](std::forward<Args>(args)...) } -> std::same_as<Ret>;
};

template <info CurrFn, typename Derived, typename Ret, typename... Args>
consteval info find_matching_member() {
  constexpr auto named_current = named_function_of<CurrFn>();
  static_assert(named_current != ^^void,
                "Dispatch call site must have an identifiable function name.");
  constexpr auto current_name = identifier_of(named_current);
  static constexpr auto members = std::define_static_array(
      members_of(^^Derived, access_context::unchecked()));

  template for (constexpr auto member : members) {
    if constexpr (has_identifier(member)) {
      if constexpr (identifier_of(member) == current_name) {
        if constexpr ((is_function(member) || is_function_template(member)) &&
                      reflected_member_invocable<member, Derived, Ret,
                                                 Args...>) {
          return member;
        }
      }
    }
  }

  return ^^void;
}

template <info CurrFn, typename Base, typename Derived, typename... Args>
consteval auto make_dispatch_stub() {
  using return_type = return_type_of_t<CurrFn>;

  return +[](Base *self, Args... args) -> return_type {
    constexpr auto member =
        find_matching_member<CurrFn, Derived, return_type, Args...>();
    static_assert(!is_type(member),
                  "Did not find a matching derived dispatch target.");

    return static_cast<Derived *>(self)->[:member:](
        std::forward<Args>(args)...);
  };
}

template <info CurrFn, typename Default, typename Base, typename Derived,
          typename... Args>
consteval auto make_default_dispatch_stub() {
  using return_type = return_type_of_t<CurrFn>;

  return +[](Default &&fn, Base *self, Args... args) -> return_type {
    constexpr auto member =
        find_matching_member<CurrFn, Derived, return_type, Args...>();

    if constexpr (!is_type(member)) {
      return static_cast<Derived *>(self)->[:member:](
          std::forward<Args>(args)...);
    } else {
      return std::forward<Default>(fn)(self, std::forward<Args>(args)...);
    }
  };
}

template <info CurrFn, typename Base, typename... Args>
consteval auto make_dispatch_table() {
  using return_type = return_type_of_t<CurrFn>;
  using function_type = return_type (*)(Base *, Args...);

  constexpr auto count = invoker_count<Base>();
  std::array<function_type, count> table{};
  std::size_t pos = 0;
  static constexpr auto invokers = invoker_list_of<Base>();

  template for (constexpr auto invoker : invokers) {
    using derived_type = [:invoker:];
    table[pos++] = make_dispatch_stub<CurrFn, Base, derived_type, Args...>();
  }

  return table;
}

template <info CurrFn, typename Default, typename Base, typename... Args>
consteval auto make_default_dispatch_table() {
  using return_type = return_type_of_t<CurrFn>;
  using function_type = return_type (*)(Default &&, Base *, Args...);

  constexpr auto count = invoker_count<Base>();
  std::array<function_type, count> table{};
  std::size_t pos = 0;
  static constexpr auto invokers = invoker_list_of<Base>();

  template for (constexpr auto invoker : invokers) {
    using derived_type = [:invoker:];
    table[pos++] = make_default_dispatch_stub<CurrFn, Default, Base,
                                              derived_type, Args...>();
  }

  return table;
}

} // namespace detail

template <std::meta::info CurrFn, typename Self, typename... Args>
  requires eligible_for_dispatch<std::remove_cvref_t<Self>>
constexpr decltype(auto) invoke(Self *self, Args &&...args) {
  using base_type = std::remove_cvref_t<Self>;
  assert(self->index != 0);
  assert(self->index <= detail::invoker_count<base_type>());

  static constexpr auto table =
      detail::make_dispatch_table<CurrFn, base_type, Args...>();

  return table[self->index - 1](self, std::forward<Args>(args)...);
}

template <std::meta::info CurrFn, typename Default, typename Self,
          typename... Args>
  requires eligible_for_dispatch<std::remove_cvref_t<Self>> &&
           std::is_invocable_r_v<detail::return_type_of_t<CurrFn>, Default,
                                 std::remove_cvref_t<Self> *, Args...>
constexpr decltype(auto) invoke(Default &&fn, Self *self, Args &&...args) {
  using base_type = std::remove_cvref_t<Self>;

  if (self->index == 0) {
    return std::forward<Default>(fn)(self, std::forward<Args>(args)...);
  }

  assert(self->index <= detail::invoker_count<base_type>());

  static constexpr auto table =
      detail::make_default_dispatch_table<CurrFn, Default, base_type,
                                          Args...>();

  return table[self->index - 1](std::forward<Default>(fn), self,
                                std::forward<Args>(args)...);
}

} // namespace cttd
