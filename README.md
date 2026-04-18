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

class printer : public cttd::enable_dispatch {
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

[Run in Compiler Explorer](https://godbolt.org/#z:OYLghAFBqd5QCxAYwPYBMCmBRdBLAF1QCcAaPECAMzwBtMA7AQwFtMQByARg9KtQYEAysib0QXAGx8BBAKoBnTAAUAHpwAMvAFYTStJg1DIApACYAQuYukl9ZATwDKjdAGFUtAK4sGe1wAyeAyYAHI%2BAEaYxCAAzBoArKQADqgKhE4MHt6%2BeqnpjgJBIeEsUTHxSXaYDplCBEzEBNk%2Bfly2mPaFDPWNBMVhkdFxibYNTS257Qrj/cGDZcOVAJS2qF7EyOwcJhoAguaxwcjeWADUJrFujcRMAJ6X2LsHZkcMJ17nl26iCkpNj2eh2Op0wFyuaHemGSBAUgP2wPeoPBPxm%2BEE8Jebw%2BXyubAamMROLB3wId2SmAA%2BgRboQ4bEngjXiDPiSrl5HLRCA8GUDmUjWSiAG41IjEQn7ZhsBTJJhbM4OAjoC4AdisCMlrEwMrlYKwDToqvVB32BEwLGSBjNKLJFKlYIsTCUmJmxC8DjOwSFqAA1tF6RYzgB6INnfjEM4MVBnFhy4jRrA0EJnLDEPAi5W2sFcmZAhEqgAieb2ZotVrZbiz9rOjudDLOrvdBDOjCYEXolPwOoIyAQRueZwbSpAIC8wQIsTM1M9DCwqnBBbOGkuxpMhZXxZDKcwSbBTAbFOQeDEFzMZizZxzBHMZjDJDOQoa7bBSkaveepctTGtpPJrbYNZOpgpBnFWWoAHSQWcBbROmmDoI8CoCDMmBCie3p4MqibzJSXq%2Bv6EDLP2%2ByDl46RGJGWo6vKaIjviTAbhqeyDrRIAig4JDfME/CIWwizEAGA6geaX7WuGZwQJCKGqMkEZMByCawRmZwgP2CRPOpMFpspABUkHgWuBZEWuxqDoO6DfkwlJ8eUlKoDCmQKGc9ndHCargdWlyLphjCODQ0R2VQEDEOaqAipSqTjtEECpnB6DLMshmMcxZk2f64EiQYWyUhEco%2BjFlnWeatkyjUMVKfBIEuY5CXJYOSUkduu6UkwwDACFwDfpgEAmOpvW4JgBq0COeF%2BgJ3y1pgjwgWlAmJbEq7rkxW4IE6ZzJF47Z4MgnpYIIeD%2BeKpqZV1Np/tWAAqmKQlsMItlywB4M%2BlLhp2eDdr2C5DugI6xRmL3xiw3wXSBrbPV2so9ggwOAryC3Fva1F6oNTCGiZxafuWZ12lqZwg6B524wAkgw3pjfS2BDm6Hr5BkAiBcRKUzN%2B21IQw0myd9I5joIk7Tmh3hsouvVWAki5SWaAtnAAtIhrE8xOU7NujjWkeRwCUdKso0cOID0clQksbrCt882%2BRfcu8Oq2c8lEGzZqqMrsSLnKWx/JSkIOwQ3PvAgNR%2BughF1WZ4kQLb0azSpkbIQ0jjIJ2hWzQogU9X16kk2T/ogZ7mCOwljNmWZB0Se9lIKFq1J/qnGnYPjIUsGFVKRYI0VZins0JfnKspYXg4hQQGwMOtaTB4XDU92Z1jWPko%2BqkW1v94PQ4s/HvzXlc8vjqbjwQNLXDzaZc9B1bBzrifQKFsGoaI9ryNDZuoZKM2wRzh%2BJ0/lcYEAZNIFf2CWlxUxH3TAABHMcIUnKdDwI9Z6r0IbfnfFcSaiFzCSFQVzEAf14IA1QEDK4ACMwgWQbyfYEtc6cwwsqJ%2BuFSb4WIBAAh8Ezg6TsFQIiUZc63WVmqISZCZJyQUsPJyXlDZmX1KjYaIBabdECt8Rh6AQLiLoCNWh5MJpAVhtgEcAsvBTRPkbFeLU/jRAIBAc2YAwBeWXnHD2Tp15uE3rzJWO896rFEb3Dxp4zDyM9E5KMZsIG%2BRnDOTO40kEaIZAZM8B8hLMxsWvb4YMOzwKhuouslMdI71YcsWWDIX65y%2BubawZwuAGyWiaPYN9dTbiGozZ4ZFggayqTrH6esUYG2OmWU6v4cbfwiZTMhUtw4hLoUVfiChCIFzOIvYgQ9WLYRCGXWO20WrEFuHcHq1tBxRnZss%2BOFkGhjPKMnVAQV%2Br9VGv6NJejsAgVdtqZOOdHY%2B17P7eChFarn3KW/LpH9KyEz6ektmKEpaOMVtOS5xAPbrEEJM7uwCB6zOsSshJG9jZb2cQyCAkKjlXPCekwi4F0gAC9uqfMWvPCpmNulXG4tGNwGwQoYnrIMk8dLNbYKoF4d4MjTlwp4Y1YufDOYQFWsnHy%2B1DoQAZWs3yXcBUTxmUPGVTL16H0MvdJQnoqDAodiKsV1J35UkaMAHwvkJkqrlaeNBZg0FbM8Q6syBqJV%2BTwK3I1KdLWCE7lMhFS9qVmk9YyuVwcNWdC1fCsySqLhp2wJQ0N3zvmdNEhWdlAAxBgJMeL1gaRRJVlcKSBWnFYkWIB81tz5RmrNqBlggF6kWc%2ByasZcQYPwM4ABZYq0Rf4Av/hVBRBNelggAEqDR7UO/SZw9jEGABTZ4N1oTNhClQLo2DZo0O9KIZ8X0QpgLwBAhh/bmGsJAtO2dk6TUKGMgq%2BqaoGydCoLkjSFgQCdv4nWsWEBWLhgAO6NAQlcM9FMw4zqvfpa9i4n0YPLmwIx3xR3ryeF8htq4m00rcOylVGbx3/j7dpSqg7cNnAQzh%2B0k6gPXRjqhNlrboxJnQNZBBCBGm4vodew%2BwqBF23tAx5AwbBBfR4y9bltR6anO%2BFhhgO8YmNTiSsp0/xTFCb47KgTFiS2xsoaQdxjr6pngLO9SGn1RC0FoA2QgYIWBkWbKtEUNsh4uoOsebdXKeWZA5VEswMmUqcZtoIlTqrKSeWdrtXyTmAp8uU/xgg3mDFx11eQrjEcu0CS%2BvMncOE5PxxuPcTZE9BxJxTuczS/a7nIDdo82Qudvajl9m8wOncOkpQDWCUOvnhmR1UkndjoihVUf4RJZ1e1XXRQ7j1%2B1fXdmJZLsN8LUK%2BUdwXFYgLvkgtanG/louOrfOiqdBFRovkICLdQSd%2B1umPGlxg1SLMZiDuCHbil1YMaa7yPlYfB1k2OYRigKXVzomGBHce6qNwa43C%2BOE25%2BmLXAf8S7ra07m3zueOXauhj668JbvoN8WaIF5EgRIzppHRPiceKA/pR4G2kfRtmrPDx48HX097oz29lKWfFj9Ui4rcbUCYTKZSn5KaUSYcZdhwj1Yf5i9xnjyXbByOgco1NoZgjYx%2Bjeh9BASzNr8sPrmjW5a/w7sGoPAtVJTnUgkyLqTcNUMpWjVPBIosCwQEmseh9p7QMXtA0RKD%2BuKS%2BoSwNjrKWltnbDC/RjUMWM06uJJ3HpXplG9mSb9357ILSdnlloximYflDOOpkLXOtOE/OzeAzyp/Fh9nDbGMTHGnbnw1hQzTHQImsGp57zojo2Z9RW4HxmSsXZKfaW2aH6nfF%2B/SQP9xAANuAo1iy9yxwMJpQxffnaG/nC7WaLv%2B0EdzyVoAQUjuMJc77x8Xwcf85ezoVyCk8HWmCq8TPvggaujMa5mFrynZxdcJ8RQwE3huf%2BJuRa9ikmsMNuHOQ89ujuh6VAz%2B1qqCVADARCQErutAVAKeCgnus63uiEvuJIN6Zk7WyuweIioe9GEevYUeKWFuW%2ByBu%2BDeIE%2BBmB5OWKHe1sn2eq32kc%2BewsmmPO8U/ukByKq8diciR6/e2AX6D6OSgIDubS769aeWumE%2BxAU%2BM%2Bc%2BUhC%2BS%2B%2Bik8l84aBB72Uaiecyusv6/64hcBXgB%2BO8SByw0h6BIEqh6h3wmhIGOBOhRhjOSUyGGMRqQutGZwseMuDoQER%2BsuUEmh86VGSudsKuVIKSvY1IbY9A2uQkP%2B%2BBgBxuFaVA5uMelu4B6R6sYYImMiF4VimREAOk9hLuOkLBkE7BPm/WnMwyaA3KTs3kqiAUbRzKbgxCUhsWGCOWPIbgf2ZRf42cMKiGLez46MZ8h8rEJK12QiFswcWWAeLRJB4yX0OKSc1y6ejazW/hbWzRSWMYweXWKWV6/uP%2BWCDG5RBe8h9cjcEUPOLc9COROe0QCUo%2Bs8T4WODuM8lg9uwsIW8Rr%2BTGmuEQtBxAouEudxyeU6HuaebBy%2B7Ov%2B/qKReiFKfhvyqagRwRp%2Be%2B1hh%2BIRgESg4RmAV%2Bc6pCMRd%2ByuD%2BCRxJB%2BEJUMyRz4aRjUGRJhABFRPJOReRbgYB1uRRdeYxmQvJIWlR1RsB8BJ2tqKBWqdRSJqe4EjRg4xBdsPRHRIyY00K7R%2BxqJuhrEwx3w4pUOExSEBp9Y/xU0PC8xsSusSx04RSIWlsCxeyGxZxScOxXRUKex%2BKNyx8EBwkeJd432mpyW/EUc3WNxxRCJDxfBL6zx4UzcZoHxf4D2sOta9afxWJIsQJVgwJYsX04JT%2BJJrJSRH%2B0J%2BRdBceVhB%2BipwE5%2BJOrZHiCZlpZOKJAxaJTEwhtpfOxYGqW4zSd8EiuJgu3wrE9EKibahJvaZwQgbuIR1JQCCee6EC900CT0HYcCTeqSaKrSKZVIyAQoy6gpS56Bmi0RU2A2WAJwN24cRElyEAl5OqLCy5QGCBtq%2BkC%2BUyP%2BuUSgkpi4rEx5HsZ5O4F5D6hRjUCmJiDhj6jw%2BS84vBS4gxcFTQCFT6yFKIViSikiOK2p3wgF12f40m6pIhXpfmdstpIem2%2BFdEjJlZGuA5tZsJ9BJFiJXZBkRpIZ0aA5DurC2Fs4BS0sJSShJ6GC5h0%2Brh8u8%2BXunhq%2BE5zah5dEKMs59KlulJu%2BDZpJO%2Bb52moeF%2Bvaq5JCtuoC4C2oW5MCu5JAzFU5usYFp5559ib5mi35dqiOhcrEpcGOWJlIUKQo3wDFZa/JmZZuoBBRDI9Zz%2BhlXlbZDqoFoU4UzlkFrl0F9Yyp3FN%2BXB24D5VcT5up3UMEulHlSBIEb5aBGBKpwi8OP5kEf5kaAFQEwFGCTlEFuR6VV5IpfZ2qEkQlSFIl84IibpX%2BwhzhFh%2BCzJiG1ADA9hklE1MlgGclWhClDRvZFSg4GFpiA1eSQ1uFIWIVhFUxxFLVWY5FTWcWrMkZMx9AdF7ZKMyiesTF5ZLJiRLF%2BZbFouJVsVZwnFWYcVCVQNvc2VvF6J/F%2BZglD6wlc4Ms4ln6i1M%2BP1JJthc1ThbuLZwNRsrS0lGhK17hYG61vhTEw518VEt8CoBASoQ5/IxIKIsk44hItNyIXEaQNImArAEoewP%2Bo5lN1NhxJwCmRVUKDEJ8gtfwwtOUyU4tTkhFTWMtf1ESxorKZmJkTUOEkK9IbgJFIEOKTAutfpOUBtoSHsBxgY48CtJFUcG0W0O0SSCR%2B5iC2t/SjMNtXIyAdajULW2MuGpllMlCd4qAEAX58pkgv5Xu/uN1K2AmVi056lKA0WEO/2wZneJhRV3w0dM1BAzGCgThZhk%2Bk1s%2B%2BN2hRN6qiaK%2BTECtet1tm07titFJ60tdrMrE7EYoLa0xQYOkM46ZVA0QjA8oFmLAzCoY3cbt20ntKUet2u96L%2BkKp1BK2d70B8c8Qk3tPSvtkRK1D4AhgdwdoGHl4dOB/uX6usDNggtAAON4cxRYZ4edONBdS1Rd1%2B8lOBT2iljU48PhytBgEtOKEQNdtt9dhhzwY9HtQkf9091Cc9VwJFO8i9V6K4K9Xt/h69ZGm9z9/tO9/AQdIddVYdDVEdkag4J9rSZ9piV99pN9Zgd9I4uNslGDBNCUIE79KUn9DppCP9sthtO0qkoDQDrtTdYDgq3DkDg0G6dC89QZ8Dy9jOa9n8Jl6DFM29mEu9uDNq%2BD4EjVhBxDrEZDPUZ419XiNDIAdDy1DDC%2Bb9pdQkbDFdVKKD8jQ6csNITYNsyQeA/Y7DFSjYHoZEItbjUcTAbj3wvjLUQT9Y19TW44NewQnJKUVtOkVtViDA1htAMI4ohxg41doth8f9f1wchFCol1/DJatq2TjppDaY59l9Z45swQwtCgqkETt9QDMNucgxJFT62DEA7QXiAALGYDeCBDeH7KZtGD%2BiQLQAhNEsHIkwXrajWR6RU%2BOBffo2YLUw5n6Q0x41QyBB04NXOO00BJ06gEHT0zeP04M14iM7QGMxM1M15jM6gSU5IKYMaafZUwQCszeOs/U405Q5c3s3tQc480oMc6c0M2eBc808M50Dc2cOM8QJMzeIMaxG0U7CDlcF4r1G4FJgYxkxgmQ182eEmO9H2GaLmNM/i9Gu6UpfsBwKsMNBwAkLwH4BwFoKQKgJwCDsCZYA2OsJsCSK8DwKQN7Gy/S6sD6CAAkBoOBLEAAJxyuSAAAcGgZgCQCQKocrCQsQKovT%2BgnAvTLLmgvAnLHAvADTGgIrxrqwcAsAMAiAKAuCbj9AZAFAkkTrdAwws6zAyQCgCAqAh%2BNAB%2B/olAEQxrpAEQwQjQdwnAwrkbzAxAdwAA8hENoKKLG7wGgCwGwIIEmwwLQDG2K6QFgBEF4MANcKZg09wLwFgLGEYOIEW/gCFLUCKFW%2By7nDUByNsOy1FIy%2By1yBELcImx4FgOGzSHgCwBm6QCKMQBEGkJgDBHW8AFyEYNa3wAYLOgAGpuo/pJt2hTv8CCAiBiDsBcB6uHvyBKBqDhu6DtAGCrumA8s2ADsNOQCrDVTIScDSxUAo6ijubSxoheSPtFkWC2omsztphYCvuEQdBdCZAuCziTBtCkCBDzClDlB5BpB0xZCeCtCYcFCZADDofDDTCdB/sCC9ATC4dTCwfkc9CzBEdDAxDTCzBId6DMxNCMfvr7xrAbBbASAMucDMukCsvsumtnCqBKuSDSySC9NnDADlYSTOMMA%2BhEQQC4CED3iHD7y8CitaBMOSuxCxDgRKtKuxCmfmdmcWemf6scCGukCTv9PgQaBytmCJCqsudufSvUOicmucDmsgCWt6f0ukC2sOvrAEAbSH5utZvOvRChBaicCSfSeycKj3sazBQ7hwcCBw3csge2rLC8DwSaeQd6AXvHviBnsyCCCKAqDqBFu3ukA/q3DJAZuCdMtGtFumtJschRfOQ6rJcydycKc7QQDKeqcSQeAWiesRjaeFdWtitMN%2BxMCpiUDtf2eTsavgRysaCau9Nyuau7dyv7dKsifhumsBdBfWsSsgD7fgSSAaAaCxBcAHe9P9MaC9Oye2exCddif%2BcLf6ftdmC/d%2BdmsA/ivTv%2Bjwe9NAA%3D)

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
  `cttd::enable_dispatch::index`.
- `cttd::invoke<current>(...)` builds a dispatch table for the current base
  member and uses the runtime slot to select the target.
- The target member is matched by reflected name and compatible signature on the
  derived type.
