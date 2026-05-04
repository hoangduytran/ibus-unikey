# ibus-unikey Technical Note

This note is intended to match the current source tree in this repository as of **May 2026** (macro I/O, binary cache, **setup interchange codecs** JSON/YAML/plist/CSV/TSV, and refactored **engine / charset / setup** module directories).

If this note, older screenshots, or user-facing help text disagree with the implementation, the code wins. In a few places the shipped help text is stale; those mismatches are called out explicitly below.

## Language

- [English](#table-of-contents)
- [Tiếng Việt](#tom-tat-tieng-viet)

## Table of Contents

- [1. Scope](#1-scope)
- [2. Verified Architecture](#2-verified-architecture)
- [3. Raw Key Flow from IBus to Final Text](#3-raw-key-flow-from-ibus-to-final-text)
- [4. Input Methods: What Exists vs What Is Actually Reachable](#4-input-methods-what-exists-vs-what-is-actually-reachable)
- [5. Output Charsets, Font Responsibility, and the Real Mediators](#5-output-charsets-font-responsibility-and-the-real-mediators)
- [6. Setup Application and Configuration Mediation](#6-setup-application-and-configuration-mediation)
- [7. Macro System](#7-macro-system)
- [8. Verified Corrections Against Older Documentation](#8-verified-corrections-against-older-documentation)
- [9. Current Limitations and Missing Features](#9-current-limitations-and-missing-features)
- [10. Suggestions](#10-suggestions)
- [11. Source Map](#11-source-map)
  - [11.1 Refactored module directories (May 2026)](#111-refactored-module-directories-may-2026)
    - [11.1.1 Charset breakup](#1111-charset-breakup-mappingcharset_components)
    - [11.1.2 Engine breakup](#1112-engine-breakup-engineengine_components)
    - [11.1.3 Include paths for the split layout](#1113-include-paths-for-the-split-layout)
- [12. Tóm tắt tiếng Việt](#12-tom-tat-tieng-viet)

## 1. Scope

`ibus-unikey` is an IBus input method engine that embeds a modified UniKey engine and exposes Vietnamese text composition to Linux desktop applications.

At a high level, the current code does these jobs:

1. Receives IBus key events.
2. Interprets logical key symbols according to the selected Vietnamese input method.
3. Maintains an internal Vietnamese composition state.
4. Converts that internal representation into the selected output charset.
5. Sends preedit and committed text back through IBus.

The project does **not** do these jobs:

- It does not manage the physical keyboard layout in the XKB sense.
- It does not choose fonts.
- It does not render glyphs.
- It does not produce a special final keycode for fonts.

The engine outputs text bytes or Unicode text. Font selection and glyph rendering are handled later by the application, toolkit, and system font stack.

## 2. Verified Architecture

The implementation is easiest to understand as five cooperating layers.

### 2.1 IBus integration layer

- `src/engine.cpp`
- `src/engine_private.h`
- `src/engine/engine_app.cpp`

This layer receives IBus events, owns the active preedit string, decides when to commit, and forwards printable input into the UniKey runtime.

### 2.2 Runtime configuration layer

- `src/config/unikey_config.h`
- `src/config/unikey_config.cpp`
- `src/config/org.freedesktop.ibus.engine.unikey.gschema.xml`

This layer maps persisted GSettings strings and booleans into runtime input-method IDs, output-charset IDs, and UniKey option fields.

### 2.3 UniKey wrapper layer

- `ukengine/engine/unikey.cpp`

This is the public runtime bridge between ibus-unikey and the UniKey core. It exposes functions such as:

- `UnikeySetup()`
- `UnikeyFilter()`
- `UnikeySetInputMethod()`
- `UnikeySetOutputCharset()`
- `UnikeySetOptions()`
- `UnikeyLoadMacroTable()`
- `UnikeyRestoreKeyStrokes()`

It also owns global output buffers such as `UnikeyBuf`, `UnikeyBufChars`, and `UnikeyBackspaces`.

### 2.4 Core composition and classification layer

- `ukengine/core/inputproc.cpp`
- `include/ukengine/core/inputproc.h`
- `ukengine/engine/engine_components/` and `ukengine/engine/engine_components/table_components/` (split compilation units for `UkEngine`, syllable tables, and helpers; see [§11.1](#111-refactored-module-directories-may-2026))
- `include/ukengine/engine/ukengine.h`
- `include/ukengine/mapping/vnlexi.h`

This layer classifies keys into semantic Vietnamese events, tracks the current word, places tones and vowel marks, performs undo and restoration, and decides whether a macro should expand.

### 2.5 Charset conversion and macro persistence layer

- `ukengine/mapping/charset_components/` (split `.cpp` units behind `VnConvert` and related; see [§11.1](#111-refactored-module-directories-may-2026))
- `include/ukengine/mapping/charset.h`
- `include/ukengine/mapping/vnconv.h`
- `include/ukengine/mapping/macro_format.h` (abstract `MacroFormat`)
- `include/ukengine/mapping/text_macro_format.h` / `ukengine/mapping/text_macro_format.cpp` (`TextMacroFormat`, UniKey TEXT import/export)
- `include/ukengine/mapping/macro_cache.h` / `ukengine/mapping/macro_cache.cpp` (`CacheManagement`, binary sidecar)
- `ukengine/mapping/mactab.cpp`
- `include/ukengine/mapping/mactab.h`
- `include/ukengine/mapping/keycons.h`

This layer converts the engine’s internal Vietnamese representation to the selected output charset and stores macro tables in memory and on disk.

### 2.6 Setup application layer

- `setup/controller/setup_controller.h` and `setup/controller/setup_controller_components/` (split controller implementation; see [§11.1](#111-refactored-module-directories-may-2026))
- `setup/config/settings_store.cpp`
- `setup/macro_utils.cpp`
- `setup/ui/main_window.ui`
- `setup/ui/macro_dialog.ui`

This layer is a GTK application for editing settings and macros. It does not talk directly to the running engine process; it writes to GSettings and the engine reloads from there.

## 3. Raw Key Flow from IBus to Final Text

The verified runtime path is shown below.

```text
IBus key event
  (keyval, keycode, modifiers)
            |
            v
src/engine.cpp
  ibus_unikey_engine_process_key_event()
            |
            v
src/engine.cpp
  ibus_unikey_engine_process_key_event_preedit()
            |
            |-- control/navigation/keypad -> commit preedit, let app handle key
            |-- backspace -> UnikeyBackspacePress()
            |-- printable ASCII / Shift paths -> UnikeyFilter() or restore
            v
ukengine/engine/unikey.cpp
  UnikeyFilter()
            |
            v
ukengine/engine/engine_components/
  UkEngine::process() in process_io.cpp; composition in append, word, state,
  macro, restore, setup, diacritic_processing; syllable tables in table_components/
            |
            v
ukengine/core/inputproc.cpp
  classify key under active input method
            |
            v
(same engine_components/)
  update composition buffer / macro match / restore / tone placement
            |
            v
ukengine/mapping/charset_components/
  convert internal representation to selected output charset (VnConvert, etc.)
            |
            v
src/engine.cpp
  update preedit, apply backspaces, commit when needed
            |
            v
application receives text
            |
            v
toolkit + font stack render glyphs
```

### 3.1 What IBus actually provides

The IBus callback receives three values:

- `keyval`: logical key symbol
- `keycode`: physical keycode or scancode-like value from the platform
- `modifiers`: Shift, Caps Lock, Ctrl, Alt, and related flags

In the current implementation, the text-processing path primarily uses `keyval` and `modifiers`. The physical `keycode` is passed into the callback but is not the main driver of Vietnamese composition.

This means ibus-unikey is working from logical keys after the desktop keyboard layout has already been resolved by XKB and IBus.

### 3.2 What the IBus layer handles directly

Before forwarding input into the UniKey core, `src/engine.cpp` handles several cases itself:

- key release events are ignored
- Ctrl, Alt, Tab, Return, Delete, navigation keys, and keypad navigation commit preedit and fall through
- Backspace calls `UnikeyBackspacePress()`
- plain printable characters are sent to `UnikeyFilter()`
- Shift+Space and Shift+Shift call `UnikeyRestoreKeyStrokes()`
- for Telex and STelex2, a standalone leading `w` may be passed through directly depending on the `standalone-w-as-uw` option

### 3.3 Word boundaries and commit behavior

The IBus layer keeps a UTF-8 preedit string in memory. After UniKey processes a key, the IBus layer:

1. removes characters according to `UnikeyBackspaces`
2. appends newly generated output bytes
3. converts legacy output back to UTF-8 for preedit display when needed
4. commits the preedit string when a word-break symbol is reached

Word-break symbols are hard-coded in `src/engine.cpp` and include punctuation, operators, whitespace, brackets, and similar separators.

## 4. Input Methods: What Exists vs What Is Actually Reachable

This is one of the most important distinctions in the codebase.

### 4.1 Physical keyboard layout vs Vietnamese input method

These are different concerns:

- Physical layout is handled by XKB and IBus before ibus-unikey sees the event.
- Vietnamese input method is handled by UniKey after ibus-unikey receives `keyval`.

So a user may physically type on US, UK, or another keyboard layout, but the Vietnamese composition method is still separately selected inside ibus-unikey.

### 4.2 Methods exposed by the current setup UI and GSettings schema

The shipped setup/config path exposes exactly four input methods:

| GSettings value | Runtime enum | UI label |
|---|---|---|
| `telex` | `UkTelex` | Extend Telex |
| `vni` | `UkVni` | VNI |
| `stelex` | `UkSimpleTelex` | STelex |
| `stelex2` | `UkSimpleTelex2` | STelex 2 |

Those mappings are defined in `src/config/unikey_config.h` and `src/config/org.freedesktop.ibus.engine.unikey.gschema.xml`.

### 4.3 Mapping tables that exist lower in the core

`ukengine/core/inputproc.cpp` contains built-in mapping tables for:

- Telex
- Simple Telex
- Simple Telex 2
- VNI
- VIQR
- Microsoft Vietnamese (`MsVi`)

It also supports a user-loaded custom keymap (`UkUsrIM`).

### 4.4 What is and is not reachable through the current ibus-unikey runtime path

`UkInputProcessor::setIM()` can switch to `UkViqr` and `UkMsVi` internally.

However, the ibus-facing wrapper function `UnikeySetInputMethod()` in `ukengine/engine/unikey.cpp` currently accepts only:

- `UkTelex`
- `UkVni`
- `UkSimpleTelex`
- `UkSimpleTelex2`
- `UkUsrIM` when a user keymap has already been loaded

So the practical current-state conclusion is:

- Telex, VNI, STelex, and STelex2 are fully reachable from the setup UI and runtime wrapper.
- A user keymap is supported by the lower runtime, but there is no shipped GSettings/UI path selecting it.
- VIQR and MsVi mapping tables exist in the core classifier, but they are not selectable through the current ibus-unikey wrapper/config path.

That is more precise than simply saying the engine supports VIQR. The tables exist, but the shipped runtime path does not expose them as a selectable IBus mode.

### 4.5 Examples of current built-in method behavior

Verified from `ukengine/core/inputproc.cpp`:

- Telex: `s f r x j` for tones, `aa ee oo` for roofs, `w` for `ă/ơ/ư`, `dd` for `đ`, `z` to cancel
- VNI: `1..5` for tones, `6..9` for vowel or `đ` changes, `0` to cancel
- VIQR table exists internally: `'`, `` ` ``, `?`, `~`, `.`, `^`, `+`, `*`, `(`, `D`, `\`
- MsVi table exists internally with its own compatibility key choices

#### Input-to-Output Flow Example (English)

**Example: Typing "a1" in VNI mode to get "á"**

1. **Key Event**: User presses 'a'. IBus sends keyval for 'a' to ibus-unikey.
2. **Classification**: Engine classifies 'a' as a base letter, adds it to the composition buffer.
3. **Key Event**: User presses '1'. IBus sends keyval for '1'.
4. **VNI Mapping**: Engine looks up '1' in `VniMethodMapping[]`, finds it means "sắc" (acute tone).
5. **Composition Update**: Engine applies the tone to the last vowel in the buffer, using `vnlexi` rules, changing 'a' to 'á'.
6. **Charset Conversion**: On commit (e.g., space pressed), engine converts the internal symbol to the selected output charset (e.g., Unicode).
7. **Output**: UTF-8 for "á" is sent to the application via IBus.
8. **Font Rendering**: The application displays "á" using the system font.

**Example: Typing "o6" in VNI mode to get "ô"**

1. User presses 'o' → classified as base letter.
2. User presses '6' → mapped to "roof" diacritic in VNI.
3. Engine updates buffer: 'o' → 'ô'.
4. On commit, 'ô' is encoded and sent to the application.

---

#### Ví dụ luồng xử lý phím (Tiếng Việt)

**Ví dụ: Gõ "a1" trong chế độ VNI để ra "á"**

1. **Sự kiện phím**: Người dùng nhấn 'a'. IBus gửi keyval 'a' vào ibus-unikey.
2. **Phân loại**: Engine nhận diện 'a' là chữ cái cơ bản, thêm vào bộ đệm soạn thảo.
3. **Sự kiện phím**: Người dùng nhấn '1'. IBus gửi keyval '1'.
4. **Ánh xạ VNI**: Engine tra '1' trong `VniMethodMapping[]`, xác định là dấu sắc.
5. **Cập nhật soạn thảo**: Engine áp dụng dấu sắc lên nguyên âm cuối trong bộ đệm, dùng quy tắc `vnlexi`, đổi 'a' thành 'á'.
6. **Chuyển mã**: Khi commit (ví dụ nhấn space), engine chuyển ký hiệu nội bộ thành bảng mã đầu ra (ví dụ Unicode).
7. **Xuất ra**: UTF-8 cho "á" được gửi tới ứng dụng qua IBus.
8. **Hiển thị font**: Ứng dụng hiển thị "á" bằng font hệ thống.

**Ví dụ: Gõ "o6" trong VNI để ra "ô"**

1. Nhấn 'o' → nhận diện là chữ cái cơ bản.
2. Nhấn '6' → ánh xạ thành dấu mũ trong VNI.
3. Engine cập nhật bộ đệm: 'o' → 'ô'.
4. Khi commit, 'ô' được mã hóa và gửi tới ứng dụng.

## 5. Output Charsets, Font Responsibility, and the Real Mediators

The user asked what mediates the transformation from key input to the final displayable Vietnamese character. In the current code there are several mediators, not a single one.

### 5.1 The actual mediator chain

The verified chain is:

1. `keyval` and `modifiers` from IBus
2. `UkKeyEvent` classification in `inputproc`
3. composition state in `UkEngine::WordInfo` and related engine state
4. canonical Vietnamese symbol naming through `VnLexiName`
5. internal standard Vietnamese character representation (`StdVnChar`)
6. charset conversion through `VnConvert()` and charset objects
7. UTF-8 preedit display or legacy-byte commit output

So the decisive mediator between typed key and final rendered character is not a font or final keycode. It is the composition pipeline plus the charset-conversion layer.

### 5.2 Output charsets exposed today

The schema currently exposes eight output charsets:

1. `unicode` -> `CONV_CHARSET_XUTF8`
2. `tcvn3` -> `CONV_CHARSET_TCVN3`
3. `vni-win` -> `CONV_CHARSET_VNIWIN`
4. `viqr` -> `CONV_CHARSET_VIQR`
5. `bk-hcm2` -> `CONV_CHARSET_BKHCM2`
6. `cstr` -> `CONV_CHARSET_UNI_CSTRING`
7. `ncr-dec` -> `CONV_CHARSET_UNIREF`
8. `ncr-hex` -> `CONV_CHARSET_UNIREF_HEX`

### 5.3 How conversion works

The composition engine does not directly build UTF-8 characters for every internal step. Instead it works with standardized Vietnamese symbols and then converts them near the output boundary.

`UkEngine::writeOutput()` and the macro path in `UkEngine::macroMatch()` convert `StdVnChar` sequences into the target charset by calling `VnConvert()`.

### 5.4 What fonts do and do not do here

The engine never chooses a font. The font stack only renders whatever text encoding the application ultimately receives.

Practical consequence:

- Unicode output normally renders correctly with modern fonts.
- Legacy output such as TCVN3 or VNI-Win only looks correct if the receiving application and font choice match that legacy encoding.

### 5.5 Why preedit can still look correct when legacy output is selected

When the selected output charset is not UTF-8, `src/engine.cpp` converts the generated legacy bytes back into UTF-8 through `latinToUtf()` for the IBus preedit display.

That is only a display adaptation for the Linux UI. It does not mean the selected output charset has been changed internally.

## 6. Setup Application and Configuration Mediation

There is no direct RPC, socket, D-Bus method, or shared controller object connecting the setup window to the active engine instance.

The real mediator is **GSettings**.

### 6.1 Verified settings flow

```text
GTK setup UI
  |
  v
SetupController
  |
  v
SettingsStore / ibus_unikey_config_set_*
  |
  v
GSettings schema: org.freedesktop.ibus.engine.unikey
  |
  v
engine change callback: ibus_unikey_config_value_changed()
  |
  v
ibus_unikey_engine_load_config()
  |
  +--> UnikeySetInputMethod()
  +--> UnikeySetOutputCharset()
  +--> UnikeySetOptions()
  |
  v
running engine reflects new settings
```

### 6.2 Setup-side code path

On the setup side:

- `SetupController::handleInputMethodChanged()` writes `input-method`
- `SetupController::handleOutputCharsetChanged()` writes `output-charset`
- `SetupController::handleSettingToggled()` writes booleans such as spell check and macro-enabled

The helper `SettingsStore` is a small wrapper over `ibus_unikey_config_set_string()` and `ibus_unikey_config_set_boolean()`.

### 6.3 Engine-side reload path

On the engine side:

- `ibus_unikey_init()` registers `ibus_unikey_config_value_changed()`
- that callback calls `ibus_unikey_engine_load_config()`
- the loaded values are pushed into the UniKey runtime through `UnikeySetInputMethod()`, `UnikeySetOutputCharset()`, and `UnikeySetOptions()`

### 6.4 Macro file path

The macro file path is currently:

- `$HOME/.ibus/unikey/macro`

This is defined by `UNIKEY_MACRO_FILE` in `src/config/unikey_config.h`.

## 7. Macro System

The macro subsystem is real and persistent. **TEXT files on disk remain human-readable** (`key:value` lines with a version header). **Binary hashing and fingerprints exist only in an optional sidecar** managed separately from TEXT export.

### 7.1 Modular I/O (`MacroFormat`, `TextMacroFormat`, `CacheManagement`)

Responsibilities are split to match the planning doc (`project_planning/ukengine/…macros_unbounded_keys…`):

| Component | Role |
|-----------|------|
| **`MacroFormat`** (`macro_format.h`) | Abstract import/export by path inside **ukengine**; **`TextMacroFormat`** implements UniKey TEXT. The **setup** app adds separate **`MacroFormatHandler`** codecs for JSON/YAML/plist/CSV/TSV (§7.8). |
| **`TextMacroFormat`** | UniKey’s legacy TEXT codec: optional UTF-8 BOM / header, `key:text` rows, **last-wins** when the same folded ASCII key prefix appears on multiple lines, unbounded line reads, UTF-8 vs VIQR based on header `version=`. **Export writes only text** (no binary, no digest lines). |
| **`CacheManagement`** | **Precomputed binary cache** for the **canonical** macro file path: **sidecar file** `"{macroTextPath}.ukmcache"` in the same directory, **FNV-1a 64** over the **raw bytes** of the TEXT file for invalidation, **atomic** write via a `*.tmp` in that directory then rename. **Import from an arbitrary user path** should still parse TEXT (or another `MacroFormat`); trusting a sidecar for non-canonical paths is a future, explicit feature. |
| **`CMacroTable`** | In-memory `MacroEntry` rows (heap-backed `std::vector<StdVnChar>` key and text), **`std::unordered_map<std::string, size_t>`** (`m_lookup`) on **folded** key bytes — **average O(1)** lookup (**last insert wins** for the same fold). This **replaces** the older UniKey pattern of keeping triggers in sorted order and using **binary search** (`bsearch`) on each lookup. |

**Canonical load/save orchestration** (see `CMacroTable::loadFromFile` / `writeToFile` in `mactab.cpp`):

1. **Load:** `CacheManagement::tryLoad` → if fingerprint matches, hydrate table + rebuild map; else **`TextMacroFormat::importFromPath`** parses TEXT; if file was legacy VIQR, rewrite UTF-8 via `writeToFile`; if already UTF-8, **`CacheManagement::persist`** refreshes the sidecar.
2. **Save:** **`TextMacroFormat::exportToPath`** writes TEXT only, then **`CacheManagement::persist`**.

### 7.2 TEXT storage format and on-disk layout

`TextMacroFormat::exportToPath` writes the header then **`key:text` rows in UTF-8**, **sorted by key** using the same folded-order comparison used historically (VNSTANDARD sequence compare with tone folding). Export does **not** emit cache metadata.

Header example (non-Windows; Windows build may prepend a UTF-8 BOM in the header line):

```text
DO NOT DELETE THIS LINE*** version=1 ***
```

Loading (`importFromPath`): reads header; **`version=1`** ⇒ UTF-8 lines; otherwise treats body as VIQR and **`writeToFile` upgrades** the file to UTF-8 on successful load.

### 7.3 Lookup behavior

Macro lookup is not a raw on-screen byte comparison.

`UkEngine::macroMatch()`:

1. scans backward in the current composition buffer (only positions already in `UkEngine`’s working buffer; capacity **`MAX_UK_ENGINE`** word slots in `include/ukengine/engine/ukengine.h`)
2. builds a standardized Vietnamese key sequence (`StdVnChar`)
3. calls **`CMacroTable::lookup()`**, which maps the same **folded** key bytes as `foldedLookupKeyBytes()` in `mactab.cpp` into an index via **`m_lookup`** (`std::unordered_map`), then returns that row’s replacement text. **Runtime lookup is hash-based**, not a binary search over a sorted table.

**Historical note:** Classic UniKey stored macro triggers in **sort order** and resolved expansion with **binary search**. This tree uses a **hash map** keyed by serialized folded `StdVnChar` bytes so lookup does not depend on maintaining a sorted in-memory array. **On-disk TEXT export** still emits rows **sorted by key** (`TextMacroFormat::exportToPath`) for stable, human-readable files; that sort is for the file format only, not for engine lookup.

### 7.4 Case behavior

Current behavior is:

- all-lowercase trigger -> lowercase replacement
- all-uppercase trigger -> uppercase replacement
- mixed-case trigger -> stored replacement is preserved

### 7.5 Trigger separator preservation

After macro text is emitted, the engine appends the triggering separator character when space or enter caused the expansion. This is why a macro can expand and still keep the final space or line break.

### 7.6 Runtime policy vs storage

- **`CMacroTable`:** row count, key length, and replacement text are **heap-backed**; there is **no** remaining UniKey-era hard cap such as 1024 rows or 16-byte keys. Failures are the usual ones (out of memory, conversion errors).
- **Typing path:** `UkEngine::macroMatch()` only considers triggers that fit in the **live composition buffer** (`m_buffer`, size **`MAX_UK_ENGINE`**). There is **no** additional macro-specific length `#define`; stored keys may be longer, but they cannot match until a suffix of that length is actually present in the buffer.
- The setup UI may still impose its own GTK buffer or validation limits independent of the engine.

### 7.7 Macro editor capabilities that exist today

The current GTK macro dialog supports:

- add/edit/delete
- clear all
- import from file and export to file, including **UniKey TEXT** (`.txt` / `.macro`) and **JSON, YAML, plist, CSV, TSV** via the interchange layer (§7.8; format from extension when AUTO)
- duplicate macro keys: **last wins** (case-folded) when loading/saving and when merging the engine table into the list store; the cell editor no longer blocks duplicate keys
- incremental tree-view search behavior on column 0 because `search_column` is set

### 7.8 Setup macro interchange: extra file formats and handler wiring

The **engine’s canonical macro file** on disk (path from `UNIKEY_MACRO_FILE` in `src/config/unikey_config.h`) is still loaded and saved through **`TextMacroFormat`** plus optional **`.ukmcache`** as described in §7.1. That path is what the running IBus engine reloads from GSettings.

The **GTK setup app** additionally supports **importing and exporting** the same logical `CMacroTable` through **pluggable interchange codecs** that live only in `setup/`. Those codecs are **not** used for automatic engine reload of the default macro path unless the user imports into the editor and saves native text (or the flow explicitly writes the canonical file).

**Dispatch API**

- `macro_interchange_import_path()` / `macro_interchange_export_path()` in `setup/macro_file_io.cpp` clear or serialize the table, resolve a `MacroInterchangeForcedFormat`, construct a `MacroFormatHandler`, and call `import_from_path` / `export_to_path`.
- With `MACRO_INTERCHANGE_FORMAT_AUTO`, the format is chosen by **`MacroFormatHandlerRegistry::detect_from_path()`** (`setup/macro_format_handler_registry.cpp`): lowercase file suffix after the final dot; **empty or unknown suffix ⇒ UniKey TEXT** (`MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY`).

**How handlers register**

- Each codec is implemented by a subclass of `MacroFormatHandler` (`include/setup/macro_format_handler.h`) in its own `setup/macro_handler_*.cpp`.
- A **static registrar** struct in that file constructs at startup and calls **`MacroFormatHandlerRegistry::register_handler(format, factory, { ".ext", … })`**, binding the enum to a factory lambda and registering **AUTO** suffix hints (include the leading dot, lowercase).
- The registry keeps a map **format → factory** and **suffix → format** under a mutex. **Last registration wins** for a given format id; duplicate suffix registration across handlers should be avoided.
- `macro_interchange` is built as a **static** library (`setup/CMakeLists.txt`). The setup executable must link it with **`-Wl,--whole-archive` / `-Wl,--no-whole-archive` on GNU ld** or **`-force_load` on macOS** so **every object file is linked** and all static initializers run before `main` (otherwise a handler can be “missing” at runtime).

**Formats and extensions (current)**

| `MacroInterchangeForcedFormat` | AUTO suffixes (typical) | Source file | Dependencies (pkg-config / CMake) |
|--------------------------------|-------------------------|-------------|-----------------------------------|
| `MACRO_INTERCHANGE_FORMAT_TEXT_UNIKEY` | `.txt`, `.macro` | `setup/macro_handler_text.cpp` | Same TEXT semantics as §7.2 (wrapper around ukengine `MacroFormat` / path I/O) |
| `MACRO_INTERCHANGE_FORMAT_YAML` | `.yaml`, `.yml` | `setup/macro_handler_yaml.cpp` | yaml-cpp; Espanso-style `matches:` sequence |
| `MACRO_INTERCHANGE_FORMAT_JSON` | `.json` | `setup/macro_handler_json.cpp` | json-glib |
| `MACRO_INTERCHANGE_FORMAT_PLIST` | `.plist` | `setup/macro_handler_plist.cpp` | libplist |
| `MACRO_INTERCHANGE_FORMAT_CSV` | `.csv` | `setup/macro_handler_csv.cpp` | libcsv; comma-separated, quoted fields |
| `MACRO_INTERCHANGE_FORMAT_TSV` | `.tsv` | `setup/macro_handler_csv.cpp` | tab-delimited variant (`DelimitedTextMacroHandler`) |

**UI:** GTK file chooser filters listing these patterns are attached by `macro_file_chooser_attach_import_filters` / `macro_file_chooser_attach_export_filters` in `setup/macro_file_io.cpp`. Import/export from the macro dialog is wired through **`setup/controller/setup_controller_components/setup_controller_macro_io.cpp`** (temporary `CMacroTable`, AUTO dispatch, then merge or save).

### 7.9 Macro editor capabilities that do not exist as a finished user-facing feature

The current code does **not** provide:

- a dedicated search box
- filter or search by replacement value
- explicit sortable columns wired to a sort model

There is one nuance here:

- **TEXT export** writes rows in a **stable sorted key order** (file-format convention); **in-memory lookup** uses a **hash map**, not sorting + `bsearch`
- the UI layer does not expose a real sorting feature for the user
- **structured import/export** (JSON, YAML, plist, CSV, TSV) **is** available from the setup app (§7.8); remaining gaps are mostly **UX polish** and **formal documented schemas** for third-party tools

Those are different things and should not be conflated.

## 8. Verified Corrections Against Older Documentation

This section records the main corrections that were necessary when comparing the repository with the previous technical note and embedded help text.

### 8.1 Supported by core is not the same as selectable in current ibus-unikey

The previous note correctly observed that VIQR and MsVi mappings exist in the core. The stricter, fully accurate statement is:

- VIQR and MsVi mapping tables exist in `UkInputProcessor`
- but the current ibus-facing wrapper and GSettings path do not expose them as selectable modes

### 8.2 Output charset count in help text is stale

`src/engine/engine_app.cpp` still says there are 7 output charsets.

The current schema and config map expose 8 output charsets because `bk-hcm2` is included.

### 8.3 The engine transforms text, not font-specific keycodes

The project does not compute a final keycode for a font. The current implementation transforms input into text bytes or Unicode and relies on external rendering infrastructure for glyph display.

### 8.4 The real setup/runtime mediator is GSettings

The running engine is not directly configured by the setup dialog through a custom controller channel. The mediator is GSettings plus the engine’s change callback.

## 9. Current Limitations and Missing Features

The current repository has the following verified limitations or gaps.

### 9.1 Input-method exposure gap

- Only Telex, VNI, STelex, and STelex2 are selectable through the shipped setup/config path.
- VIQR and MsVi mappings exist in code but are not exposed through the current wrapper/config path.
- User-defined keymaps can be loaded in the lower runtime but are not exposed in the shipped setup UI or schema as a normal selectable mode.

### 9.2 Macro limits and UX gaps

- Macro tables are **not** capped at legacy fixed sizes; practical limits are **memory** and file size when loading/saving.
- While typing, macro expansion is still limited by the engine composition buffer width (**`MAX_UK_ENGINE`**), not by a separate macro-only constant.
- Macro editing is table-based only.
- There is no dedicated searchable or filterable macro browser.
- There is no real user-facing sorting feature in the macro dialog.

### 9.3 Interchange formats: engine vs setup

- **Engine canonical path:** load/save for `UNIKEY_MACRO_FILE` remains **UniKey TEXT** plus optional **`.ukmcache`** (§7.1). The IBus engine does not read JSON/YAML/etc. directly off that path.
- **Setup application:** the macro dialog can **import and export** **`CMacroTable`** through **JSON, YAML, Apple plist (text substitutions), CSV, and TSV** in addition to native TEXT, using **`macro_interchange_*_path`** and registered handlers (§7.8).
- **Remaining gap:** there is no published, versioned interchange schema document for external tool authors; each codec follows conventions implemented in `setup/macro_handler_*.cpp`.

### 9.4 Rendering boundary limitation

- Legacy output looks correct only when the receiving application and font selection are compatible with that charset.
- ibus-unikey cannot enforce that compatibility from inside the engine.

### 9.5 Documentation or code drift still present elsewhere

- The embedded long description in `src/engine/engine_app.cpp` still mentions 7 output charsets.
- The setup layer currently uses the config API directly in several places even though a `SettingsStore` wrapper exists.

## 10. Suggestions

These are recommendations, not current-state claims.

### 10.1 Make method exposure consistent

Pick one of these directions:

1. expose VIQR and MsVi through GSettings and the setup UI
2. or explicitly document them as internal-only tables and keep them unreachable from the public path

### 10.2 Harden macro interoperability

Structured interchange (**JSON, YAML, plist, CSV, TSV**) already exists in the setup app (§7.8). Worth doing next:

- publish a short **versioned schema** or examples per format so other tools can rely on stable field names and shapes
- **round-trip tests** (import → export → compare) per codec
- explicit documentation of **case rules** and **escape semantics** where they differ from UniKey TEXT

External macro ecosystems still differ in trigger matching and encoding; treat interchange as a **conversion** surface, not guaranteed identity with third-party editors.

### 10.3 Improve macro UX

Useful additions would be:

- visible search box
- filtering by key and replacement text
- actual sortable columns
- overflow warnings when approaching macro limits

### 10.4 Fix documentation drift in code

Update the engine descriptor description in `src/engine/engine_app.cpp` so it matches the actual eight output charsets and the current behavior of STelex2 and standalone `w` handling.

### 10.5 Add regression tests around configuration and macros

The most valuable focused tests would be:

- input-method reachability tests
- output-charset coverage tests
- macro import and export round-trip tests
- macro-limit boundary tests
- setup-to-engine config propagation tests

## 11. Source Map

These files are the most important entry points when tracing the current implementation.

**Build boundary:** The static library `libukengine` is defined in `ukengine/CMakeLists.txt`. The former monolithic translation units **`ukengine/mapping/charset.cpp`** and **`ukengine/engine/ukengine.cpp`** are **not** in the tree; CMake compiles the split modules listed below instead. **`ukengine/engine/unikey.cpp`** remains the IBus-facing C API bridge and is its own TU.

- IBus key processing and preedit handling: `src/engine.cpp`
- Engine application metadata and stale help text: `src/engine/engine_app.cpp`
- Config key maps and macro path: `src/config/unikey_config.h`
- GSettings schema: `src/config/org.freedesktop.ibus.engine.unikey.gschema.xml`
- Setup controller: `setup/controller/setup_controller.h`, `setup/controller/setup_controller_components/*.cpp`
- Setup settings wrapper: `setup/config/settings_store.cpp`
- Setup macro helpers: `setup/macro_utils.cpp`
- Macro dialog UI: `setup/ui/macro_dialog.ui`
- UniKey wrapper/runtime bridge: `ukengine/engine/unikey.cpp`
- Core composition engine: `ukengine/engine/engine_components/*.cpp`, `ukengine/engine/engine_components/table_components/*.cpp` (main entry `UkEngine::process` in `process_io.cpp`)
- Input method mapping tables: `ukengine/core/inputproc.cpp`
- Input event definitions: `include/ukengine/core/inputproc.h`
- Vietnamese symbolic vocabulary: `include/ukengine/mapping/vnlexi.h`
- Charset conversion layer: `ukengine/mapping/charset_components/*.cpp`
- Macro TEXT codec (engine / native table I/O): `ukengine/mapping/text_macro_format.cpp`
- Macro binary cache: `ukengine/mapping/macro_cache.cpp`
- Macro persistence and lookup: `ukengine/mapping/mactab.cpp`
- Shared limits and enums: `include/ukengine/mapping/keycons.h`
- **Setup macro interchange (import/export codecs):** `setup/macro_file_io.cpp`, `setup/macro_format_handler_registry.cpp`, `setup/macro_interchange_common.cpp`, `setup/macro_handler_text.cpp`, `setup/macro_handler_json.cpp`, `setup/macro_handler_yaml.cpp`, `setup/macro_handler_plist.cpp`, `setup/macro_handler_csv.cpp`

### 11.1 Refactored module directories (May 2026)

Several former single-file modules are now **split across multiple `.cpp` translation units** while keeping the same public headers and link boundary (`libukengine.a` / setup executable). For **ukengine**, CMake **enumerates each `.cpp` explicitly** in `ukengine/CMakeLists.txt` (no glob); **`setup/CMakeLists.txt`** does the same for setup splits.

#### 11.1.1 Charset breakup (`mapping/charset_components/`)

- **Replaces:** `ukengine/mapping/charset.cpp` (removed).
- **Now:** nine sources — `charset_base.cpp`, `charset_doublebyte.cpp`, `charset_globals.cpp`, `charset_library.cpp`, `charset_stdvn.cpp`, `charset_unicode.cpp`, `charset_utf8_viqr.cpp`, `charset_viqr.cpp`, `charset_wincp1258.cpp` — plus shared `charset_internal.h` in the same directory.
- **Public API:** unchanged; callers still use `charset.h`, `vnconv.h`, and `VnConvert()`.

#### 11.1.2 Engine breakup (`engine/engine_components/`)

- **Replaces:** `ukengine/engine/ukengine.cpp` (removed).
- **Now:** `UkEngine` body and syllable-table helpers are split across `engine/engine_components/*.cpp` and `engine/engine_components/table_components/*.cpp`; the runtime C wrapper stays in **`engine/unikey.cpp`** (see [§2.3](#23-unikey-wrapper-layer)).
- **Public API:** unchanged; `include/ukengine/engine/ukengine.h` and related headers describe `UkEngine`.

#### 11.1.3 Include paths for the split layout

Sources under `charset_components/` use includes such as `"charset_components/charset_internal.h"`. Sources under `engine_components/` use `"engine_components/engine_internal.h"` (and similar). For those to resolve when each file is compiled from its real path, **`ukengine/CMakeLists.txt` adds** `${CMAKE_CURRENT_SOURCE_DIR}/mapping` and `${CMAKE_CURRENT_SOURCE_DIR}/engine` to **`target_include_directories(ukengine …)`** so the parent of `charset_components/` / `engine_components/` is on the include path.

---

**`ukengine/engine/engine_components/`** — `UkEngine` implementation (non-table logic):

- `process_io.cpp` — `UkEngine::process`, escape/no-spell paths, backspace output
- `append.cpp`, `word.cpp`, `state.cpp`, `macro.cpp`, `restore.cpp`, `setup.cpp`, `diacritic_processing.cpp`
- `engine_internal.h`, `engine_tables_shared.h` — shared between these TUs (not public API)

**`ukengine/engine/engine_components/table_components/`** — syllable-table machinery keyed off `UkEngine`:

- `table_globals.cpp`, `table_init.cpp`, `table_lookup.cpp`, `table_keyproc.cpp`, `table_comparators.cpp`, `table_validators.cpp`, `table_vcpair.cpp`, `table_vseq_cseq.cpp`, `table_internal.h`

**`ukengine/mapping/charset_components/`** — charset conversion behind `VnConvert()` / `charset.h`:

- `charset_globals.cpp`, `charset_base.cpp`, `charset_unicode.cpp`, `charset_doublebyte.cpp`, `charset_viqr.cpp`, `charset_utf8_viqr.cpp`, `charset_library.cpp`, `charset_wincp1258.cpp`, `charset_stdvn.cpp`, `charset_internal.h`

**`setup/controller/setup_controller_components/`** — GTK setup controller (header `setup/controller/setup_controller.h`):

- `setup_controller_helpers.cpp`, `setup_controller_globals.cpp`, `setup_controller_lifecycle.cpp`, `setup_controller_config.cpp`, `setup_controller_macro_io.cpp`, `setup_controller_macro_model.cpp`, `setup_controller_macro_editor.cpp`, `setup_controller_internal.h`

There is **no** longer a monolithic `ukengine/engine/ukengine.cpp`, `ukengine/mapping/charset.cpp`, or `setup/controller/setup_controller.cpp` in the tree. **`ukengine/CMakeLists.txt` must list the replacement `charset_components` and `engine_components` sources** (and the `mapping` / `engine` include dirs above); otherwise configuration will fail with missing `.cpp` errors. Behavior is intended to match the pre-split layout.

## 12. Tóm tắt tiếng Việt

Phần này là tóm tắt ngắn gọn bằng tiếng Việt cho các kết luận quan trọng nhất.

### 12.1 Luồng xử lý phím thực tế

- IBus gửi `keyval`, `keycode`, `modifiers` vào `src/engine.cpp`.
- ibus-unikey chủ yếu dùng `keyval` và `modifiers` để xử lý gõ tiếng Việt.
- XKB và IBus đã xử lý layout bàn phím vật lý trước đó rồi.
- UniKey lõi phân loại phím, cập nhật bộ đệm soạn thảo, rồi chuyển sang bảng mã đầu ra.
- Ứng dụng và font của hệ thống mới là nơi hiển thị glyph cuối cùng.

### 12.2 Kiểu gõ hiện thật sự chọn được

Trong đường đi cấu hình hiện tại, chỉ có 4 kiểu gõ chọn được từ setup hoặc UI:

1. Telex
2. VNI
3. STelex
4. STelex2

VIQR và MsVi có bảng ánh xạ trong lõi, nhưng hiện không được lộ ra như chế độ chọn được qua wrapper hoặc config hiện tại.

### 12.3 Mediator thực sự là gì

Các lớp trung gian chính là:

- `UkKeyEvent`
- trạng thái soạn thảo trong `UkEngine` (mã nguồn tách trong `engine/engine_components/`)
- ký hiệu tiếng Việt chuẩn `VnLexiName`
- biểu diễn chuẩn `StdVnChar`
- lớp chuyển mã `VnConvert()` / các file trong `charset_components/`

Không có bước nào tạo keycode cuối cùng cho font. Engine tạo văn bản, còn font stack hiển thị.

### 12.4 Kết nối giữa setup và engine đang chạy

Mediator giữa setup và engine là **GSettings**, không phải controller trực tiếp.

Luồng là:

`SetupController -> GSettings -> callback trong engine -> UnikeySet*()`

### 12.5 Giới hạn macro hiện tại

- Không còn giới hạn cứng kiểu cũ (ví dụ 1024 macro / độ dài key cố định); bảng macro lưu trên heap; giới hạn thực tế chủ yếu là **bộ nhớ** và kích thước buffer soạn thảo khi đang gõ (**`MAX_UK_ENGINE`** trong `ukengine.h`).
- File TEXT chuẩn có sidecar cache nhị phân tùy chọn `.ukmcache` (mục 7).
- UI import/export: **UniKey TEXT** (`.txt` / `.macro`) và **JSON, YAML, plist, CSV, TSV** (mục 7.8).

UI macro vẫn thiếu:

- ô search riêng
- sort cột đầy đủ cho người dùng
- tài liệu schema chính thức cho từng định dạng trao đổi

### 12.6 Kết luận ngắn

Nếu cần mô tả chính xác nhất về phiên bản hiện tại, cách nói đúng là:

- ibus-unikey nhận logical key từ IBus
- UniKey biến logical key thành trạng thái tiếng Việt nội bộ
- lớp chuyển mã biến trạng thái đó thành bảng mã đầu ra
- ứng dụng và font mới hiển thị chữ Việt cuối cùng
