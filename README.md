### Hiện tại bộ gõ này không còn được hỗ trợ. Do đó, nếu bạn gặp vấn đề trong quá trình sử dụng thì có thể xem xét sử dụng một trong các tùy chọn sau:
- [ibus-bamboo](https://github.com/BambooEngine/ibus-bamboo)
- [ibus-bogo](https://github.com/BoGoEngine/ibus-bogo)
- Một trong các bản fork của ibus-unikey https://github.com/vn-input/ibus-unikey/network/members
- Hoặc các bộ gõ khác...

IBus-Unikey IME
===============

[![Build Status](https://travis-ci.org/vn-input/ibus-unikey.svg?branch=master)](https://travis-ci.org/vn-input/ibus-unikey)

ibus-unikey is an [IBus](https://github.com/ibus/ibus) IME.
It use Unikey-engine for progress key event.
(a modified version of it)

### For install, please visit [wiki](https://github.com/vn-input/ibus-unikey/wiki) page

### Technical documentation

- [Technical Note](TECHNICAL_NOTE.md)

### Building from source (development)

Typical out-of-tree build:

```sh
mkdir -p build && cd build
cmake ..
cmake --build .
```

Install steps remain as described in the wiki; the commands above are enough to compile the engine, IBus components, and setup tools for local work.

**Macro interchange (setup I/O)** — `ibus-setup-unikey` links a static `macro_interchange` library that imports/exports UniKey text, Espanso-style YAML, JSON, Apple plist, CSV, and TSV. Configure fails if these dev packages are missing on Debian/Ubuntu:

```sh
sudo apt install libgtk-3-dev libjson-glib-dev libplist-dev libcsv-dev libyaml-cpp-dev
```

(Fedora/arch names differ; use your distro’s search for `json-glib`, `libplist`, `libcsv`, `yaml-cpp`.)

### Testing

The project uses **CTest**. After configuring a build directory, run the full suite in any of these equivalent ways:

```sh
cd build
cmake --build . --target test
# or, with Makefiles:
make test
# or:
ctest --output-on-failure
```

- **`ukengine_test`** — core engine and macro table smoke tests (no extra dependencies).
- Tests under **`tests/macros/`** exercise macro setup helpers and **loader routing by file extension** (`.txt`, `.yaml`, `.yml`, `.json`, `.plist`, `.csv`, `.tsv`). They need **GTK 3** and the interchange libraries listed above. If `cmake ..` fails in `setup/` or `tests/macros`, install the packages and re-run CMake.

**Reviewer-oriented docs:** [**`tests/macros/README.md`**](tests/macros/README.md) (fixtures and what each CTest covers), [**`tests/macros/TEST_PLAN_macro_interchange_io.md`**](tests/macros/TEST_PLAN_macro_interchange_io.md) (manual GTK checks + automated mapping), and [**`tests/instructions/README.md`**](tests/instructions/README.md) (how to run a single test).

**Per-test instructions** (what each executable checks, how to run it) live under **`tests/instructions/`**, with one markdown file paired to each main test source for easier code review.

### Manual QA: macro interchange (round-trip and cross-platform)

Automated tests only assert that **non-empty** tables load from small fixtures. For release-quality review, follow the **step-by-step checklist** in [**`tests/macros/TEST_PLAN_macro_interchange_io.md` — § Manual and round-trip QA**](tests/macros/TEST_PLAN_macro_interchange_io.md#manual-and-round-trip-qa-humans-and-reviewers):

- Start from **[`tests/macros/unikey_macros.txt`](tests/macros/unikey_macros.txt)** (large Vietnamese sample): **clear all macros**, import, then type triggers in **IBus Unikey** in a **terminal**, **GUI text editor**, and **web** fields to confirm expansions and **tonal/orthography** (accents).
- **Export** (e.g. plist, YAML, JSON, CSV/TSV) and **re-import** into a **clean** table to confirm **round-trip** parity for counts and spot-checked triggers.
- **macOS:** if available, export plist from Linux setup and **import into macOS Text Replacements**, or export on Mac and **import on Linux** (see the test plan for caveats).
- Always **reset/clear** the macro table (or use a throwaway user profile) before each import-focused test so results are not masked by old rows.

### Recent developer-facing changes (testing)

- **`tests/CMakeLists.txt`** now includes **`tests/macros/`** so macro-related tests are registered in CTest when dependencies are available.
- **Macro file I/O tests** use fixture files from **`tests/macros/`** (the path passed to the test matches that directory in the build).
- **Instruction markdown** was added under **`tests/instructions/`** for discoverability; extend that folder when you add new test sources.
- CMake already enables testing at the top level; the generated **`test`** target runs CTest — do not duplicate a custom `test` target in `CMakeLists.txt` (that name is reserved).

For larger design work (e.g. macro storage and cache), see [`project_planning/`](project_planning/) if present in your checkout.

