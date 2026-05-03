# Test suite instructions

Use these notes when reviewing changes or running tests locally. Each file pairs **one** executable test source under [`tests/`](../) so expectations stay obvious without reading every `.cpp`.

## Run everything

[`enable_testing()`](../../CMakeLists.txt) is set at the project root. CMake generates a **`test`** build target that runs **CTest** — from the build directory:

```sh
cmake --build . --target test
make test            # Makefile generator
ctest --output-on-failure
```

GTK-based targets under [`tests/macros/`](../macros/) are added only when **`src/setup/macro_file_io.cpp`** exists (see [`tests/CMakeLists.txt`](../CMakeLists.txt)). If that subtree is absent, **`ukengine_test`** alone runs via CTest. When macros tests are enabled and **`cmake ..` fails** on missing GTK/pkg-config, install **`libgtk-3-dev`** (Debian/Ubuntu naming) or the equivalent for your distro.

## Pairing table

| Instruction | Test source | CMake test name (CTest) |
|-------------|---------------|-------------------------|
| [`ukengine_test.md`](ukengine_test.md) | [`tests/ukengine_test.cpp`](../ukengine_test.cpp) | `ukengine_test` |
| [`macros/macro_file_io_test.md`](macros/macro_file_io_test.md) | [`tests/macros/macro_file_io_test.cpp`](../macros/macro_file_io_test.cpp) | `macro-file-io-test` |
| [`macros/macro_export_filename_test.md`](macros/macro_export_filename_test.md) | [`tests/macros/macro_export_filename_test.cpp`](../macros/macro_export_filename_test.cpp) | `macro-export-filename-test` |
| [`macros/macro_dialog_state_test.md`](macros/macro_dialog_state_test.md) | [`tests/macros/macro_dialog_state_test.cpp`](../macros/macro_dialog_state_test.cpp) | `macro-dialog-state-test` |

Run a single test by name:

```sh
ctest -R ukengine_test --output-on-failure
ctest -R macro-file-io --output-on-failure
```

## Related planning / QA markdown

Higher-level or historical scenarios live elsewhere, for example [`tests/setup-macro-ui/TEST_PLAN_current_commit_search_sort_edit_return.md`](../setup-macro-ui/TEST_PLAN_current_commit_search_sort_edit_return.md) and [`tests/macro-engine/`](../macro-engine/). Those complement—but do not replace—the per-source instructions above.
