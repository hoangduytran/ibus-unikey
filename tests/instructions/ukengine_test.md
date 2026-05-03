# `ukengine_test` — instructions

**Source:** [`tests/ukengine_test.cpp`](../ukengine_test.cpp)  
**CTest name:** `ukengine_test`

## Purpose

Smoke-tests the UniKey C++ engine pipeline (`UnikeySetup`, Telex filter, UTF-8 output) and **`CMacroTable`** behavior after the unbounded macro storage + hash lookup + `.ukmcache` sidecar work.

## Prerequisites

- Build target `ukengine_test` (linked against `libukengine`).
- Macro/cache scenarios create temporary files under `/tmp` (`/tmp/ibus_ukengine_test_XXXXXX`). Writable `/tmp` is required.

## How to run

```sh
# From build directory
./ukengine_test
# or
ctest -R ukengine_test --output-on-failure
```

## What each block exercises

| Block | What | Expected result |
|--------|------|----------------|
| Telex `dd` → `đ` | Core engine UTF-8 output | Buffer contains UTF-8 for đ (`0xC4 0x91`). |
| `lookupCSeq` | Consonant sequence table | `lookupCSeq(vnl_d, …)` returns `cs_d`. |
| `CMacroTable` 1025 rows | Regression: no fixed 1024 entry cap | `addItem` succeeds for all rows; `getCount() == 1025`. |
| Long key/value (~3500 / ~800 UTF-8 chars) | Dynamic conversion buffers | One row; UTF-8 round-trip via `getKey` / `getText` matches input strings. |
| Duplicate `addItem` same key | Hash map last-wins | `getCount() == 1`; text is the second value. |
| File `low` / `LOW` lines | ASCII fold last-wins on load | One row; replacement text is `win` (second line wins). |
| Sidecar cache | First load parses text and writes `{path}.ukmcache`; second load reads cache; editing text invalidates fingerprint | After first load, `.ukmcache` exists; second load still returns `cached_value`; after rewriting file to `q:fresh_value`, load returns `fresh_value`. |

## What reviewers should check

- Failures in **`lookupCSeq`** or **`dd → đ`** usually indicate regressions in core mapping or UTF-8 output.
- Failure in the **1025-row** loop implies macro storage or `addItem` regression.
- Failures in **long key/value** or **duplicate key** point to `utf8ToStdVnVector`, `foldedLookupKeyBytes`, or vector growth.
- Failures in **file last-wins** point to `mactabFoldKeyPrefix` / `buildLastWinsLineIndex` / `applyLoadedLinesToTable`.
- Failures in **sidecar** point to `MacroBinaryCache`, FNV fingerprint over the macro text file, or `persistForTextFile` / `tryLoadForTextFile`.

## Extend this test

When adding more macro behavior, extend [`tests/ukengine_test.cpp`](../ukengine_test.cpp) and update this file. GTK-focused scenarios belong under [`macros/`](macros/) when those sources are present in the tree.
