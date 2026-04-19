# Compile-Time Template Dispatch

Compile-time dispatch for reflection-enabled C++26 toolchains.

`cttd` is a small header-only library that lets a base class expose a stable API
while forwarding calls to derived implementations through a compile-time
generated dispatch table. Instead of writing a manual registry, switch, or
virtual interface for every call path, you register the participating derived
types once and dispatch by reflecting the current base member.

The project is intentionally lightweight and experimental. It is designed for
toolchains that support C++ reflection via `<meta>` and `-freflection`.

## Why use it?

- Header-only and easy to drop into an existing experiment.
- Generates dispatch stubs at compile time from reflected type information.
- Keeps the user-facing entry point on the base type.
- Supports an optional fallback path when dispatch is not initialized or no
  matching derived member exists.

## Requirements

- A reflection-enabled C++26 toolchain with support for `<meta>`, like GCC 16.

## Installation

Copy `cttd.hpp` into your project and include it:

```cpp
#include "cttd.hpp"
```

## Quick start

1. Forward-declare the derived types that will participate in dispatch.
2. Register them with `cttd::define_invokers<Base, Derived...>()`.
3. Inherit the base type from `cttd::enable_dispatch`.
4. Call `cttd::set_invoker<Base>(this)` in each derived constructor.
5. Inside the base member, capture `std::meta::current_function()` and pass it
   to `cttd::invoke`.

## Example

```cpp
#include "cttd.hpp"
#include <iostream>
#include <print>

class line_printer;
class compact_printer;

class printer;
consteval {
  cttd::define_invokers<printer, line_printer, compact_printer>();
}

class printer : public cttd::enable_dispatch<printer> {
public:
  template <typename... Args> void print(Args &&...args) {
    constexpr auto current = std::meta::current_function();
    return cttd::invoke<current>(this, std::forward<Args>(args)...);
  }
};

class line_printer : public printer {
public:
  line_printer() { cttd::set_invoker<printer>(this); }

  template <typename... Args> void print(Args &&...args) {
    (std::println("{}", std::forward<Args>(args)), ...);
  }
};

class compact_printer : public printer {
public:
  compact_printer() { cttd::set_invoker<printer>(this); }

  template <typename... Args> void print(Args &&...args) {
    (std::print("{}", std::forward<Args>(args)), ...);
    std::cout << '\n';
  }
};

int main() {
  line_printer a;
  compact_printer b;

  printer *current = &a;
  current->print("hello", "world");

  current = &b;
  current->print("hello", "world");

  return 0;
}
```

To run the repository example in `test.cpp`:

```sh
g++ -std=c++26 -freflection test.cpp -o test && ./test
```

[Run in Compiler Explorer](https://godbolt.org/z/s6YeTjoPs)

## Optional fallback dispatch

If a call site should fall back to a default implementation when the dispatch
slot is not initialized, or when a derived type does not provide a matching
member, use the overload that accepts a callable:

```cpp
template <typename... Args> void print(Args &&...args) {
  constexpr auto current = std::meta::current_function();
  return cttd::invoke<current>(
      [](printer *self, auto &&...fallback_args) {
        (std::print("{}", std::forward<decltype(fallback_args)>(fallback_args)),
         ...);
        std::cout << '\n';
      },
      this, std::forward<Args>(args)...);
}
```

## How it works

- `cttd::define_invokers<Base, Derived...>()` records the participating derived
  types at compile time.
- `cttd::set_invoker<Base>(this)` stores the derived slot in
  `cttd::enable_dispatch<Base>::index`.
- `cttd::invoke<current>(...)` builds a dispatch table for the current base
  member and uses the runtime slot to select the target.
- The target member is matched by reflected name and compatible signature on the
  derived type.
