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
- Tests under **`tests/macros/`** (dialog state, file I/O, export filename helpers) need **GTK 3** and related dev packages (e.g. `libgtk-3-dev` on Debian/Ubuntu) so CMake can find `gtk+-3.0` via pkg-config. If the macro subproject fails to configure, install those packages and re-run `cmake ..`.

**Per-test instructions** (what each executable checks, how to run a single test) live in [**`tests/instructions/README.md`**](tests/instructions/README.md), with one markdown file paired to each main test source for easier code review.

### Recent developer-facing changes (testing)

- **`tests/CMakeLists.txt`** now includes **`tests/macros/`** so macro-related tests are registered in CTest when dependencies are available.
- **Macro file I/O tests** use fixture files from **`tests/macros/`** (the path passed to the test matches that directory in the build).
- **Instruction markdown** was added under **`tests/instructions/`** for discoverability; extend that folder when you add new test sources.
- CMake already enables testing at the top level; the generated **`test`** target runs CTest — do not duplicate a custom `test` target in `CMakeLists.txt` (that name is reserved).

For larger design work (e.g. macro storage and cache), see [`project_planning/`](project_planning/) if present in your checkout.

