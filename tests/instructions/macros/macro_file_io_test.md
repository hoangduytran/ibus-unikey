# `macro_file_io_test` — instructions

**Source:** [`tests/macros/macro_file_io_test.cpp`](../../macros/macro_file_io_test.cpp)  
**CTest name:** `macro-file-io-test`

## Purpose

Verifies `macro_table_load_any_format()` loads non-empty macro tables from fixture files **by filename extension**: native `.txt`, Espanso-style `.yaml`, Apple XML `.plist`, generic `.json` (array or map), and `.csv` samples under the fixture directory passed as **`argv[1]`**.

## Prerequisites

- GTK/GLib test deps (same as other `tests/macros/` targets).
- Fixture directory containing at least:
  - `unikey_macros.txt`
  - `unikey_macro.yaml`
  - `unikey_macro.plist`
  - `generic_macros.json`
  - `macros.csv`

The CMake-defined command passes [`tests/macros/`](../../macros/) as that directory.

**Test plan:** [`tests/macros/TEST_PLAN_macro_interchange_io.md`](../../macros/TEST_PLAN_macro_interchange_io.md) maps scenarios to this executable and related macro UI tests.

## How to run

```sh
ctest -R macro-file-io-test --output-on-failure
# or manually:
./test-macro-file-io /path/to/ibus-unikey/tests/macros
```

## What reviewers should check

- **Zero macros loaded** for any format: fixture paths wrong, parser regression, or loader ignoring extension.
- Changes to macro import plumbing should keep **all three** extensions behaving consistently where supported.

## Extend this test

When adding new interchange formats, place fixtures beside these files and extend [`macro_file_io_test.cpp`](../../macros/macro_file_io_test.cpp); update this instruction with new filenames and expectations.
