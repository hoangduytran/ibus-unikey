# `macro_export_filename_test` — instructions

**Source:** [`tests/macros/macro_export_filename_test.cpp`](../../macros/macro_export_filename_test.cpp)  
**CTest name:** `macro-export-filename-test`

## Purpose

Unit-tests **pure string helpers** in [`macro_export_filename.cpp`](../../../src/setup/macro_export_filename.cpp): completing export paths with the chosen extension, detecting extension conflicts, and edge cases (hidden dotfiles, `NULL` preferred extension).

## Prerequisites

- GTK/GLib (header-only style helpers but linked with GTK test libs per CMake).
- **No** fixtures or `argv` arguments.

## How to run

```sh
ctest -R macro-export-filename-test --output-on-failure
./test-macro-export-filename
```

## What reviewers should check

Export dialog behavior should remain aligned with these rules:

- Missing extension → append selected filter extension.
- Path already ends with the selected extension → unchanged.
- Selected filter differs from existing extension → append second extension (`.plist.yaml` pattern) and expose conflicts to UI logic via `macro_export_filename_has_extension_conflict`.

## Extend this test

Add one case per new export filter or naming rule; keep tests deterministic (no real filesystem besides string manipulation).
