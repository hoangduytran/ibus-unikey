# `macro_file_io_test` — instructions

**Source:** [`tests/macros/macro_file_io_test.cpp`](../../macros/macro_file_io_test.cpp)  
**CTest name:** `macro-file-io-test`

## Purpose

Verifies `macro_table_load_any_format()` loads non-empty macro tables from fixture files **by filename extension**: native `.txt`, Espanso-style `.yaml`, Apple XML/binary `.plist` (via libplist), generic `.json` (array or map, via json-glib), `.csv` and `.tsv` (via libcsv). The fixture directory is passed as **`argv[1]`**.

## Prerequisites

- **GTK / GLib** (pkg-config `gtk+-3.0`, `gio-2.0`) — same as other `tests/macros/` targets.
- **Interchange codecs** linked into `macro_interchange`: **json-glib**, **libplist**, **libcsv**, **yaml-cpp** (CMake will not configure the setup target without the matching `-dev` packages).
- Fixture directory containing at least:
  - `unikey_macros.txt`
  - `unikey_macro.yaml`
  - `unikey_macro.plist`
  - `generic_macros.json`
  - `macros.csv`
  - `macros.tsv`

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
- Changes to macro import plumbing should keep **all registered extensions** behaving consistently (each `g_test_add_func` path should stay green after `ctest -R macro-file-io`).

## Extend this test

When adding new interchange formats, register a suffix in the appropriate `macro_handler_*.cpp`, add a small fixture under [`tests/macros/`](../../macros/), extend [`macro_file_io_test.cpp`](../../macros/macro_file_io_test.cpp) and the combined `test_all_formats_resolve_by_filename_extension`; update this instruction and [`tests/macros/README.md`](../../macros/README.md).

## Manual QA (humans; not run by CTest)

This executable only checks **fixture → non-empty table** per extension. Full review requires **typing macros in IBus**, **accent correctness**, and **export → clear → import** cycles. Use the detailed checklist in [**`tests/macros/TEST_PLAN_macro_interchange_io.md`**](../../macros/TEST_PLAN_macro_interchange_io.md#manual-and-round-trip-qa-humans-and-reviewers) (import [`unikey_macros.txt`](../../macros/unikey_macros.txt) first, then plist/YAML/JSON/CSV round-trips and cross-OS plist where available).
