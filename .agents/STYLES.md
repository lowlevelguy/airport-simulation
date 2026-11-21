# STYLES — code style guide

Stylistic and coding conventions for this project (and the author's embedded code in
general). Agents should *enforce* these rules and *flag* violations (especially
MISRA, §9) rather than silently "fixing" them.

**Scope:** this guide applies to code written by the author. Third-party / vendor
code vendored into the repository is **exempt at every level** — it is not to be
reformatted or rewritten.

---

## 1. Language & standard

- C only. C11 / GNU11 (`-std=gnu11`, set in the build system).
- C89/C99-era vendor code is fine as-is; the author's code may use C11 features
  where they help (e.g. `_Static_assert`).
- `extern "C"` guards in headers that may be consumed from C++.

## 2. Formatting

- **Tabs only** for indentation — the character used is `\t`. The 4-space display
  width is an editor setting, not something to emit as literal spaces.
- **K&R / 1TBS braces**: opening brace on the same line as the declaration or
  control keyword.
  ```c
  void module_ctrl_pins_init(void) {
      ...
  }

  if (ret == ERR_OK) {
      ...
  }
  ```
- **Always use braces**, even for single-line `if` / `while` / `for` / `else`
  bodies. (MISRA 15.6.)
- One statement per line.
- Lines wrap at ~100 columns. A wrapped continuation is indented one tab. Group
  arguments naturally when wrapping a call, one tab deeper than the statement:
  ```c
  hal_gpio_write(CFG_LED_OUT_PORT, CFG_LED_OUT_PIN,
      GPIO_LEVEL_LOW);
  ```
- Struct initializers use `= {0}` for zero-init:
  ```c
  gpio_config_t gpio_init = {0};
  ```
- Align macro name→value columns with tabs:
  ```c
  #define CFG_IRQ_PORT     GPIOC
  #define CFG_IRQ_PIN      5
  ```

## 3. Blank lines

Rules for vertical whitespace between file elements:

- **2 blank lines** between `/* XXX ---- */` **file sections** (the banner-style
  section headers, §5).
- **1 blank line** between type declarations and between function definitions inside
  the same file section.
- **1 blank line** between groups of variable or macro declarations inside the same
  file section when the groups serve different purposes (e.g. a TX group vs an RX
  group). No blank line between members of the same group.
- Macros follow the same rule as variables.

## 4. Naming

| Thing              | Convention          | Example                     |
|--------------------|---------------------|-----------------------------|
| Files              | `snake_case`, board/mcu-qualified where relevant | `bsp_stm32f401xe.c` |
| Functions / vars   | `snake_case`        | `module_ctrl_pins_init`     |
| Private helpers    | `module_` prefix    | `module_ctrl_pins_init`     |
| Enum typedefs      | `CamelCase_e`       | `BspState_e`                |
| Enum entries       | `ALL_CAPS`, suffixed with the enum typedef name | `BSP_STATE_OK` |
| Struct typedefs    | `snake_case_t`      | —                           |
| Macros / constants | `ALL_CAPS`          | `CFG_DEBUG`                 |

- Enum entries are always prefixed by the enum typedef name in caps:
  `EnumTypedef_e` → `ENUM_TYPEDEF_VALUE_1`.
- **Trailing comma** after the last enumerator (guards against missing-comma errors
  when appending entries).
- `void` in empty parameter lists; **always name** function parameters (MISRA 8.2).
- Public interface symbols keep the fixed contract names where the contract is
  defined externally (e.g. a vendor porting layer) — do not rename them.
- See §9 for the integer/boolean type policy.

## 5. File layout

Author's `.c` files follow the banner-style section structure, each headed by a
banner comment with aligned dashes filling to ~column 100:

```
/* Includes ------------------------------------------------------------------*/
/* Types ---------------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/
```

- Sections appear in the order above. Omit a section that has nothing in it.
- No license/copyright banner in author files (vendored files carry their own).
- Includes first, per §6.
- Private helpers are declared `STATIC` (see §7) and **defined before use** — no
  separate private prototype section.
- Variables live in the section that matches their visibility (private variables at
  file scope, locals inside the function body).

## 6. Includes

Order, most-external to most-internal:

1. **libc** — with `<>`
2. **MCU / vendor headers** (device headers, CMSIS) — with `""`
3. **Vendor SDK / HAL drivers** — with `""`
4. **Other Drivers** — with `""`
5. **Project utility headers** — with `""`
6. **Own headers** — with `""`

```c
#include <stdio.h>          /* libc */
#include "stm32f401xe.h"    /* device / CMSIS */
#include "stm32f4xx_hal.h"  /* HAL drivers */
#include "conf_app.h"       /* own */
```

(The boundary between device headers, HAL drivers and project utils is fuzzy; keep
the relative order and group by category.)

## 7. Statics, testability & Doxygen

- File-scope statics use the `STATIC` macro so they can be exported to unit tests:
  ```c
  #ifndef BUILD_TESTING
  #define STATIC static
  #else /* defined(BUILD_TESTING) */
  #define STATIC
  #endif
  ```
- **Every function is documented with Doxygen.**
  - Public functions: documentation goes with the **prototype in the header**.
  - Private functions: documentation goes with the **definition**.
  - Vendor-required functions the author implements (e.g. porting-layer hooks):
    document at the definition (the vendor header is off-limits).
  - Tag usage:
    - `@brief` — always.
    - `@param` — for every parameter.
    - `@return` — when the function returns a value.
    - `@note` — useful extra information.
    - `@details` — when the mechanism warrants a fuller explanation.
  - Keep the block minimal; no vendor-style `@fn`/`@see`/`@warning` bloat unless
    genuinely useful.
- Comments are written in **British English** ("initialise", "behaviour").
- `//` for single-line inline notes; `/* */` for multi-line comment blocks.

## 8. Interfacing with vendor code

- Vendored third-party code is **never modified** — not formatting, not
  refactoring, not "fixing".
- The author's integration code lives in the porting / wrapper layers and the
  project's configuration headers.
- Follow the vendor's naming for the porting contract exactly; do not rename
  contract symbols.
- When the vendor's typedefs (`uint8`, `sint8`, ...) cross the boundary, use them at
  the boundary; do not change the vendor's function signatures.

## 9. MISRA C:2012

The author is learning MISRA C through embedded work. The subset below is adopted for
**author-written code**; vendor code is exempt. Agents must **flag** violations when
the author misses them.

### Adopted rules

- **8.2** Function types in prototype form, named parameters.
- **8.4 / 8.5** Compatible single declaration; objects/functions declared before use.
- **10.4 / 10.5** Operands of operators have the same essential type category; no
  mixing enums/integers implicitly. Use explicit casts where mixing is unavoidable.
- **14.4** Controlling expressions are essentially Boolean: write `if (ret ==
  ERR_OK)` not `if (ret)`; write `while (flag == true)` not `while (flag)`.
- **15.6** Bodies of `if` / `else` / `while` / `for` are compound statements — the
  braces rule in §2.
- **16.1–16.5** `switch`: every non-empty case terminates with `break` / `return`;
  a `default` clause is present.
- **17.7** The return value of a non-void function is used (e.g. HAL status checked
  against the success code).
- **21.1** Reserved identifiers are not used: **avoid leading/trailing double
  underscores** in include guards. Prefer `CFG_APP_H` over `__CFG_APP_H__`.
  (Existing author headers with `__..._H__` guards are to be migrated; vendor guards
  are exempt.)
- **Directive 4.6** Typedefs indicate size and signedness — the author's application
  code uses `stdint`.

### Type policy

- **Vendor/driver boundary:** use the vendor's typedefs (`uint8`, `uint16`,
  `uint32`, `sint8`, `sint16`, `sint32`) where the contract requires them.
- **Application code:** use `stdint` (`uint8_t`, `int32_t`, ...).
- **Booleans:** use `bool` from `<stdbool.h>` for flags and predicates in
  application code. Cast/convert explicitly at the vendor boundary.

### Accepted deviations

Logged here as they arise. Each entry: rule, location, justification.

| Rule                     | Location | Justification |
|--------------------------|----------|---------------|
| 17.3 (variadic `printf`) | debug print macro | Debug print macro wrapping libc `printf`; accepted for tooling, keep log level sane |
| §8 (vendor never modified) | *location* | *justification* |

---

## 10. Canonical examples

- *(To be filled in: an author-written `.c` file exemplifying file layout, sections,
  `STATIC`, K&R braces, Doxygen style, wrapping.)*
- *(To be filled in: a config header exemplifying header guards, tab-aligned
  macros, include grouping.)*
- *(To be filled in: a thin config header.)*

---

## 11. Open industry standards as reference

- Prefer open industry-standard specifications as references when defining
  conventions, contracts and patterns (e.g. AUTOSAR BSW specifications such as
  the SPI handler/driver specification, for driver and middleware behaviour,
  API semantics and return codes) over ad-hoc or private conventions.
