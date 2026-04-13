# ibus-unikey Technical Note

This note describes how `ibus-unikey` works internally, from IBus key events down to UniKey's Vietnamese composition engine, charset conversion layer, and macro replacement pipeline.

## Language
<a id="language"></a>
- [English](#table-of-contents)
- [Tiếng Việt](#ghi-chu-ky-thuat-tieng-viet)

## Table of Contents

- [1. Scope](#1-scope)
- [2. High-Level Architecture](#2-high-level-architecture)
- [3. End-to-End Runtime Flow](#3-end-to-end-runtime-flow)
- [4. How IBus Delivers Raw Keys](#4-how-ibus-delivers-raw-keys)
- [5. How UniKey Classifies and Translates Keys](#5-how-unikey-classifies-and-translates-keys)
- [6. Composition Buffer and Vietnamese Rules](#6-composition-buffer-and-vietnamese-rules)
- [7. Input Methods and Keyboard Mappings](#7-input-methods-and-keyboard-mappings)
- [8. Output Charsets, Conversion, and Fonts](#8-output-charsets-conversion-and-fonts)
- [9. Macro System](#9-macro-system)
- [10. Configuration and Setup Application](#10-configuration-and-setup-application)
- [11. Features and Behavioral Options](#11-features-and-behavioral-options)
- [12. Important Implementation Notes](#12-important-implementation-notes)
- [13. Source Map](#13-source-map)

## 1. Scope

`ibus-unikey` is an IBus input method engine that embeds a modified UniKey engine (`ukengine`) and exposes it to Linux desktop applications through IBus.

At a high level, it does four jobs:

1. Receives raw key events from IBus.
2. Interprets those keys using a selected Vietnamese input method such as Telex or VNI.
3. Converts the internal Vietnamese character representation into the requested output charset.
4. Pushes the result back to the application through IBus preedit and commit APIs.

The project does not draw glyphs and does not select fonts. It only produces text bytes and code points. Font selection and final rendering are handled by the target application, toolkit, and system font stack.

## 2. High-Level Architecture

The codebase is split into three main layers:

### Diagrams

- [High-Level Architecture (page 1)](project_dir/diagrams/01_high_level_architecture.svg): overall layers and dataflow from IBus to charset output.
- [Keystroke State Machine (page 2)](project_dir/diagrams/02_keystroke_state_machine.svg): composition state transitions for keystrokes.
- [VnLexi Mapping (page 3)](project_dir/diagrams/03_vnlexi_mapping.svg): how `vnlexi.h` maps into input processing and engine composition.
- [Charset Commit Flow (page 4)](project_dir/diagrams/04_charset_commit.svg): when the engine finalizes composition and encodes output bytes.

Below are inline previews of each diagram (click the image to open the SVG file):

[High-Level Architecture (page 1)](project_dir/diagrams/01_high_level_architecture.svg)

![](project_dir/diagrams/01_high_level_architecture.svg)

[Keystroke State Machine (page 2)](project_dir/diagrams/02_keystroke_state_machine.svg)

![](project_dir/diagrams/02_keystroke_state_machine.svg)

[VnLexi Mapping (page 3)](project_dir/diagrams/03_vnlexi_mapping.svg)

![](project_dir/diagrams/03_vnlexi_mapping.svg)

[Charset Commit Flow (page 4)](project_dir/diagrams/04_charset_commit.svg)

![](project_dir/diagrams/04_charset_commit.svg)

### 2.1 IBus integration layer

- [src/engine.cpp](src/engine.cpp) implements the `IBusEngine` callbacks.
- [src/engine_private.h](src/engine_private.h) stores the runtime state for the active engine instance.
- [src/config/unikey_config.h](src/config/unikey_config.h) maps persisted settings to internal enums and charset IDs.
- [src/config/org.freedesktop.ibus.engine.unikey.gschema.xml](src/config/org.freedesktop.ibus.engine.unikey.gschema.xml) defines the public settings schema.

This layer is responsible for talking to IBus, managing preedit text, reacting to settings changes, and calling the UniKey wrapper API.

### 2.2 UniKey wrapper layer

- [ukengine/engine/unikey.cpp](ukengine/engine/unikey.cpp) exposes a C-style API such as `UnikeySetup()`, `UnikeyFilter()`, `UnikeySetInputMethod()`, `UnikeySetOutputCharset()`, and `UnikeyLoadMacroTable()`.

This layer owns the global engine object, shared engine state, and exported output buffers such as `UnikeyBuf`, `UnikeyBufChars`, and `UnikeyBackspaces`.

### 2.3 Core Vietnamese engine and conversion layer

- [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp) contains the main composition state machine.
- [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp) and [include/ukengine/core/inputproc.h](include/ukengine/core/inputproc.h) define the input method mapping tables and key-event classification logic.
- [include/ukengine/mapping/vnlexi.h](include/ukengine/mapping/vnlexi.h) defines `VnLexiName`, the symbolic Vietnamese alphabet and sequence vocabulary used by the core engine to classify letters and by the mapping layer to refer to vowel clusters and consonant clusters.
- [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp) and [include/ukengine/mapping/charset.h](include/ukengine/mapping/charset.h) implement charset conversion objects.
- [include/ukengine/mapping/vnconv.h](include/ukengine/mapping/vnconv.h) defines charset identifiers and conversion options.
- [ukengine/tables/data.cpp](ukengine/tables/data.cpp) contains many of the Vietnamese mapping tables used by the conversion layer.

This layer decides what a key means, how it changes the current word, how tones and diacritics move, when text must be restored, and how output bytes are generated.

`vnlexi.h` is the shared symbol vocabulary for the Vietnamese character system in ibus-unikey. It gives names to base letters, toned letters, vowel sequences, and consonant clusters so the processor can classify input consistently and the mapping layer can work with canonical Vietnamese symbols instead of raw key sequences.

In practice, this is the state transition pipeline for keyboard strokes: raw key events are classified by `inputproc.cpp`, the resulting Vietnamese symbol state is tracked by `vnlexi.h` and `ukengine.cpp`, and the final Vietnamese text is converted by `charset.cpp` into the requested output encoding. The engine does not create fonts itself; it outputs encoded text, and the desktop font stack renders the Vietnamese glyphs.

| Step | What happens |
|---|---|
| 1 | Raw key strokes enter the input processor. |
| 2 | `vnlexi.h` gives names to the Vietnamese symbols and sequences used during classification. |
| 3 | `ukengine.cpp` updates the composition state and produces the final Vietnamese text. |
| 4 | `charset.cpp` converts that text to the selected output encoding, and the font stack renders the glyphs. |

```text
+------------------+
| Raw key strokes  |
+------------------+
             |
             v
+-----------------------------------+
| inputproc.cpp classifies the key  |
+-----------------------------------+
             |
             v
+-------------------------------------------+
| vnlexi.h names the symbol state           |
| for Vietnamese letters and sequences      |
+-------------------------------------------+
             |
             v
+-----------------------------------+
| ukengine.cpp updates composition  |
+-----------------------------------+
             |
             v
+---------------------------------------------+
| charset.cpp converts to the selected format |
+---------------------------------------------+
             |
             v
+-----------------------------------+
| font stack renders the glyphs     |
+-----------------------------------+
```

## 3. End-to-End Runtime Flow

The normal runtime path is:

1. IBus calls `ibus_unikey_engine_process_key_event()` in [src/engine.cpp](src/engine.cpp).
2. The engine checks the key class: navigation, control, backspace, keypad, printable ASCII, and so on.
3. For printable input, the IBus layer calls `UnikeyFilter(keyval)` in [ukengine/engine/unikey.cpp](ukengine/engine/unikey.cpp).
4. `UnikeyFilter()` forwards the key to `MyKbEngine.process(...)`, implemented in [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp).
5. The core engine classifies the key by the active input method and decides whether it is:
   - a normal letter,
   - a Vietnamese tone key,
   - a diacritic modifier,
   - a word break,
   - an escape sequence,
   - or a candidate for macro expansion.
6. The engine updates its internal composition buffer and writes the result into the exported UniKey output buffer.
7. The IBus integration layer reads `UnikeyBackspaces`, `UnikeyBuf`, and `UnikeyBufChars`.
8. If output charset is UTF-8, it appends bytes directly to the preedit string.
9. If output charset is a legacy encoding, it converts the bytes back to UTF-8 for display in the IBus preedit UI before showing them to the user.
10. When a commit boundary is reached, the engine commits the preedit string to the target application.

The important point is that the UniKey core works with its own internal representation first, then a charset converter is applied near the output boundary.

## 4. How IBus Delivers Raw Keys

The entry point is `ibus_unikey_engine_process_key_event()` in [src/engine.cpp](src/engine.cpp). IBus delivers three values:

- `keyval`: the logical key symbol,
- `keycode`: the physical key code,
- `modifiers`: bit flags such as Shift and Caps Lock.

The ibus-unikey code primarily uses `keyval` for actual text interpretation.

### 4.1 What the IBus layer handles directly

Before handing input to the UniKey core, the IBus layer handles a few cases itself:

- Backspace: calls `UnikeyBackspacePress()` and updates preedit.
- Non-text control/navigation keys: commits pending preedit and lets the application receive the key.
- Keypad keys: commits pending preedit and lets the application handle them.
- Shift state: passed to `UnikeySetCapsState()` so the core engine can make case-sensitive decisions.

### 4.2 Printable characters

Printable ASCII keys are sent into UniKey through `UnikeyFilter(keyval)`.

There is one notable special path in [src/engine.cpp](src/engine.cpp): when the input method is Telex or Simple Telex 2 and the `standalone-w-as-uw` option is disabled, a standalone `w` at the beginning of a word can be passed through directly instead of being treated as a Telex modifier.

### 4.3 Preedit and commit behavior

The IBus layer keeps a `std::string` preedit buffer. After every processed key:

- it removes characters when `UnikeyBackspaces > 0`,
- it appends the generated output bytes,
- and it updates the visible preedit string.

When a word break or non-processable control path is reached, `ibus_unikey_buffer_commit()` commits the preedit string into the application.

## 5. How UniKey Classifies and Translates Keys

The first stage inside the UniKey core is classification. The mapping tables and event enums live in [include/ukengine/core/inputproc.h](include/ukengine/core/inputproc.h).

The central concept is `UkKeyEvent`, which records:

- the event type,
- the classified character type,
- the Vietnamese lexical symbol,
- the raw key code,
- and tone information.

The input processor maps raw key codes into semantic actions. The most
important event types are listed below with a short definition and example.

| Event | Meaning | Example |
|---|---|---|
| `vneTone1` | Acute tone (sắc) | `a` → <span style="font-size:2em;line-height:1">á</span> |
| `vneTone2` | Grave tone (huyền) | `a` → <span style="font-size:2em;line-height:1">à</span> |
| `vneTone3` | Hook tone (hỏi) | `a` → <span style="font-size:2em;line-height:1">ả</span> |
| `vneTone4` | Tilde tone (ngã) | `a` → <span style="font-size:2em;line-height:1">ã</span> |
| `vneTone5` | Dot tone (nặng) | `a` → <span style="font-size:2em;line-height:1">ạ</span> |
| `vneRoof_a` | Roof (circumflex) on `a` | `aa` → <span style="font-size:2em;line-height:1">â</span> |
| `vneRoof_e` | Roof on `e` | `ee` → <span style="font-size:2em;line-height:1">ê</span> |
| `vneRoof_o` | Roof on `o` | `oo` → <span style="font-size:2em;line-height:1">ô</span> |
| `vneHook_u` | Horn on `u` (ư) | `uw` → <span style="font-size:2em;line-height:1">ư</span> |
| `vneHook_o` | Horn on `o` (ơ) | `ow` → <span style="font-size:2em;line-height:1">ơ</span> |
| `vneBowl` | Bowl / breve modifier (ă) | `a8` or `a(` → <span style="font-size:2em;line-height:1">ă</span> |
| `vneDd` | Doubled consonant → `đ` | `dd` → <span style="font-size:2em;line-height:1">đ</span> |
| `vne_telex_w` | Telex special `w` handling (creates ă/ơ/ư) | `aw` → <span style="font-size:2em;line-height:1">ă</span>, `ow` → <span style="font-size:2em;line-height:1">ơ</span>, `uw` → <span style="font-size:2em;line-height:1">ư</span> |
| `vneMapChar` | Explicit mapping to a Vietnamese symbol | direct key → <span style="font-size:2em;line-height:1">ô</span>/<span style="font-size:2em;line-height:1">ư</span> |
| `vneEscChar` | Escape / cancel a Vietnamese action | treat marker as raw character |
| `vneNormal` | Normal key: no Vietnamese mapping | `a`, `b`, `1` |

This step is where a letter like `s` stops meaning the Latin letter `s` and starts meaning “apply acute tone” under Telex rules, or where `1` means “tone 1” under VNI rules.

## 6. Composition Buffer and Vietnamese Rules

The composition logic is implemented in [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp).

Internally, the engine maintains word and keystroke state, including:

- the current word buffer,
- tone and diacritic placement,
- whether the current run is Vietnamese or not,
- change positions for incremental update,
- and the list of original keystrokes used for restoration.

### 6.1 Main processing path

The main method is `UkEngine::process(...)`. It classifies the key and dispatches to routines such as:

- `processTone()` for tone changes,
- `processDd()` for `dd -> đ`,
- `appendVowel()` for vowel composition,
- `appendConsonnant()` for consonant insertion,
- `checkEscapeVIQR()` for VIQR escape handling,
- `restoreKeyStrokes()` when a Vietnamese transformation must be undone.

### 6.2 Why the engine needs backspaces

Vietnamese composition often modifies earlier characters. For example, a later tone key may change a vowel typed several keystrokes ago. The engine therefore returns:

- a count of how many displayed characters must be removed,
- and a replacement byte sequence for the new composed form.

That is why the IBus layer must apply backspaces to preedit before appending the replacement text.

### 6.3 Restore keystrokes

The engine supports restoring original keystrokes through `UnikeyRestoreKeyStrokes()`, used by the IBus layer for shortcuts such as Shift+Space and Shift+Shift. This lets the user undo an automatic Vietnamese interpretation and recover the raw typed sequence.

## 7. Input Methods and Keyboard Mappings

It is important to distinguish between:

- the physical desktop keyboard layout handled by XKB and IBus,
- and the Vietnamese input method handled by UniKey.

The IBus engine descriptor uses `layout="*"`, which means ibus-unikey is not bound to a specific physical layout definition. It expects the application stack to deliver key symbols, and then UniKey interprets those symbols according to the selected Vietnamese method.

### 7.1 Input methods exposed by the IBus setup UI

The setup UI and GSettings schema expose four input methods:

1. `telex` (`UkTelex`) as “Extend Telex”
2. `vni` (`UkVni`)
3. `stelex` (`UkSimpleTelex`)
4. `stelex2` (`UkSimpleTelex2`)

These are defined in [src/config/unikey_config.h](src/config/unikey_config.h) and in the settings schema at [src/config/org.freedesktop.ibus.engine.unikey.gschema.xml](src/config/org.freedesktop.ibus.engine.unikey.gschema.xml).

### 7.2 Additional input methods implemented in the engine

The UniKey core also contains internal mappings for:

- `UkViqr`
- `UkMsVi`
- `UkUsrIM` for a user-loaded custom mapping

These exist in the engine code and headers, but they are not exposed through the current IBus setup schema.

### 7.3 Mapping summary

#### Telex

Defined by `TelexMethodMapping` in [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp).
Typical rules are shown in the table below.

| Key(s) | Action | Example |
|---|---|---|
| `s`, `f`, `r`, `x`, `j` | Tone keys (sắc, huyền, hỏi, ngã, nặng) | `a` → <span style="font-size:2em;line-height:1">á</span>, <span style="font-size:2em;line-height:1">à</span>, <span style="font-size:2em;line-height:1">ả</span>, <span style="font-size:2em;line-height:1">ã</span>, <span style="font-size:2em;line-height:1">ạ</span> |
| `z` | Remove marks / cancel | `á` + `z` → `a` |
| `aa` | Roof on `a` | `aa` → <span style="font-size:2em;line-height:1">â</span> |
| `ee` | Roof on `e` | `ee` → <span style="font-size:2em;line-height:1">ê</span> |
| `oo` | Roof on `o` | `oo` → <span style="font-size:2em;line-height:1">ô</span> |
| `dd` | Doubled consonant → `đ` | `dd` → <span style="font-size:2em;line-height:1">đ</span> |
| `w` (Telex) | Participates in `ă`, `ơ`, `ư` formation | `aw` → <span style="font-size:2em;line-height:1">ă</span>, `ow` → <span style="font-size:2em;line-height:1">ơ</span>, `uw` → <span style="font-size:2em;line-height:1">ư</span> |

#### VNI

Defined by `VniMethodMapping` in [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp).
Typical VNI rules are shown below.

| Key(s) | Action | Example |
|---|---|---|
| `1`..`5` | Tone digits (1=acute, 2=grave, 3=hook, 4=tilde, 5=dot) | `a1` → <span style="font-size:2em;line-height:1">á</span>, `a2` → <span style="font-size:2em;line-height:1">à</span> |
| `6`,`7`,`8`,`9` | Vowel-shape modifiers and `đ` | `a6`/`a7` → <span style="font-size:2em;line-height:1">â</span>/<span style="font-size:2em;line-height:1">ă</span>, `d9` → <span style="font-size:2em;line-height:1">đ</span> |
| `0` | Reset / cancel | cancels active marks |


#### Simple Telex and Simple Telex 2

`SimpleTelexMethodMapping` and `SimpleTelex2MethodMapping` in [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp) define reduced Telex variants. The table below compares the full Telex mode with the simplified mode.

| Feature | Full Telex (non-simplified) | Simple Telex (simplified) |
|---|---|---|
| Tone keys | Uses `s`, `f`, `r`, `x`, `j` with full context heuristics to place tones on the correct vowel. Example: `a` + `s` → <span style="font-size:2em;line-height:1">á</span>. | Uses the same tone keys, but with simpler rules and fewer context rewrites. Example: `a` + `s` → <span style="font-size:2em;line-height:1">á</span>. |
| Circumflex and vowel shape | `aa` → <span style="font-size:2em;line-height:1">â</span>, `ee` → <span style="font-size:2em;line-height:1">ê</span>, `oo` → <span style="font-size:2em;line-height:1">ô</span> using full composition logic. | The same sequences produce the same characters, but the simplified mode applies fewer multi-letter heuristics. |
| `w` handling | Full Telex applies contextual `w` rules: `aw` → <span style="font-size:2em;line-height:1">ă</span>, `ow` → <span style="font-size:2em;line-height:1">ơ</span>, `uw` → <span style="font-size:2em;line-height:1">ư</span>. | Simple Telex is more conservative with `w`; `SimpleTelex2` often passes a standalone `w` at the start of a word through unchanged. |
| `dd` to `đ` | `dd` → <span style="font-size:2em;line-height:1">đ</span>. | `dd` → <span style="font-size:2em;line-height:1">đ</span>. |
| Cancel behavior | `z` removes marks and may undo more than one recent composition step. | `z` removes marks with simpler, less aggressive undo behavior. |
| Tradeoff | More expressive and more context-aware, but slightly more likely to surprise the user. | Simpler, more predictable, and less likely to make broad context-based rewrites. |

`SimpleTelex2` is a minor variant that differs mainly in how `w` is treated at word boundaries.

**Illustrative differences**

| Input sequence | Full Telex (non-simplified) | Simple Telex (simplified) |
|---|---|---|
| `aw` | `aw` becomes <span style="font-size:2em;line-height:1">ă</span>. | `aw` also becomes <span style="font-size:2em;line-height:1">ă</span>. |
| Standalone `w` at the start of a word | May be interpreted as a modifier in some contexts. | Often passed through as a literal `w` and left unchanged. |
| Ambiguous tone placement | Uses context to place the tone on the linguistically correct vowel, and may move an earlier mark. | Usually applies the tone to the most recently typed vowel and avoids moving earlier marks. |
| `z` after multiple transformations | May undo several recent composition steps. | Usually removes the last applied mark only. |


#### VIQR

Defined by `VIQRMethodMapping` in [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp), and supported more deeply by the VIQR charset conversion logic in [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp).

Typical VIQR markers and examples:

| Marker | Action | Example |
|---|---|---|
| `'` | Acute (sắc) | `a'` → <span style="font-size:2em;line-height:1">á</span> |
| `` ` `` | Grave (huyền) | ``a` `` → <span style="font-size:2em;line-height:1">à</span> |
| `?` | Hook (hỏi) | `a?` → <span style="font-size:2em;line-height:1">ả</span> |
| `~` | Tilde (ngã) | `a~` → <span style="font-size:2em;line-height:1">ã</span> |
| `.` | Dot (nặng) | `a.` → <span style="font-size:2em;line-height:1">ạ</span> |
| `^` | Roof / vowel shapes | `a^` → <span style="font-size:2em;line-height:1">â</span>, `o^` → <span style="font-size:2em;line-height:1">ô</span> |

#### Microsoft Vietnamese compatibility
`MsViMethodMapping` exists for compatibility with Microsoft-style Vietnamese input behavior. Typical examples follow the same Vietnamese outputs shown above (e.g., `dd` → <span style="font-size:2em;line-height:1">đ</span>), but use Microsoft-specific key choices.

## 8. Output Charsets, Conversion, and Fonts

Output charset selection is configured in [src/config/unikey_config.h](src/config/unikey_config.h) and implemented by the conversion classes in [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp).

### 8.1 Internal engine representation

The UniKey core does not work directly on UTF-8 strings while composing. It uses an internal standardized Vietnamese character representation (`StdVnChar`) and converts that representation to the requested output charset at the boundary.

### 8.2 Exposed output charsets

The IBus setup schema exposes these output charsets:

1. `unicode` -> `CONV_CHARSET_XUTF8`
2. `tcvn3` -> `CONV_CHARSET_TCVN3`
3. `vni-win` -> `CONV_CHARSET_VNIWIN`
4. `viqr` -> `CONV_CHARSET_VIQR`
5. `bk-hcm2` -> `CONV_CHARSET_BKHCM2`
6. `cstr` -> `CONV_CHARSET_UNI_CSTRING`
7. `ncr-dec` -> `CONV_CHARSET_UNIREF`
8. `ncr-hex` -> `CONV_CHARSET_UNIREF_HEX`

### 8.3 Charset converter objects

The conversion layer includes converters for:

- internal Vietnamese format,
- single-byte legacy charsets,
- double-byte charsets,
- Unicode,
- decomposed and composed Unicode,
- UTF-8,
- VIQR,
- UTF-8 plus VIQR hybrid output.

`CVnCharsetLib` in [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp) owns and returns these converter objects.

### 8.4 What happens with fonts

This is the practical rule:

- ibus-unikey outputs text bytes in a selected encoding,
- IBus and the target application receive those bytes or Unicode text,
- the application and system font stack render glyphs.

That means:

- for `unicode`, modern system fonts usually render correctly,
- for legacy charsets like TCVN3 or VNI-Win, the text only looks correct if the consuming application expects that encoding and uses a compatible legacy font.

So ibus-unikey does not “deliver fonts”. It delivers encoded text. Font correctness is external to the engine.

### 8.5 Why preedit is still shown as UTF-8

Even when a non-UTF-8 output charset is selected, the IBus frontend still needs something displayable in the Linux UI. The IBus layer therefore converts generated bytes back into UTF-8 for the preedit display using `latinToUtf(...)` in [src/engine.cpp](src/engine.cpp). This is a display convenience, not a change to the configured target output charset logic.

## 9. Macro System

Macros are a distinct subsystem layered on top of the Vietnamese composition engine.

### 9.1 Storage and loading

- The macro file path is defined as `~/.ibus/unikey/macro` by `UNIKEY_MACRO_FILE` in [src/config/unikey_config.h](src/config/unikey_config.h).
- The IBus engine loads this file on startup by calling `UnikeyLoadMacroTable(...)` in [src/engine.cpp](src/engine.cpp).
- The wrapper API forwards that request to the macro table store in [ukengine/engine/unikey.cpp](ukengine/engine/unikey.cpp).

### 9.2 When macro replacement runs

Macro matching is performed in `UkEngine::macroMatch(...)` in [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp).

The engine scans backward across the current buffered word, builds a standardized Vietnamese key sequence, and tries to find a macro entry in the macro table.

### 9.3 Internal matching format

Macro keys are not matched against raw screen bytes. They are matched against the internal Vietnamese standard representation assembled from the composition buffer. This is important because it allows macro lookup to work consistently even when the visible output charset is not UTF-8.

### 9.4 Case handling

If a macro matches, the replacement text is case-adjusted based on the typed trigger sequence:

- all lowercase trigger -> lowercase replacement,
- all uppercase trigger -> uppercase replacement,
- mixed case trigger -> replacement kept as stored.

### 9.5 Output conversion and separator preservation

After a macro match:

1. the replacement text is converted from the internal Vietnamese standard format to the currently selected output charset,
2. the current triggering key is appended after the macro output when needed,
3. the engine resets composition state while preserving the correct backspace count and output bytes.

This explains why a macro can expand and still keep the trailing space or enter key that triggered the expansion.

### 9.6 Setup UI macro editor

The setup application loads macros into a GTK list store, lets the user edit them, then writes them back to disk.

Relevant files:

- [setup/controller/setup_controller.cpp](setup/controller/setup_controller.cpp)
- [setup/macro_utils.cpp](setup/macro_utils.cpp)
- [setup/ui/macro_dialog.ui](setup/ui/macro_dialog.ui)

The setup UI converts macro text between internal Vietnamese standard format and UTF-8 for display and editing.

## 10. Configuration and Setup Application

The desktop setup application is built from the `setup/` directory and writes settings through GSettings.

### 10.1 Persistence model

Settings are persisted under the schema:

- `org.freedesktop.ibus.engine.unikey`

Key settings include:

- input method,
- output charset,
- spell check,
- auto restore non-Vietnamese,
- modern style,
- free marking,
- macro enabled,
- standalone `w` behavior.

### 10.2 Update flow

The flow is:

1. user changes a setting in the setup UI,
2. `SetupController` writes the value through `ibus_unikey_config_set_*`,
3. GSettings persists the change,
4. the engine receives the change callback,
5. the running IBus engine updates UniKey using `UnikeySetInputMethod()`, `UnikeySetOutputCharset()`, or `UnikeySetOptions()`.

This means configuration changes can be applied to the live engine without restarting the whole desktop session.

### 10.3 More settings entry point

The engine descriptor created in [src/engine/engine_app.cpp](src/engine/engine_app.cpp) registers the setup executable `ibus-setup-unikey`, so IBus can launch the graphical settings application.

## 11. Features and Behavioral Options

From the current schema and engine code, the main user-visible features are:

### 11.1 Vietnamese mode with multiple input methods

The active input method controls how Latin keystrokes become Vietnamese letters.

### 11.2 Multiple output charsets

The same internal Vietnamese text can be emitted as UTF-8, legacy encodings, VIQR, C string escapes, or numeric character references.

### 11.3 Spell check

This affects whether the engine enforces Vietnamese word validity rules while composing.

### 11.4 Auto restore non-Vietnamese word

When enabled, the engine can restore original keystrokes for words that should not remain in Vietnamese-composed form. This works together with spell checking.

### 11.5 Modern style

This controls orthographic style choices such as modern placement conventions described in the settings schema.

### 11.6 Free marking

This relaxes some tone and mark placement constraints so users can type with more freedom.

### 11.7 Macro expansion

When enabled, typed trigger strings can be replaced by stored macro text.

### 11.8 Standalone `w` as `ư`

This changes Telex behavior for `w` at word beginning and is handled partly in the IBus integration layer.

### 11.9 Restore shortcuts

The engine description in [src/engine/engine_app.cpp](src/engine/engine_app.cpp) documents these shortcuts:

- Shift+Space
- Shift+Shift

These ask the engine to restore original keystrokes instead of keeping the Vietnamese transformation.

## 12. Important Implementation Notes

### 12.1 The project exposes four input methods in the current IBus UI, not every method implemented by the UniKey core

The setup UI currently exposes Telex, VNI, Simple Telex, and Simple Telex 2. The core code contains additional mappings such as VIQR and Microsoft Vietnamese compatibility, but they are not selectable from the shipped GSettings choices.

### 12.2 The code and descriptive text are not perfectly synchronized everywhere

For example, [src/engine/engine_app.cpp](src/engine/engine_app.cpp) still describes 7 output charsets in its help text, while the current GSettings schema exposes 8 choices including `bk-hcm2`.

### 12.3 Display encoding and committed encoding are different concerns

The preedit text shown in the IBus UI may be displayed through UTF-8 conversion even when the target output charset is a legacy encoding. This is necessary for UI display and should not be confused with the configured logical output mode.

### 12.4 Physical keyboard layout is outside this engine

ibus-unikey interprets logical key values delivered by IBus. It does not install or manage a physical keyboard layout in the XKB sense.

## 13. Source Map

These are the most important files to read when tracing behavior:

- IBus engine entry and preedit handling: [src/engine.cpp](src/engine.cpp)
- Engine descriptor and user-facing help text: [src/engine/engine_app.cpp](src/engine/engine_app.cpp)
- Config keys and setting-to-enum maps: [src/config/unikey_config.h](src/config/unikey_config.h)
- GSettings schema: [src/config/org.freedesktop.ibus.engine.unikey.gschema.xml](src/config/org.freedesktop.ibus.engine.unikey.gschema.xml)
- UniKey public wrapper API: [ukengine/engine/unikey.cpp](ukengine/engine/unikey.cpp)
- Core composition engine: [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp)
- Input method mapping tables: [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp)
- Input event definitions: [include/ukengine/core/inputproc.h](include/ukengine/core/inputproc.h)
- Vietnamese lexical symbols and sequence enums: [include/ukengine/mapping/vnlexi.h](include/ukengine/mapping/vnlexi.h)
- Charset conversion layer: [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp)
- Charset constants and options: [include/ukengine/mapping/vnconv.h](include/ukengine/mapping/vnconv.h)
- Macro editing UI flow: [setup/controller/setup_controller.cpp](setup/controller/setup_controller.cpp)
- Macro GTK helpers: [setup/macro_utils.cpp](setup/macro_utils.cpp)

<a id="ghi-chu-ky-thuat-tieng-viet"></a>
## Ghi chú kỹ thuật tiếng Việt

Phần này là bản dịch tiếng Việt của tài liệu kỹ thuật ở trên. Nếu bạn muốn đọc bản tiếng Anh, quay lại phần [Language](#language).

### Mục lục tiếng Việt

- [1. Phạm vi](#1-pham-vi)
- [2. Kiến trúc tổng thể](#2-kien-truc-tong-the)
- [3. Luồng chạy từ đầu đến cuối](#3-luong-chay-tu-dau-den-cuoi)
- [4. IBus chuyển phím thô vào engine như thế nào](#4-ibus-chuyen-phim-tho-vao-engine-nhu-the-nao)
- [5. UniKey phân loại và dịch phím như thế nào](#5-unikey-phan-loai-va-dich-phim-nhu-the-nao)
- [6. Bộ đệm soạn thảo và các quy tắc tiếng Việt](#6-bo-dem-soan-thao-va-cac-quy-tac-tieng-viet)
- [7. Kiểu gõ và bảng ánh xạ bàn phím](#7-kieu-go-va-bang-anh-xa-ban-phim)
- [8. Bảng mã đầu ra, chuyển đổi và font](#8-bang-ma-dau-ra-chuyen-doi-va-font)
- [9. Hệ thống macro](#9-he-thong-macro)
- [10. Cấu hình và ứng dụng thiết lập](#10-cau-hinh-va-ung-dung-thiet-lap)
- [11. Tính năng và tùy chọn hành vi](#11-tinh-nang-va-tuy-chon-hanh-vi)
- [12. Các ghi chú triển khai quan trọng](#12-cac-ghi-chu-trien-khai-quan-trong)
- [13. Bản đồ mã nguồn](#13-ban-do-ma-nguon)

## 1. Phạm vi

`ibus-unikey` là một bộ gõ IBus tích hợp engine UniKey đã được chỉnh sửa (`ukengine`) và cung cấp nó cho các ứng dụng Linux thông qua IBus.

Ở mức tổng quát, dự án này làm bốn việc:

1. Nhận sự kiện phím thô từ IBus.
2. Diễn giải các phím đó theo kiểu gõ tiếng Việt đang chọn như Telex hoặc VNI.
3. Chuyển biểu diễn ký tự tiếng Việt nội bộ sang bảng mã đầu ra được yêu cầu.
4. Gửi kết quả ngược lại cho ứng dụng thông qua preedit và commit của IBus.

Dự án không tự vẽ glyph và cũng không tự chọn font. Nó chỉ tạo ra byte văn bản hoặc mã ký tự. Việc chọn font và hiển thị cuối cùng do ứng dụng đích, toolkit và font stack của hệ thống đảm nhiệm.

## 2. Kiến trúc tổng thể

Mã nguồn được chia thành ba lớp chính:

### 2.1 Lớp tích hợp IBus

- [src/engine.cpp](src/engine.cpp) hiện thực các callback của `IBusEngine`.
- [src/engine_private.h](src/engine_private.h) lưu trạng thái runtime của instance engine đang hoạt động.
- [src/config/unikey_config.h](src/config/unikey_config.h) ánh xạ cấu hình đã lưu sang enum nội bộ và ID bảng mã.
- [src/config/org.freedesktop.ibus.engine.unikey.gschema.xml](src/config/org.freedesktop.ibus.engine.unikey.gschema.xml) định nghĩa schema cấu hình công khai.

Lớp này chịu trách nhiệm giao tiếp với IBus, quản lý preedit, phản ứng với thay đổi cấu hình và gọi API bọc UniKey.

### 2.2 Lớp bọc UniKey

- [ukengine/engine/unikey.cpp](ukengine/engine/unikey.cpp) cung cấp các API kiểu C như `UnikeySetup()`, `UnikeyFilter()`, `UnikeySetInputMethod()`, `UnikeySetOutputCharset()` và `UnikeyLoadMacroTable()`.

Lớp này sở hữu đối tượng engine toàn cục, vùng trạng thái dùng chung và các buffer đầu ra được export như `UnikeyBuf`, `UnikeyBufChars` và `UnikeyBackspaces`.

### 2.3 Lớp engine tiếng Việt và chuyển đổi bảng mã

- [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp) chứa state machine soạn thảo chính.
- [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp) và [include/ukengine/core/inputproc.h](include/ukengine/core/inputproc.h) định nghĩa bảng ánh xạ kiểu gõ và logic phân loại sự kiện phím.
- [include/ukengine/mapping/vnlexi.h](include/ukengine/mapping/vnlexi.h) định nghĩa `VnLexiName`, tức bộ ký hiệu chữ cái tiếng Việt và các mã chuỗi chuẩn mà lõi UniKey dùng để phân loại chữ cái, còn lớp ánh xạ dùng để gọi tên các cụm nguyên âm và cụm phụ âm.
- [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp) và [include/ukengine/mapping/charset.h](include/ukengine/mapping/charset.h) hiện thực các bộ chuyển đổi bảng mã.
- [include/ukengine/mapping/vnconv.h](include/ukengine/mapping/vnconv.h) định nghĩa ID bảng mã và các tùy chọn chuyển đổi.
- [ukengine/tables/data.cpp](ukengine/tables/data.cpp) chứa nhiều bảng dữ liệu tiếng Việt dùng bởi lớp chuyển đổi.

Lớp này quyết định một phím có ý nghĩa gì, nó thay đổi từ hiện tại như thế nào, dấu và âm được dời ra sao, khi nào phải khôi phục chuỗi gõ, và byte đầu ra được sinh như thế nào.

`vnlexi.h` là bộ từ vựng ký hiệu dùng chung cho hệ thống chữ tiếng Việt trong ibus-unikey. Nó đặt tên cho các chữ cái cơ bản, chữ có dấu, chuỗi nguyên âm và cụm phụ âm để bộ xử lý đầu vào phân loại phím một cách nhất quán và lớp ánh xạ làm việc với ký hiệu tiếng Việt chuẩn thay vì chuỗi phím thô.

Trong thực tế, đây là chuỗi chuyển trạng thái của các phím gõ: các sự kiện phím thô được `inputproc.cpp` phân loại, trạng thái ký hiệu tiếng Việt được `vnlexi.h` và `ukengine.cpp` theo dõi, rồi `charset.cpp` chuyển văn bản tiếng Việt cuối cùng sang bảng mã đầu ra đã chọn. Engine không tự tạo font; nó chỉ xuất văn bản đã mã hóa, còn bộ font của hệ điều hành sẽ hiển thị các ký tự tiếng Việt.

| Bước | Điều xảy ra |
|---|---|
| 1 | Các phím gõ thô đi vào bộ xử lý đầu vào. |
| 2 | `vnlexi.h` đặt tên cho các ký hiệu và chuỗi tiếng Việt được dùng trong quá trình phân loại. |
| 3 | `ukengine.cpp` cập nhật trạng thái soạn thảo và tạo ra văn bản tiếng Việt cuối cùng. |
| 4 | `charset.cpp` chuyển văn bản đó sang bảng mã đầu ra đã chọn, rồi hệ thống font hiển thị các ký tự. |

```text
+------------------+
| Phím gõ thô      |
+------------------+
             |
             v
+--------------------------------+
| inputproc.cpp phân loại phím    |
+--------------------------------+
             |
             v
+----------------------------------------------+
| vnlexi.h đặt tên cho trạng thái ký hiệu      |
| tiếng Việt                                   |
+----------------------------------------------+
             |
             v
+--------------------------------+
| ukengine.cpp cập nhật trạng thái |
| soạn thảo                       |
+--------------------------------+
             |
             v
+----------------------------------------------+
| charset.cpp chuyển sang bảng mã đã chọn      |
+----------------------------------------------+
             |
             v
+--------------------------------+
| bộ font hiển thị các ký tự     |
+--------------------------------+
```

## 3. Luồng chạy từ đầu đến cuối

Luồng xử lý thông thường là:

1. IBus gọi `ibus_unikey_engine_process_key_event()` trong [src/engine.cpp](src/engine.cpp).
2. Engine kiểm tra loại phím: điều hướng, control, backspace, keypad, ASCII in được, v.v.
3. Với ký tự in được, lớp IBus gọi `UnikeyFilter(keyval)` trong [ukengine/engine/unikey.cpp](ukengine/engine/unikey.cpp).
4. `UnikeyFilter()` chuyển phím sang `MyKbEngine.process(...)`, được hiện thực trong [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp).
5. Engine lõi phân loại phím theo kiểu gõ đang chọn và quyết định đây là:
   - ký tự thường,
   - phím đặt dấu tiếng Việt,
   - phím biến đổi dấu phụ,
   - ký tự ngắt từ,
   - chuỗi escape,
   - hoặc ứng viên mở rộng macro.
6. Engine cập nhật bộ đệm soạn thảo nội bộ và ghi kết quả vào buffer đầu ra của UniKey.
7. Lớp tích hợp IBus đọc `UnikeyBackspaces`, `UnikeyBuf` và `UnikeyBufChars`.
8. Nếu bảng mã đầu ra là UTF-8, nó nối trực tiếp các byte vào chuỗi preedit.
9. Nếu bảng mã đầu ra là bảng mã cũ, nó chuyển các byte đó ngược lại sang UTF-8 để hiển thị preedit trong giao diện IBus.
10. Khi gặp ranh giới commit, engine commit chuỗi preedit vào ứng dụng đích.

Điểm quan trọng là UniKey lõi luôn làm việc với biểu diễn nội bộ trước, sau đó bộ chuyển đổi bảng mã mới được áp vào gần ranh giới đầu ra.

## 4. IBus chuyển phím thô vào engine như thế nào

Điểm vào là `ibus_unikey_engine_process_key_event()` trong [src/engine.cpp](src/engine.cpp). IBus chuyển vào ba giá trị:

- `keyval`: ký hiệu logic của phím,
- `keycode`: mã phím vật lý,
- `modifiers`: cờ trạng thái như Shift và Caps Lock.

Mã của ibus-unikey chủ yếu dùng `keyval` để diễn giải nội dung văn bản.

### 4.1 Những gì lớp IBus xử lý trực tiếp

Trước khi chuyển input vào lõi UniKey, lớp IBus tự xử lý một số trường hợp:

- Backspace: gọi `UnikeyBackspacePress()` và cập nhật preedit.
- Phím điều khiển hoặc điều hướng không phải văn bản: commit preedit đang có và để ứng dụng nhận phím.
- Phím keypad: commit preedit và để ứng dụng xử lý.
- Trạng thái Shift: được chuyển cho `UnikeySetCapsState()` để lõi engine có thể đưa ra quyết định liên quan đến chữ hoa chữ thường.

### 4.2 Ký tự in được

Các phím ASCII in được được đưa vào UniKey qua `UnikeyFilter(keyval)`.

Có một nhánh đặc biệt đáng chú ý trong [src/engine.cpp](src/engine.cpp): khi kiểu gõ là Telex hoặc Simple Telex 2 và tùy chọn `standalone-w-as-uw` bị tắt, ký tự `w` đứng riêng ở đầu từ có thể được chuyển thẳng qua thay vì bị hiểu là bộ biến đổi của Telex.

### 4.3 Hành vi preedit và commit

Lớp IBus giữ một buffer preedit kiểu `std::string`. Sau mỗi phím được xử lý:

- nó xóa ký tự khi `UnikeyBackspaces > 0`,
- nó nối thêm các byte đầu ra được sinh ra,
- và nó cập nhật preedit hiển thị.

Khi gặp ký tự ngắt từ hoặc nhánh control không xử lý, `ibus_unikey_buffer_commit()` sẽ commit chuỗi preedit vào ứng dụng.

## 5. UniKey phân loại và dịch phím như thế nào

Giai đoạn đầu tiên bên trong lõi UniKey là phân loại. Các bảng ánh xạ và enum sự kiện nằm trong [include/ukengine/core/inputproc.h](include/ukengine/core/inputproc.h).

Khái niệm trung tâm là `UkKeyEvent`, ghi lại:

- loại sự kiện,
- loại ký tự đã phân loại,
- ký hiệu từ vựng tiếng Việt,
- mã phím thô,
- và thông tin dấu.

Bộ xử lý input ánh xạ mã phím thô vào các hành động ngữ nghĩa. Những loại sự kiện chính được liệt kê dưới đây kèm định nghĩa ngắn và ví dụ.

| Sự kiện | Ý nghĩa | Ví dụ |
|---|---|---|
| `vneTone1` | Dấu sắc | `a` → <span style="font-size:2em;line-height:1">á</span> |
| `vneTone2` | Dấu huyền | `a` → <span style="font-size:2em;line-height:1">à</span> |
| `vneTone3` | Dấu hỏi | `a` → <span style="font-size:2em;line-height:1">ả</span> |
| `vneTone4` | Dấu ngã | `a` → <span style="font-size:2em;line-height:1">ã</span> |
| `vneTone5` | Dấu nặng | `a` → <span style="font-size:2em;line-height:1">ạ</span> |
| `vneRoof_a` | Dấu mũ trên `a` (â) | `aa` → <span style="font-size:2em;line-height:1">â</span> |
| `vneRoof_e` | Dấu mũ trên `e` (ê) | `ee` → <span style="font-size:2em;line-height:1">ê</span> |
| `vneRoof_o` | Dấu mũ trên `o` (ô) | `oo` → <span style="font-size:2em;line-height:1">ô</span> |
| `vneHook_u` | Dấu móc (horn) trên `u` (ư) | `uw` → <span style="font-size:2em;line-height:1">ư</span> |
| `vneHook_o` | Dấu móc trên `o` (ơ) | `ow` → <span style="font-size:2em;line-height:1">ơ</span> |
| `vneBowl` | Dấu bát / breve (ă) | `a8` hoặc `a(` → <span style="font-size:2em;line-height:1">ă</span> |
| `vneDd` | Chuyển `dd` → `đ` | `dd` → <span style="font-size:2em;line-height:1">đ</span> |
| `vne_telex_w` | Xử lý đặc biệt `w` trong Telex (tạo ă/ơ/ư) | `aw` → <span style="font-size:2em;line-height:1">ă</span>, `ow` → <span style="font-size:2em;line-height:1">ơ</span>, `uw` → <span style="font-size:2em;line-height:1">ư</span> |
| `vneMapChar` | Ánh xạ ký tự rõ ràng sang ký hiệu tiếng Việt | phím trực tiếp → <span style="font-size:2em;line-height:1">ô</span>/<span style="font-size:2em;line-height:1">ư</span> |
| `vneEscChar` | Escape / hủy hành động tiếng Việt | coi ký hiệu như ký tự thô |
| `vneNormal` | Phím bình thường: không ánh xạ tiếng Việt | `a`, `b`, `1` |

Đây là bước mà một chữ như `s` không còn chỉ là chữ Latin `s` nữa mà trở thành “đặt dấu sắc” theo luật Telex, hoặc `1` trở thành “dấu số 1” trong VNI.

## 6. Bộ đệm soạn thảo và các quy tắc tiếng Việt

Logic soạn thảo nằm trong [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp).

Bên trong, engine duy trì trạng thái từ và chuỗi gõ, bao gồm:

- bộ đệm từ hiện tại,
- vị trí dấu và dấu phụ,
- việc đoạn hiện tại có phải tiếng Việt hay không,
- vị trí thay đổi để cập nhật gia tăng,
- và danh sách phím gốc để phục hồi khi cần.

### 6.1 Đường xử lý chính

Hàm chính là `UkEngine::process(...)`. Nó phân loại phím rồi phân nhánh sang các routine như:

- `processTone()` để xử lý dấu thanh,
- `processDd()` cho `dd -> đ`,
- `appendVowel()` để ghép nguyên âm,
- `appendConsonnant()` để thêm phụ âm,
- `checkEscapeVIQR()` để xử lý escape của VIQR,
- `restoreKeyStrokes()` khi phải hoàn tác một chuyển đổi tiếng Việt.

### 6.2 Vì sao engine cần backspace

Soạn thảo tiếng Việt thường sửa những ký tự đã gõ trước đó. Ví dụ, một phím đặt dấu gõ sau có thể thay đổi nguyên âm đã nhập từ vài phím trước. Vì vậy engine trả về:

- số lượng ký tự hiển thị phải xóa,
- và chuỗi byte thay thế cho dạng ghép mới.

Đó là lý do lớp IBus phải áp dụng backspace lên preedit trước khi nối văn bản thay thế.

### 6.3 Khôi phục phím gốc

Engine hỗ trợ khôi phục chuỗi gõ ban đầu thông qua `UnikeyRestoreKeyStrokes()`, được lớp IBus dùng cho các phím tắt như Shift+Space và Shift+Shift. Việc này cho phép người dùng hủy một diễn giải tiếng Việt tự động và lấy lại chuỗi đã gõ thô.

## 7. Kiểu gõ và bảng ánh xạ bàn phím

Cần phân biệt rõ giữa:

- layout bàn phím vật lý do XKB và IBus xử lý,
- và kiểu gõ tiếng Việt do UniKey xử lý.

Engine descriptor của IBus dùng `layout="*"`, nghĩa là ibus-unikey không bị buộc vào một định nghĩa layout vật lý cụ thể. Nó chỉ chờ ngăn xếp ứng dụng chuyển vào các ký hiệu phím, sau đó UniKey diễn giải các ký hiệu đó theo kiểu gõ tiếng Việt đang chọn.

### 7.1 Các kiểu gõ được UI thiết lập IBus hiện tại cung cấp

UI thiết lập và schema GSettings hiện tại cung cấp bốn kiểu gõ:

1. `telex` (`UkTelex`) với tên hiển thị “Extend Telex”
2. `vni` (`UkVni`)
3. `stelex` (`UkSimpleTelex`)
4. `stelex2` (`UkSimpleTelex2`)

Chúng được định nghĩa trong [src/config/unikey_config.h](src/config/unikey_config.h) và schema cấu hình tại [src/config/org.freedesktop.ibus.engine.unikey.gschema.xml](src/config/org.freedesktop.ibus.engine.unikey.gschema.xml).

### 7.2 Các kiểu gõ khác có trong engine

Lõi UniKey còn chứa thêm các ánh xạ nội bộ cho:

- `UkViqr`
- `UkMsVi`
- `UkUsrIM` cho bảng gõ do người dùng nạp vào

Các kiểu này có trong mã engine và header, nhưng hiện chưa được lộ ra trong schema thiết lập IBus đang dùng.

### 7.3 Tóm tắt ánh xạ

#### Telex

Được định nghĩa bởi `TelexMethodMapping` trong [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp).
Các quy tắc điển hình được tóm tắt trong bảng sau.

| Phím | Hành động | Ví dụ |
|---|---|---|
| `s`, `f`, `r`, `x`, `j` | Phím dấu (sắc, huyền, hỏi, ngã, nặng) | `a` → <span style="font-size:2em;line-height:1">á</span>, <span style="font-size:2em;line-height:1">à</span>, <span style="font-size:2em;line-height:1">ả</span>, <span style="font-size:2em;line-height:1">ã</span>, <span style="font-size:2em;line-height:1">ạ</span> |
| `z` | Bỏ dấu / hủy | `á` + `z` → `a` |
| `aa` | Dấu mũ trên `a` | `aa` → <span style="font-size:2em;line-height:1">â</span> |
| `ee` | Dấu mũ trên `e` | `ee` → <span style="font-size:2em;line-height:1">ê</span> |
| `oo` | Dấu mũ trên `o` | `oo` → <span style="font-size:2em;line-height:1">ô</span> |
| `dd` | Chuyển `dd` → `đ` | `dd` → <span style="font-size:2em;line-height:1">đ</span> |
| `w` (Telex) | Tham gia tạo `ă`, `ơ`, `ư` | `aw` → <span style="font-size:2em;line-height:1">ă</span>, `ow` → <span style="font-size:2em;line-height:1">ơ</span>, `uw` → <span style="font-size:2em;line-height:1">ư</span> |

#### VNI

Được định nghĩa bởi `VniMethodMapping` trong [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp).
Các quy tắc VNI điển hình:

| Phím | Hành động | Ví dụ |
|---|---|---|
| `1`..`5` | Số dấu (1=sắc, 2=huyền, 3=hỏi, 4=ngã, 5=nặng) | `a1` → <span style="font-size:2em;line-height:1">á</span>, `a2` → <span style="font-size:2em;line-height:1">à</span> |
| `6`,`7`,`8`,`9` | Biến đổi hình nguyên âm và `đ` | `a6`/`a7` → <span style="font-size:2em;line-height:1">â</span>/<span style="font-size:2em;line-height:1">ă</span>, `d9` → <span style="font-size:2em;line-height:1">đ</span> |
| `0` | Reset / hủy | Hủy dấu đang hoạt động |

#### Simple Telex và Simple Telex 2

`SimpleTelexMethodMapping` và `SimpleTelex2MethodMapping` trong [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp) xác định các biến thể Telex rút gọn. Bảng sau so sánh Telex đầy đủ với chế độ đơn giản hóa.

| Tính năng | Telex đầy đủ | Simple Telex (đơn giản hóa) |
|---|---|---|
| Phím dấu | Dùng `s`, `f`, `r`, `x`, `j` cùng các quy tắc suy luận theo ngữ cảnh để đặt dấu lên nguyên âm đúng. Ví dụ: `a` + `s` → <span style="font-size:2em;line-height:1">á</span>. | Vẫn dùng các phím dấu đó, nhưng quy tắc đơn giản hơn và ít viết lại theo ngữ cảnh hơn. Ví dụ: `a` + `s` → <span style="font-size:2em;line-height:1">á</span>. |
| Dấu mũ và biến nguyên âm | `aa` → <span style="font-size:2em;line-height:1">â</span>, `ee` → <span style="font-size:2em;line-height:1">ê</span>, `oo` → <span style="font-size:2em;line-height:1">ô</span> theo logic ghép đầy đủ. | Các chuỗi đó vẫn cho cùng kết quả, nhưng chế độ đơn giản hóa áp dụng ít heuristic nhiều ký tự hơn. |
| Xử lý `w` | Dùng quy tắc theo ngữ cảnh: `aw` → <span style="font-size:2em;line-height:1">ă</span>, `ow` → <span style="font-size:2em;line-height:1">ơ</span>, `uw` → <span style="font-size:2em;line-height:1">ư</span>. | Thận trọng hơn với `w`; `SimpleTelex2` thường cho `w` đứng riêng ở đầu từ đi qua nguyên dạng. |
| `dd` thành `đ` | `dd` → <span style="font-size:2em;line-height:1">đ</span>. | `dd` → <span style="font-size:2em;line-height:1">đ</span>. |
| Hủy / bỏ dấu | `z` có thể hoàn tác nhiều bước ghép gần nhất. | `z` thường chỉ bỏ dấu vừa áp dụng, không can thiệp sâu vào phần ghép trước đó. |
| Đổi lại | Chính xác hơn và giàu ngữ cảnh hơn, nhưng dễ tạo thêm chuyển đổi ngoài ý muốn hơn. | Đơn giản hơn, dễ đoán hơn và ít thay đổi theo ngữ cảnh hơn. |

`SimpleTelex2` là một biến thể nhỏ, khác chủ yếu ở cách xử lý `w` tại ranh giới từ.

**Ví dụ minh họa**

| Chuỗi nhập | Telex đầy đủ | Simple Telex (đơn giản hóa) |
|---|---|---|
| `aw` | `aw` thành <span style="font-size:2em;line-height:1">ă</span>. | `aw` cũng thành <span style="font-size:2em;line-height:1">ă</span>. |
| `w` đứng riêng ở đầu từ | Có thể được hiểu là phím biến đổi trong một số ngữ cảnh. | Thường được giữ nguyên như ký tự `w`. |
| Phím dấu nhập sau khi đã có nhiều nguyên âm | Có thể dời dấu sang nguyên âm phù hợp theo ngữ cảnh. | Thường chỉ tác động lên nguyên âm vừa nhập gần nhất. |
| `z` sau nhiều lần ghép | Có thể hoàn tác nhiều bước ghép gần đây. | Thường chỉ xóa dấu vừa áp dụng. |

#### VIQR

Được định nghĩa bởi `VIQRMethodMapping` trong [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp), và còn được hỗ trợ sâu hơn bởi logic bảng mã VIQR trong [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp).

Các quy tắc VIQR điển hình:

| Ký tự | Hành động | Ví dụ |
|---|---|---|
| `'` | Dấu sắc | `a'` → <span style="font-size:2em;line-height:1">á</span> |
| `` ` `` | Dấu huyền | ``a` `` → <span style="font-size:2em;line-height:1">à</span> |
| `?` | Dấu hỏi | `a?` → <span style="font-size:2em;line-height:1">ả</span> |
| `~` | Dấu ngã | `a~` → <span style="font-size:2em;line-height:1">ã</span> |
| `.` | Dấu nặng | `a.` → <span style="font-size:2em;line-height:1">ạ</span> |
| `^` | Dấu mũ / biến nguyên âm | `a^` → <span style="font-size:2em;line-height:1">â</span>, `o^` → <span style="font-size:2em;line-height:1">ô</span> |

#### Tương thích Microsoft Vietnamese

`MsViMethodMapping` tồn tại để tương thích với hành vi nhập tiếng Việt kiểu Microsoft. Ví dụ điển hình sử dụng cùng các ký tự tiếng Việt như trên (ví dụ: `dd` → <span style="font-size:2em;line-height:1">đ</span>) nhưng với lựa chọn phím khác theo chuẩn Microsoft.

## 8. Bảng mã đầu ra, chuyển đổi và font

Việc chọn bảng mã đầu ra được cấu hình trong [src/config/unikey_config.h](src/config/unikey_config.h) và được hiện thực bởi các lớp chuyển đổi trong [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp).

### 8.1 Biểu diễn nội bộ của engine

Trong lúc soạn thảo, lõi UniKey không làm việc trực tiếp trên chuỗi UTF-8. Nó dùng một biểu diễn ký tự tiếng Việt chuẩn nội bộ (`StdVnChar`) rồi mới chuyển biểu diễn đó sang bảng mã đầu ra ở ranh giới cuối.

### 8.2 Các bảng mã đầu ra được lộ ra

Schema thiết lập IBus hiện tại lộ ra các bảng mã sau:

1. `unicode` -> `CONV_CHARSET_XUTF8`
2. `tcvn3` -> `CONV_CHARSET_TCVN3`
3. `vni-win` -> `CONV_CHARSET_VNIWIN`
4. `viqr` -> `CONV_CHARSET_VIQR`
5. `bk-hcm2` -> `CONV_CHARSET_BKHCM2`
6. `cstr` -> `CONV_CHARSET_UNI_CSTRING`
7. `ncr-dec` -> `CONV_CHARSET_UNIREF`
8. `ncr-hex` -> `CONV_CHARSET_UNIREF_HEX`

### 8.3 Các đối tượng chuyển đổi bảng mã

Lớp chuyển đổi bao gồm các converter cho:

- định dạng tiếng Việt nội bộ,
- bảng mã đơn byte cũ,
- bảng mã đôi byte,
- Unicode,
- Unicode ghép và tách,
- UTF-8,
- VIQR,
- đầu ra lai UTF-8 cộng VIQR.

`CVnCharsetLib` trong [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp) sở hữu và trả về các đối tượng converter này.

### 8.4 Font được xử lý ra sao

Quy tắc thực tế là:

- ibus-unikey xuất ra các byte văn bản theo bảng mã đã chọn,
- IBus và ứng dụng đích nhận các byte đó hoặc nhận văn bản Unicode,
- ứng dụng và font stack của hệ thống chịu trách nhiệm render glyph.

Điều đó có nghĩa là:

- với `unicode`, các font hệ thống hiện đại thường hiển thị đúng,
- với các bảng mã cũ như TCVN3 hoặc VNI-Win, văn bản chỉ hiển thị đúng nếu ứng dụng nhận đúng bảng mã đó và dùng font tương thích kiểu cũ.

Vì vậy ibus-unikey không “chuyển kèm font”. Nó chỉ chuyển văn bản đã được mã hóa. Việc đúng font là trách nhiệm nằm ngoài engine.

### 8.5 Vì sao preedit vẫn hiển thị bằng UTF-8

Ngay cả khi chọn một bảng mã đầu ra không phải UTF-8, frontend IBus vẫn cần một thứ có thể hiển thị được trong giao diện Linux. Vì vậy lớp IBus chuyển ngược các byte sinh ra thành UTF-8 cho phần preedit bằng `latinToUtf(...)` trong [src/engine.cpp](src/engine.cpp). Đây chỉ là tiện ích hiển thị, không phải thay đổi logic bảng mã đầu ra mà người dùng đã chọn.

## 9. Hệ thống macro

Macro là một hệ con riêng, nằm phía trên engine soạn thảo tiếng Việt.

### 9.1 Lưu trữ và nạp

- Đường dẫn file macro được định nghĩa là `~/.ibus/unikey/macro` bởi `UNIKEY_MACRO_FILE` trong [src/config/unikey_config.h](src/config/unikey_config.h).
- Engine IBus nạp file này lúc khởi động bằng cách gọi `UnikeyLoadMacroTable(...)` trong [src/engine.cpp](src/engine.cpp).
- API bọc tiếp tục chuyển yêu cầu này vào kho macro table trong [ukengine/engine/unikey.cpp](ukengine/engine/unikey.cpp).

### 9.2 Macro replacement chạy khi nào

Việc dò macro được thực hiện trong `UkEngine::macroMatch(...)` tại [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp).

Engine quét lùi trên từ đang có trong buffer, dựng một chuỗi khóa theo chuẩn tiếng Việt nội bộ, rồi thử tìm một entry tương ứng trong bảng macro.

### 9.3 Định dạng so khớp nội bộ

Khóa macro không được so khớp với byte đang hiển thị trên màn hình. Chúng được so khớp với biểu diễn chuẩn tiếng Việt nội bộ được lắp từ composition buffer. Điểm này rất quan trọng vì nó giúp macro hoạt động ổn định ngay cả khi bảng mã đầu ra nhìn thấy không phải UTF-8.

### 9.4 Xử lý chữ hoa chữ thường

Nếu macro khớp, chuỗi thay thế sẽ được chỉnh kiểu chữ theo chuỗi kích hoạt đã gõ:

- khóa toàn chữ thường -> thay thế thành chữ thường,
- khóa toàn chữ hoa -> thay thế thành chữ hoa,
- khóa trộn kiểu -> giữ nguyên văn bản macro đã lưu.

### 9.5 Chuyển đổi đầu ra và giữ lại ký tự phân cách

Sau khi macro khớp:

1. văn bản thay thế được chuyển từ định dạng chuẩn tiếng Việt nội bộ sang bảng mã đầu ra hiện tại,
2. ký tự kích hoạt hiện tại được nối thêm sau đầu ra macro khi cần,
3. engine reset trạng thái soạn thảo nhưng vẫn giữ đúng số backspace và byte đầu ra.

Điều này giải thích vì sao một macro có thể bung ra mà vẫn giữ được dấu cách hoặc phím enter cuối cùng đã kích hoạt nó.

### 9.6 UI chỉnh sửa macro trong ứng dụng setup

Ứng dụng setup nạp macro vào một GTK list store, cho phép người dùng sửa, rồi ghi ngược về đĩa.

Các file liên quan:

- [setup/controller/setup_controller.cpp](setup/controller/setup_controller.cpp)
- [setup/macro_utils.cpp](setup/macro_utils.cpp)
- [setup/ui/macro_dialog.ui](setup/ui/macro_dialog.ui)

UI setup chuyển đổi văn bản macro giữa định dạng chuẩn tiếng Việt nội bộ và UTF-8 để hiển thị và chỉnh sửa.

## 10. Cấu hình và ứng dụng thiết lập

Ứng dụng thiết lập desktop được build từ thư mục `setup/` và ghi cấu hình qua GSettings.

### 10.1 Mô hình lưu trữ

Cấu hình được lưu dưới schema:

- `org.freedesktop.ibus.engine.unikey`

Các khóa cấu hình quan trọng gồm:

- input method,
- output charset,
- spell check,
- auto restore non-Vietnamese,
- modern style,
- free marking,
- macro enabled,
- hành vi của `w` đứng riêng.

### 10.2 Luồng cập nhật

Luồng hoạt động là:

1. người dùng đổi một thiết lập trong UI setup,
2. `SetupController` ghi giá trị qua `ibus_unikey_config_set_*`,
3. GSettings lưu lại thay đổi,
4. engine nhận callback báo thay đổi,
5. engine IBus đang chạy cập nhật UniKey bằng `UnikeySetInputMethod()`, `UnikeySetOutputCharset()` hoặc `UnikeySetOptions()`.

Điều này có nghĩa là thay đổi cấu hình có thể áp dụng vào engine đang chạy mà không cần khởi động lại toàn bộ phiên desktop.

### 10.3 Điểm vào của phần More settings

Engine descriptor được tạo trong [src/engine/engine_app.cpp](src/engine/engine_app.cpp) đăng ký executable `ibus-setup-unikey`, nhờ đó IBus có thể chạy ứng dụng cấu hình đồ họa.

## 11. Tính năng và tùy chọn hành vi

Dựa trên schema hiện tại và mã engine, các tính năng nhìn thấy bởi người dùng chính là:

### 11.1 Chế độ tiếng Việt với nhiều kiểu gõ

Kiểu gõ đang chọn quyết định cách các phím Latin biến thành chữ tiếng Việt.

### 11.2 Nhiều bảng mã đầu ra

Cùng một văn bản tiếng Việt nội bộ có thể được xuất ra dưới dạng UTF-8, bảng mã cũ, VIQR, chuỗi escape kiểu C hoặc numeric character reference.

### 11.3 Kiểm tra chính tả

Tùy chọn này ảnh hưởng đến việc engine có áp dụng các luật hợp lệ của từ tiếng Việt trong lúc soạn hay không.

### 11.4 Tự khôi phục từ không phải tiếng Việt

Khi bật, engine có thể khôi phục chuỗi gõ ban đầu cho các từ không nên giữ ở dạng tiếng Việt đã biến đổi. Tính năng này phối hợp với kiểm tra chính tả.

### 11.5 Kiểu gõ hiện đại

Tùy chọn này điều khiển các lựa chọn chính tả như quy ước đặt dấu theo kiểu hiện đại được mô tả trong schema cấu hình.

### 11.6 Gõ tự do

Tùy chọn này nới lỏng một số ràng buộc về vị trí dấu và dấu phụ để người dùng có thể gõ thoải mái hơn.

### 11.7 Mở rộng macro

Khi bật, các chuỗi kích hoạt đã gõ có thể được thay bằng văn bản macro đã lưu.

### 11.8 `w` đứng riêng thành `ư`

Tùy chọn này thay đổi hành vi của Telex đối với `w` ở đầu từ và được xử lý một phần trong lớp tích hợp IBus.

### 11.9 Phím tắt khôi phục

Mô tả engine trong [src/engine/engine_app.cpp](src/engine/engine_app.cpp) có nhắc các phím tắt sau:

- Shift+Space
- Shift+Shift

Các phím này yêu cầu engine phục hồi chuỗi gõ gốc thay vì giữ phép biến đổi tiếng Việt.

## 12. Các ghi chú triển khai quan trọng

### 12.1 Dự án hiện chỉ lộ ra bốn kiểu gõ trong UI IBus, không phải toàn bộ kiểu gõ mà lõi UniKey có

UI setup hiện chỉ lộ Telex, VNI, Simple Telex và Simple Telex 2. Trong mã lõi vẫn tồn tại các ánh xạ khác như VIQR và tương thích Microsoft Vietnamese, nhưng chúng không chọn được từ các lựa chọn GSettings hiện đang ship.

### 12.2 Mã và phần mô tả người dùng chưa đồng bộ hoàn toàn ở mọi nơi

Ví dụ, [src/engine/engine_app.cpp](src/engine/engine_app.cpp) vẫn mô tả 7 bảng mã đầu ra trong help text, trong khi schema GSettings hiện tại lộ ra 8 lựa chọn bao gồm `bk-hcm2`.

### 12.3 Mã hiển thị và mã commit là hai vấn đề khác nhau

Preedit hiển thị trong giao diện IBus có thể được chuyển thành UTF-8 để hiển thị ngay cả khi bảng mã đầu ra đích là bảng mã cũ. Điều này là cần thiết cho giao diện và không nên bị hiểu nhầm là bảng mã logic mà người dùng đã chọn bị thay đổi.

### 12.4 Layout bàn phím vật lý không thuộc trách nhiệm của engine này

ibus-unikey diễn giải các giá trị phím logic mà IBus chuyển vào. Nó không cài đặt hay quản lý layout bàn phím vật lý theo nghĩa của XKB.

## 13. Bản đồ mã nguồn

Đây là các file quan trọng nhất nếu muốn lần theo hành vi của hệ thống:

- Điểm vào engine IBus và xử lý preedit: [src/engine.cpp](src/engine.cpp)
- Engine descriptor và help text hướng người dùng: [src/engine/engine_app.cpp](src/engine/engine_app.cpp)
- Khóa cấu hình và ánh xạ setting sang enum: [src/config/unikey_config.h](src/config/unikey_config.h)
- Schema GSettings: [src/config/org.freedesktop.ibus.engine.unikey.gschema.xml](src/config/org.freedesktop.ibus.engine.unikey.gschema.xml)
- API bọc công khai của UniKey: [ukengine/engine/unikey.cpp](ukengine/engine/unikey.cpp)
- Lõi engine soạn thảo: [ukengine/engine/ukengine.cpp](ukengine/engine/ukengine.cpp)
- Bảng ánh xạ kiểu gõ: [ukengine/core/inputproc.cpp](ukengine/core/inputproc.cpp)
- Định nghĩa sự kiện input: [include/ukengine/core/inputproc.h](include/ukengine/core/inputproc.h)
- Ký hiệu tiếng Việt và enum chuỗi ký tự: [include/ukengine/mapping/vnlexi.h](include/ukengine/mapping/vnlexi.h)
- Lớp chuyển đổi bảng mã: [ukengine/mapping/charset.cpp](ukengine/mapping/charset.cpp)
- Hằng số bảng mã và tùy chọn: [include/ukengine/mapping/vnconv.h](include/ukengine/mapping/vnconv.h)
- Luồng UI chỉnh macro: [setup/controller/setup_controller.cpp](setup/controller/setup_controller.cpp)
- Hàm trợ giúp GTK cho macro: [setup/macro_utils.cpp](setup/macro_utils.cpp)
