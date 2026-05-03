# `macro_dialog_state_test` — instructions

**Source:** [`tests/macros/macro_dialog_state_test.cpp`](../../macros/macro_dialog_state_test.cpp)  
**CTest name:** `macro-dialog-state-test`

## Purpose

Validates **macro import/export dialog directory memory**: defaults (Documents), remembering parent directories after picking files, and shared last-working-dir between import and export flows. Uses an isolated temporary **`HOME`** and in-memory **GSettings** backend.

## Prerequisites

- Compiled **`gsettings`** schemas installed or discoverable. The CMake test passes the **`gsettings`** subdirectory under the **build tree** as `argv[1]`:

  ```cmake
  COMMAND test-macro-dialog-state ${PROJECT_BINARY_DIR}/gsettings
  ```

  That matches setup / schema generation for ibus-unikey (see main [`CMakeLists.txt`](../../../CMakeLists.txt) / `src` wiring).

- GTK/GLib.

## How to run

```sh
ctest -R macro-dialog-state-test --output-on-failure
```

Manual run (adjust schema dir if needed):

```sh
./test-macro-dialog-state /path/to/build/gsettings
```

## What reviewers should check

- Failures often mean **schema path** wrong or **`ibus_unikey_config_*`** key behavior changed.
- Tests mutate env vars (`HOME`, `GSETTINGS_*`)—run single-threaded; do not assume parallel safety without audit.

## Extend this test

New dialog persistence keys should get cases here and a short bullet in this file.
