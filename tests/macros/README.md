# UI macro tests

This directory contains focused tests and fixtures for macro setup UI behavior.

## What is covered

Automated coverage includes:

- **Macro interchange I/O** (`macro_table_load_any_format` / `macro_interchange_*`): loads non-empty tables from fixtures by extension — `.txt`, `.yaml`/`.yml`, `.plist`, `.json`, `.csv`, `.tsv` (see [`TEST_PLAN_macro_interchange_io.md`](TEST_PLAN_macro_interchange_io.md)).
- **Remembered chooser directories** (GSettings): when no saved location exists, both choosers default to `$HOME/Documents`; import/export remember the parent directory of the chosen file; the latest path overwrites the previous value; invalid paths fall back to `$HOME/Documents`.
- **Export filename completion** helpers (`macro_export_filename_*`).

## Files

- `macro_file_io_test.cpp` — interchange loader tests (paired with `setup/macro_file_io.cpp`)
- `macro_dialog_state_test.cpp` — remembered chooser folders (`setup/macro_dialog_state.cpp`)
- `macro_export_filename_test.cpp` — export basename helpers (`setup/macro_export_filename.cpp`)
- `unikey_macros.txt`, `unikey_macro.yaml`, `unikey_macro.plist`, `generic_macros.json`, `macros.csv`, `macros.tsv` — interchange fixtures
- [`TEST_PLAN_macro_interchange_io.md`](TEST_PLAN_macro_interchange_io.md) — automated CTest mapping **and** **manual / round-trip / cross-platform** QA (primary reviewer checklist)

## Manual QA reminder

**CTest does not replace typing in real apps.** After automated tests pass, follow [**`TEST_PLAN_macro_interchange_io.md`**](TEST_PLAN_macro_interchange_io.md) § *Manual and round-trip QA*: import [`unikey_macros.txt`](unikey_macros.txt), exercise triggers with Vietnamese accents in terminal and editors, then export/import each interchange format and cross-check files from other OSes when possible.

From the repository root (install **`libgtk-3-dev`**, **`libjson-glib-dev`**, **`libplist-dev`**, **`libcsv-dev`**, **`libyaml-cpp-dev`** first so `setup/` configures):

```bash
cmake -S . -B build
cmake --build build
```

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

To run only this suite:

```bash
ctest --test-dir build -R macro-dialog-state-test --output-on-failure
```

## Notes

- The tests use `GSETTINGS_BACKEND=memory`, so they do not modify the user's
  real desktop settings.
- The schema is loaded from the build tree (`build/gsettings`).
- GTK chooser bookmark UI is left to GTK itself; these tests focus on the
  application-owned remembered last directories.
