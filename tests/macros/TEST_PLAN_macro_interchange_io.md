# Test Plan — Macro interchange import/export I/O

**Design:** [`project_planning/UI/macros/20260503_170659_macro_format_handlers_io_design.md`](../../project_planning/UI/macros/20260503_170659_macro_format_handlers_io_design.md)

## Scope

Automated and manual validation of **filename-based macro interchange** (dispatcher in `setup/macro_file_io.cpp`): UniKey text, Espanso-style YAML (`matches:` / `trigger` / `replace`), Apple XML plist text replacements, generic JSON (array or flat map), RFC-style CSV/TSV — without writing canonical `.ukmcache` sidecars or using `CMacroTable::loadFromFile` / `writeToFile` for non-native extensions.

## Linked automated tests (CTest)

| Test case (g_test path prefix) | Instruction doc | CMake / CTest name |
|--------------------------------|-----------------|-------------------|
| `/ui/macros/file-io/load-*` | [`tests/instructions/macros/macro_file_io_test.md`](../instructions/macros/macro_file_io_test.md) | `macro-file-io-test` |
| Macro chooser remembered directories | [`tests/instructions/macros/macro_dialog_state_test.md`](../instructions/macros/macro_dialog_state_test.md) | `macro-dialog-state-test` |
| Export filename suffix completion | [`tests/instructions/macros/macro_export_filename_test.md`](../instructions/macros/macro_export_filename_test.md) | `macro-export-filename-test` |

Fixtures live beside the sources under [`tests/macros/`](.): `unikey_macros.txt`, `unikey_macro.yaml`, `unikey_macro.plist`, `generic_macros.json`, `macros.csv`.

## Manual scenarios (GTK setup)

Build and run:

```bash
cmake -S . -B build && cmake --build build
./build/setup/ibus-setup-unikey
```

1. **Import — extension routing:** Open Macro preferences → Import → pick each fixture type; merged rows appear and no `.ukmcache` file appears next to the imported interchange file (only native UniKey engine paths use the cache).
2. **Export — extension routing:** Export with basename `test.yaml`, `test.plist`, `test.json`, `test.csv`; confirm output format matches extension (YAML Espanso-like block, XML plist array of dicts, JSON array, CSV header).
3. **Chooser filters:** Import/Export dialogs list “All supported formats” and per-type filters from `macro_file_chooser_attach_*`.

## Phạm vi (Tiếng Việt)

Kiểm tra **nhập/xuất macro định dạng trao đổi** theo phần mở rộng tên file (dispatcher trong `setup/macro_file_io.cpp`): text UniKey, YAML Espanso (`matches:`), plist XML (Apple), JSON tổng quát, CSV/TSV — **không** ghi cache `.ukmcache` và **không** dùng `loadFromFile` / `writeToFile` cho định dạng không phải text gốc.

Bảng test tự động và hướng dẫn chạy nằm trong các file instruction trong [`tests/instructions/macros/`](../instructions/macros/).
