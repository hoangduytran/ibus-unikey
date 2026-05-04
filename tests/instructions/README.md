# Test suite instructions

Use these notes when reviewing changes or running tests locally. Each file pairs **one** executable test source under [`tests/`](../) so expectations stay obvious without reading every `.cpp`.

## Run everything

[`enable_testing()`](../../CMakeLists.txt) is set at the project root. CMake generates a **`test`** build target that runs **CTest** — from the build directory:

```sh
cmake --build . --target test
make test            # Makefile generator
ctest --output-on-failure
```

GTK-based targets under [`tests/macros/`](../macros/) are added only when **`setup/macro_file_io.cpp`** exists (see [`tests/CMakeLists.txt`](../CMakeLists.txt)). If that helper is absent, **`ukengine_test`** alone runs via CTest. When macros tests are enabled and **`cmake ..` fails** on missing dependencies, install GTK 3 **and** macro interchange libraries—for example on **Debian / Ubuntu**:

```sh
sudo apt update
sudo apt install libgtk-3-dev libjson-glib-dev libplist-dev libcsv-dev libyaml-cpp-dev
```

`setup/CMakeLists.txt` requires these for `macro_interchange` (used by `ibus-setup-unikey` and `macro-file-io-test`).

On **Fedora**: `sudo dnf install gtk3-devel`. On **Arch Linux**: `sudo pacman -S gtk3`. Use your distro’s package search if names differ (`gtk+3`, `pkg-config`, etc.).

To **see if GTK 3 dev files are already installed** before running `apt install`:

- **Debian / Ubuntu** — package status (installed vs not):

  ```sh
  dpkg -s libgtk-3-dev
  ```

  Exit code `0` and a line like `Status: install ok installed` means it is installed. You can also list the version with `apt list --installed libgtk-3-dev 2>/dev/null`.

- **Any distro with pkg-config** — headers/pkg-config module for GTK+ 3:

  ```sh
  pkg-config --exists gtk+-3.0 && pkg-config --modversion gtk+-3.0
  ```

  If this prints a version number, CMake/pkg-config can usually find GTK 3.

- **Fedora**: `rpm -q gtk3-devel`

- **Arch Linux**: `pacman -Qi gtk3`

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

### Macro interchange: manual round-trip QA

`macro-file-io-test` only checks fixture loads. For **typing in real apps**, Vietnamese **accents**, **export → clear → import**, and **Linux ↔ macOS plist**, follow [**`tests/macros/TEST_PLAN_macro_interchange_io.md` — Manual and round-trip QA**](../macros/TEST_PLAN_macro_interchange_io.md#manual-and-round-trip-qa-humans-and-reviewers).

## Related planning / QA markdown

Higher-level or historical scenarios live elsewhere, for example [`tests/setup-macro-ui/TEST_PLAN_current_commit_search_sort_edit_return.md`](../setup-macro-ui/TEST_PLAN_current_commit_search_sort_edit_return.md), [`tests/macros/TEST_PLAN_macro_interchange_io.md`](../macros/TEST_PLAN_macro_interchange_io.md), and [`tests/macro-engine/`](../macro-engine/). Those complement—but do not replace—the per-source instructions above.
