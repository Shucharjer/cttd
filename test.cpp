#include "cttd.hpp"
#include <iostream>
#include <print>
#include <vector>

using namespace cttd;

class invoker_a;
class invoker_b;
class invoker_c;

class base;
consteval { define_invokers<base, invoker_a, invoker_b, invoker_c>(); }
class base : public enable_dispatch<base> {
public:
  template <typename... Args> void foo(Args &&...args) {
    constexpr auto current = std::meta::current_function();
    return invoke<current>(this, std::forward<Args>(args)...);
  }
};

class invoker_a : public base, public std::vector<int> /* interference item */ {
public:
  invoker_a() { set_invoker<base>(this); }
  template <typename... Args> void foo(Args &&...args) {
    (std::println("{}", std::forward<Args>(args)), ...);
  }
};
class invoker_b : public base {
public:
  invoker_b() { set_invoker<base>(this); }
  template <typename... Args> void foo(Args &&...args) {
    (std::print("{}", std::forward<Args>(args)), ...);
  }
};
class invoker_c : public base {
public:
  invoker_c() { set_invoker<base>(this); }
  template <typename... Args> void foo(Args &&...args) {
    (std::print("{} ", std::forward<Args>(args)), ...);
  }
};

template <typename> struct api {};

struct usr_api : api<usr_api> {};

int main() {
  base *base = nullptr;

  invoker_a a;
  invoker_b b;
  invoker_c c;

  base = &a;
  std::println("pos in invokers: {}", base->index);
  base->foo(1, "42", "hello world");
  base = &b;
  std::println("pos in invokers: {}", base->index);
  base->foo(1, "42", "hello world");
  base = &c;
  std::println("pos in invokers: {}", base->index);
  base->foo(1, "42", "hello world");
  std::cout << "\n";

  std::println("finish test");

  return 0;
}
