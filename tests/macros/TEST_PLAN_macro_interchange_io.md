# Test Plan — Macro interchange import/export I/O

**Design (if present in your checkout):** `project_planning/UI/macros/…macro_format_handlers_io_design.md` — optional design notes; the authoritative behavior is in `setup/macro_handler_*.cpp` and `include/setup/macro_file_io.h`.

## Scope

Automated and manual validation of **filename-based macro interchange** (dispatcher in `setup/macro_file_io.cpp`): UniKey text, Espanso-style YAML (`matches:` / `trigger` / `replace`), Apple XML plist text replacements, generic JSON (array or flat map), RFC-style CSV/TSV — without writing canonical `.ukmcache` sidecars or using `CMacroTable::loadFromFile` / `writeToFile` for non-native extensions.

## Linked automated tests (CTest)

| Test case (g_test path prefix) | Instruction doc | CMake / CTest name |
|--------------------------------|-----------------|-------------------|
| `/ui/macros/file-io/load-*` | [`tests/instructions/macros/macro_file_io_test.md`](../instructions/macros/macro_file_io_test.md) | `macro-file-io-test` |
| Macro chooser remembered directories | [`tests/instructions/macros/macro_dialog_state_test.md`](../instructions/macros/macro_dialog_state_test.md) | `macro-dialog-state-test` |
| Export filename suffix completion | [`tests/instructions/macros/macro_export_filename_test.md`](../instructions/macros/macro_export_filename_test.md) | `macro-export-filename-test` |

Fixtures live beside the sources under [`tests/macros/`](.): `unikey_macros.txt`, `unikey_macro.yaml`, `unikey_macro.plist`, `generic_macros.json`, `macros.csv`, `macros.tsv`.

---

## Manual and round-trip QA (humans and reviewers)

Use this when verifying macro **interchange** beyond what **`macro-file-io-test`** asserts. **IBUS must use your built or installed `ibus-unikey`** with the same macro file the setup tool writes, or engine behavior will not match the table you edited in setup.

### Shared rules

1. **Clear before each import-focused scenario** — In Macro preferences, remove all rows (or delete/replace the user macro file and cache per project docs) so you are not mixing old triggers with the file you are testing.
2. **Confirm in three places** after importing the canonical text fixture:
   - **Terminal** (e.g. GNOME Terminal, Konsole): focus the terminal, ensure Unikey is active, type a known **short** trigger from [`unikey_macros.txt`](unikey_macros.txt) and confirm the **full phrase**, **diacritics**, and **word boundaries** behave as expected.
   - **GUI text editor** (e.g. gedit, Kate, a simple GTK app): same checks; catches focus/IM quirks that terminals sometimes hide.
   - **Another context** (e.g. browser address bar vs search box, or a second editor): optional but useful for IM focus edge cases.
3. **Sample triggers** — The large `unikey_macros.txt` lists thousands of abbreviations; pick a **small written checklist** (e.g. 5–10 rows) mixing **ASCII**, **Vietnamese with full tones**, and **symbols/colons** if present, and tick them off after each import or export round-trip.

### Phase A — Baseline import from native text

1. Build/run setup: `cmake -S . -B build && cmake --build build && ./build/setup/ibus-setup-unikey` (or install and run the packaged setup).
2. **Clear** all macros.
3. **Import** [`tests/macros/unikey_macros.txt`](unikey_macros.txt) (UniKey native text format).
4. Apply **§ Shared rules**: terminal + editor + accents; spot-check the macro **definitions** in the setup list against the file for a few lines.
5. Optionally **restart IBus** or re-login if the engine does not see updates (environment-dependent).

### Phase B — Export → clear → import (round-trip per format)

Repeat **separately for each** format below (always **export**, then **clear**, then **import** the exported file, then re-check a few triggers in terminal/editor):

| Step | Format | Suggested export name | What to eyeball in the file |
|------|--------|----------------------|-----------------------------|
| B1 | **Apple plist** | `roundtrip.plist` | XML plist, array of dicts with `shortcut`/`phrase` (or synonyms). |
| B2 | **YAML (Espanso-style)** | `roundtrip.yaml` | Top-level `matches:` list of `trigger` / `replace`. |
| B3 | **JSON** | `roundtrip.json` | Array of `{ "trigger", "content" }` **or** flat string map; either should re-import. |
| B4 | **CSV** | `roundtrip.csv` | Quoted fields; header `trigger`/`content`. |
| B5 | **TSV** | `roundtrip.tsv` | Same as CSV but tab-separated. |
| B6 | **Native text** | `roundtrip.txt` | Optional: export then re-import to confirm native path still works. |

**Pass criteria:** After each re-import, the **row count** in setup is plausible (match expectations for your subset if you used a filtered table), and **spot-checked triggers** still expand correctly with **correct Vietnamese accents**.

### Phase C — Cross-platform (plist)

**Linux → macOS (if you have a Mac)**

1. On Linux: after Phase A (or B1), export **`mac-handoff.plist`** from setup.
2. Transfer the file to macOS (airdrop, scp, cloud).
3. On macOS: System Settings → Keyboard → Text Replacements (**or** import via a tool that accepts Apple plist for replacements—exact UI varies by macOS version). Confirm a few **shortcuts** work in TextEdit or Notes.

**macOS → Linux**

1. On macOS: export or copy your **Text Replacements** plist (user procedure depends on OS; some users sync via iCloud; developers may copy a plist from `~/Library/…` backups—do not invent paths here without verifying on your system).
2. On Linux: **clear** Unikey macros, **import** that plist via setup, then run **§ Shared rules** and compare a handful of **shortcut/phrase** pairs to the Mac side.

**Pass criteria:** No parser error; imported table non-empty; sampled triggers match **phrase text** closely (allow for Apple field-name or encoding quirks; report discrepancies as bugs).

### Phase D — “Foreign” fixtures already in the repo

With a **cleared** table, import each of (from [`tests/macros/`](.)):

- `unikey_macro.plist`, `unikey_macro.yaml`, `generic_macros.json`, `macros.csv`, `macros.tsv`

Then spot-check **at least one** trigger from each file in an editor. These are **small** fixtures; they complement `unikey_macros.txt`, not replace it.

### Phase E — Negative and edge scenarios (optional but valuable)

- **Empty / broken file:** Import an empty file or a JSON/plist that is not a supported shape; setup should show an error and **not** silently wipe unrelated rows if you canceled (behavior depends on UI—note any bug).
- **Duplicate triggers:** Import a file where the same folded trigger appears twice; behavior should match project policy (**last wins** in text macros—confirm same after interchange import).
- **Wrong extension:** Rename a JSON file to `.txt` and try import; **AUTO** routing may mis-parse—document if you expect users to pick the right filter.
- **Binary plist:** Our handler supports binary plists via libplist; if you have a **binary** `bplist` from macOS, try importing on Linux after clearing the table.

### Phase F — Automation reminder

```bash
ctest --test-dir build -R macro-file-io-test --output-on-failure
```

---

## Manual smoke (GTK setup) — short list

Build and run:

```bash
cmake -S . -B build && cmake --build build
./build/setup/ibus-setup-unikey
```

1. **Import — extension routing:** Open Macro preferences → Import → pick each fixture type; merged rows appear and no `.ukmcache` file appears next to the imported interchange file (only native UniKey engine paths use the cache).
2. **Export — extension routing:** Export with basename `test.yaml`, `test.plist`, `test.json`, `test.csv`, `test.tsv`; confirm output format matches extension (YAML block `matches:`, XML plist array of dicts, JSON array, quoted CSV/TSV with `trigger`/`content` header).
3. **Chooser filters:** Import/Export dialogs list “All supported formats” and per-type filters from `macro_file_chooser_attach_*`.

## Phạm vi (Tiếng Việt)

Kiểm tra **nhập/xuất macro định dạng trao đổi** theo phần mở rộng tên file (dispatcher trong `setup/macro_file_io.cpp`): text UniKey, YAML Espanso (`matches:`), plist (Apple, XML hoặc binary qua libplist), JSON tổng quát, CSV/TSV — **không** ghi cache `.ukmcache` và **không** dùng `loadFromFile` / `writeToFile` cho định dạng không phải text gốc.

**Kiểm thử tay:** Xoá macro trước mỗi lần import; import [`unikey_macros.txt`](unikey_macros.txt); gõ thử trong **terminal**, **trình soạn thảo**, kiểm tra **dấu thanh**; xuất plist/JSON/YAML/CSV rồi xoá và nhập lại để **round-trip**; nếu có **macOS**, thử plist hai chiều Linux ↔ Mac.

Bảng test tự động và hướng dẫn chạy nằm trong các file instruction trong [`tests/instructions/macros/`](../instructions/macros/).
