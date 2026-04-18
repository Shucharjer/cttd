#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <meta>
#include <type_traits>
#include <utility>
#include <vector>

namespace cttd {

namespace detail {

template <typename Base>
struct invokers; // for no macro define derived type list

}

template <typename Base> struct enable_dispatch {
  std::uint32_t index = 0;
};

// define a special "type list" for vtable search
template <typename Base, typename... Derived> consteval void define_invokers() {
  using namespace std::meta;

  std::vector<info> members;
  template for (constexpr auto derived : {^^Derived *...}) {
    data_member_options options{.name = identifier_of(remove_pointer(derived))};
    members.emplace_back(data_member_spec(derived, options));
  }
  define_aggregate(^^detail::invokers<Base>, members);
}

// has public identifier
template <typename T>
concept eligible_for_dispatch = std::derived_from<T, enable_dispatch<T>>;

namespace detail {

template <typename T, typename Invokers> struct position_of {
  static constexpr std::uint32_t value = [] consteval -> std::uint32_t {
    using namespace std::meta;

    std::uint32_t pos = 0;
    auto context = access_context::unchecked();
    for (auto member : nonstatic_data_members_of(^^Invokers, context)) {
      if (is_same_type(^^T, remove_pointer(type_of(member)))) {
        return pos;
      }
      ++pos;
    }
    return static_cast<std::uint32_t>(-1);
  }();
};

} // namespace detail

// set index
template <typename Base, typename Derived>
  requires eligible_for_dispatch<Base> && std::derived_from<Derived, Base>
constexpr void set_invoker(Derived *self) noexcept {
  constexpr auto pos =
      detail::position_of<Derived, detail::invokers<Base>>::value;
  static_assert(pos != static_cast<std::uint32_t>(-1),
                "Derived is not present in invokers<Base>.");
  static_cast<enable_dispatch<Base> *>(self)->index = pos + 1;
}

namespace detail {

using namespace std::meta;

template <typename Base> consteval auto invoker_members() {
  return std::define_static_array(
      nonstatic_data_members_of(^^invokers<Base>, access_context::unchecked()));
}

template <typename Base> consteval std::uint32_t invoker_count() {
  return static_cast<std::uint32_t>(invoker_members<Base>().size());
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
        if constexpr (has_parent(member) &&
                      is_same_type(parent_of(member), ^^Derived)) {
          if constexpr ((is_function(member) || is_function_template(member)) &&
                        reflected_member_invocable<member, Derived, Ret,
                                                   Args...>) {
            return member;
          }
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
    static_assert(member != ^^void,
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

    if constexpr (member != ^^void) {
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
  static constexpr auto members = invoker_members<Base>();

  template for (constexpr auto member : members) {
    using derived_type = [:remove_pointer(type_of(member)):];
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
  static constexpr auto members = invoker_members<Base>();

  template for (constexpr auto member : members) {
    using derived_type = [:remove_pointer(type_of(member)):];
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
