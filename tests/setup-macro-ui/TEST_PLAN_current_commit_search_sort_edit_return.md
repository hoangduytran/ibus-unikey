# Test Plan — Current Changes (Setup Macro UI Search/Sort/Edit/Return)

## Scope (English)

This plan validates the current UI changes:
- two-list behavior (`default list` + `search list`),
- action icon bar (`Add`, `Edit`, `Delete`, `Clear`, `Return`),
- non-inline editing with dedicated editor dialog,
- search activation and return behavior,
- `Return` button visibility rules.

## Phạm vi (Tiếng Việt)

Kế hoạch này xác thực thay đổi UI hiện tại:
- mô hình 2 danh sách (`default list` + `search list`),
- thanh nút icon (`Add`, `Edit`, `Delete`, `Clear`, `Return`),
- bỏ inline edit, dùng editor dialog riêng,
- hành vi bật/tắt search list,
- quy tắc hiện/ẩn nút `Return`.

---

## Environment Setup (English)

Component under test:
- `setup/ui/macro_dialog.ui`
- `setup/controller/setup_controller.cpp/.h`
- `setup/signal_handlers.cpp/.h`
- `setup/ui/setup_view.cpp/.h`

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

Run setup app:

```bash
./build/setup/ibus-setup-unikey
```

Macro test data location:
- Use macro scenario files under `tests/macros` when importing test data.
- Keep deterministic test fixtures in `tests/macros` so all testers use the same inputs.
- Interchange formats (YAML, plist, JSON, CSV) are exercised by the loader tests and [`tests/macros/TEST_PLAN_macro_interchange_io.md`](../macros/TEST_PLAN_macro_interchange_io.md); use small fixtures first when probing import-merge behavior in this UI plan.

## Chuẩn bị môi trường (Tiếng Việt)

Thành phần kiểm thử:
- `setup/ui/macro_dialog.ui`
- `setup/controller/setup_controller.cpp/.h`
- `setup/signal_handlers.cpp/.h`
- `setup/ui/setup_view.cpp/.h`

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

Chạy setup app:

```bash
./build/setup/ibus-setup-unikey
```

Vị trí dữ liệu test macro:
- Dùng các file kịch bản macro trong `tests/macros` khi import dữ liệu test.
- Giữ fixture cố định trong `tests/macros` để mọi người test cùng một đầu vào.
- Định dạng trao đổi (YAML, plist, JSON, CSV) được kiểm tra trong test loader và [`tests/macros/TEST_PLAN_macro_interchange_io.md`](../macros/TEST_PLAN_macro_interchange_io.md); khi thử import trong kế hoạch UI này nên bắt đầu với fixture nhỏ.

---

## Test Group A — Icon Action Bar and Visibility

### A1. Icon-only buttons render and tooltips work

**Part tested (English)**  
UI icon bar and hover labels.

**Phần được test (Tiếng Việt)**  
Thanh icon và tooltip khi hover.

**Condition (English)**  
Open macro dialog, inspect bottom action bar.

**Điều kiện (Tiếng Việt)**  
Mở macro dialog và quan sát thanh nút dưới bảng.

**Steps**
1. Click macro-edit button in main setup window.
2. Verify only icon is visible for `Add/Edit/Delete/Clear/Return`.
3. Hover each icon and check tooltip text.

**Expected result (English)**  
- Icon-only display (no visible label text).
- Tooltip labels appear on hover.

**Kết quả mong đợi (Tiếng Việt)**  
- Hiện icon, không hiện text trên nút.
- Tooltip hiển thị đúng tên nút khi hover.

---

## Shared macro fixtures for this plan (English)

Before running UI scenarios, prepare shared macro files in `tests/macros`:

```bash
mkdir -p tests/macros
cat > tests/macros/ui_base_macros.txt <<'EOF'
sig:Best regards
addr:123 Main Street
hello:Xin chào bạn
thu_cam_on:Cảm ơn bạn rất nhiều
EOF
```

Import `tests/macros/ui_base_macros.txt` in the macro dialog before executing groups B/C/D.

## Bộ dữ liệu macro dùng chung cho kế hoạch này (Tiếng Việt)

Trước khi chạy các kịch bản UI, chuẩn bị file dùng chung trong `tests/macros`:

```bash
mkdir -p tests/macros
cat > tests/macros/ui_base_macros.txt <<'EOF'
sig:Best regards
addr:123 Main Street
hello:Xin chao ban
EOF
```

Hãy import `tests/macros/ui_base_macros.txt` trong macro dialog trước khi chạy nhóm B/C/D.
File mẫu này chứa cả nội dung tiếng Anh và tiếng Việt (có dấu) để kiểm tra Unicode end-to-end.

---

### A2. `Return` button visibility toggles by search-list state

**Part tested (English)**  
`Return` visibility policy.

**Phần được test (Tiếng Việt)**  
Quy tắc hiện/ẩn nút `Return`.

**Condition (English)**  
Search query that yields results, then return to default list.

**Điều kiện (Tiếng Việt)**  
Nhập query có kết quả, sau đó quay lại default list.

**Steps**
1. Ensure some macro rows exist.
2. Search with query matching at least one row and press Enter.
3. Confirm `Return` button is visible.
4. Click `Return`.
5. Confirm default list is shown and `Return` is hidden.

**Expected result (English)**  
- `Return` visible only while active search list has found entries.
- Hidden when back in default list.

**Kết quả mong đợi (Tiếng Việt)**  
- `Return` chỉ hiện khi đang ở search list và có kết quả.
- Quay về default list thì `Return` ẩn.

---

## Test Group B — Search List Behavior

### B1. Enter activates search list

**Part tested (English)**  
Search projection activation path.

**Phần được test (Tiếng Việt)**  
Nhánh kích hoạt danh sách kết quả tìm kiếm.

**Steps**
1. Type query in search box.
2. Press Enter.

**Expected result (English)**  
- Table switches to search-result subset.
- Rows shown are only matching rows.

**Kết quả mong đợi (Tiếng Việt)**  
- Bảng chuyển sang tập kết quả tìm kiếm.
- Chỉ hiển thị các dòng khớp query.

---

### B2. Esc does not return to default list in active search mode

**Part tested (English)**  
Updated escape-key policy.

**Phần được test (Tiếng Việt)**  
Chính sách phím Esc mới trong search mode.

**Steps**
1. Enter search mode with matching results.
2. Press Esc.

**Expected result (English)**  
- Still in search list.
- Return to default list requires `Return` button.

**Kết quả mong đợi (Tiếng Việt)**  
- Vẫn ở search list.
- Muốn về default list phải bấm nút `Return`.

---

## Test Group C — Edit/Add/Delete/Clear in Both Modes

### C1. Add in default list

**Part tested (English)**  
Add action and editor dialog save flow.

**Phần được test (Tiếng Việt)**  
Nút Add và luồng Save trong editor dialog.

**Steps**
1. In default list, click `Add`.
2. Fill `Replace` and `With`.
3. Save.

**Expected result (English)**  
- New row appears in table.
- Data persists after dialog Save and reopen.

**Kết quả mong đợi (Tiếng Việt)**  
- Dòng mới xuất hiện trong bảng.
- Dữ liệu còn sau khi Save và mở lại dialog.

---

### C2. Edit by row activation and by icon

**Part tested (English)**  
Dual edit triggers and canonical update path.

**Phần được test (Tiếng Việt)**  
Hai cách mở edit và cập nhật canonical row.

**Steps**
1. Select row, click `Edit`, modify content, Save.
2. Double-click a row, modify content, Save.

**Expected result (English)**  
- Both triggers open editor.
- Saved content reflected immediately in table.

**Kết quả mong đợi (Tiếng Việt)**  
- Cả hai cách đều mở được editor.
- Dữ liệu sau Save cập nhật ngay trên bảng.

---

### C3. Delete in search list updates canonical row set

**Part tested (English)**  
Search-list delete through canonical index mapping.

**Phần được test (Tiếng Việt)**  
Xóa trong search list thông qua mapping canonical index.

**Condition**  
Need at least one matching row in search list.

**Steps**
1. Enter search list.
2. Select row and click `Delete`.
3. Click `Return` to default list.

**Expected result (English)**  
- Deleted row disappears from search list immediately.
- Row is also absent in default list.

**Kết quả mong đợi (Tiếng Việt)**  
- Dòng bị xóa mất ngay trong search list.
- Quay về default list cũng không còn dòng đó.

---

### C4. `Clear` mode-dependent behavior

**Part tested (English)**  
Split clear semantics.

**Phần được test (Tiếng Việt)**  
Hành vi `Clear` theo mode.

**Steps (search mode)**
1. Enter search list.
2. Click `Clear`.

**Expected (search mode)**
- Only search state is cleared.
- Returns to default list; data not deleted.

**Steps (default mode)**
1. In default list, click `Clear`.
2. Confirm in warning dialog.

**Expected (default mode)**
- All rows removed.
- This is destructive and explicit.

**Kết quả mong đợi (Tiếng Việt)**
- Search mode: chỉ xóa kết quả tìm, không xóa dữ liệu gốc.
- Default mode: xóa toàn bộ dữ liệu sau khi xác nhận.

---

## Test Group D — Sorting in Default and Search Lists

### D1. Column sort works in default list

**Part tested (English)**  
Tree-view sort integration for active list.

**Phần được test (Tiếng Việt)**  
Sắp xếp cột cho danh sách đang active.

**Steps**
1. In default list, click `Word` column header repeatedly.
2. Repeat with `Replace with` header.

**Expected result (English)**  
- Ordering changes as expected by sort direction.

**Kết quả mong đợi (Tiếng Việt)**  
- Thứ tự dòng thay đổi đúng theo hướng sắp xếp.

---

### D2. Column sort works in search list

**Part tested (English)**  
Search-list sorting and canonical mapping integrity.

**Phần được test (Tiếng Việt)**  
Sắp xếp trong search list và toàn vẹn mapping canonical.

**Steps**
1. Enter search list with multiple results.
2. Sort by each column.
3. Edit one row after sort.

**Expected result (English)**  
- Search-list sorting works.
- Edit still applies to correct canonical row.

**Kết quả mong đợi (Tiếng Việt)**  
- Search list sắp xếp được.
- Edit vẫn cập nhật đúng dòng canonical.

---

## Test Group E — Build and Runtime Smoke

### E1. Build and launch smoke test

**Validation code**

```bash
cmake -S . -B build
cmake --build build -j
./build/setup/ibus-setup-unikey
```

**Expected result (English)**  
No build/link errors; setup app launches and macro dialog functions.

**Kết quả mong đợi (Tiếng Việt)**  
Không lỗi build/link; app setup mở được và macro dialog hoạt động.

---

## Quick Regression Checklist (English)

- [ ] `Return` button appears only in active search list with results  
- [ ] `Esc` does not exit search-list mode  
- [ ] `Return` exits search list to default list and hides itself  
- [ ] `Clear` behavior differs correctly by mode  
- [ ] Add/Edit/Delete work in both default/search contexts  
- [ ] Sorting works in both default and search lists  

## Checklist hồi quy nhanh (Tiếng Việt)

- [ ] Nút `Return` chỉ hiện khi search list đang active và có kết quả  
- [ ] `Esc` không thoát search list  
- [ ] `Return` đưa về default list và tự ẩn  
- [ ] `Clear` khác nhau đúng theo mode  
- [ ] Add/Edit/Delete chạy đúng ở cả default/search  
- [ ] Sắp xếp chạy đúng ở cả default và search list

