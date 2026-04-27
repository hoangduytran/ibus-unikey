# Test Plan — Previous Commit (Macro Engine + Persistence)

## Scope (English)

This plan validates the previous macro-related commit that introduced:
- dynamic macro storage behavior (no fixed 1024-item hard stop),
- deterministic duplicate-key behavior (`last wins`),
- load/save path hardening and error propagation,
- include/compatibility wrapper alignment that should not break builds.

## Phạm vi (Tiếng Việt)

Kế hoạch này xác thực commit trước đó liên quan macro:
- bỏ giới hạn cố định 1024 macro trong runtime,
- xử lý key trùng theo quy tắc xác định (`dòng cuối cùng thắng`),
- tăng độ an toàn đường dẫn load/save và thông báo lỗi,
- đồng bộ include wrapper/compatibility nhưng không làm vỡ build.

---

## Environment Setup (English)

Component under test:
- `ukengine/mapping/mactab.cpp`
- `include/ukengine/mapping/mactab.h`
- `setup/macro_utils.cpp`
- `tests/ukengine_test.cpp`

Run these commands from repo root:

```bash
cmake -S . -B build
cmake --build build -j
```

Macro test data location:
- Use macro files under `tests/macros` for all scenarios in this plan.
- Dùng các tệp macro trong `tests/macros` cho mọi trường hợp cần tập tin macros trong kế hoạch này.
- If a file is missing, create it in `tests/macros` using the provided snippets.
- Nếu thiếu file, hãy tạo file đó trong `tests/macros` bằng các snippet đã cung cấp.
- Temporary instruction: do not use YAML (`*.yaml`) or PLIST (`*.plist`) macro files in this plan yet.
- Hướng dẫn tạm thời: chưa sử dụng file macro YAML (`*.yaml`) hoặc PLIST (`*.plist`) trong kế hoạch này.

## Chuẩn bị môi trường (Tiếng Việt)

Thành phần kiểm thử:
- `ukengine/mapping/mactab.cpp`
- `include/ukengine/mapping/mactab.h`
- `setup/macro_utils.cpp`
- `tests/ukengine_test.cpp`

Chạy lệnh tại thư mục gốc:

```bash
cmake -S . -B build
cmake --build build -j
```

Vị trí dữ liệu test macro:
- Dùng các file macro trong `tests/macros` cho mọi trường hợp của tài liệu này.
- Nếu chưa có file, tạo trực tiếp trong `tests/macros` theo snippet bên dưới.

---

## Test Group A — Capacity and Large Input

### A1. More than 1024 entries are accepted

**Part tested (English)**  
Macro in-memory growth and add flow.

**Phần được test (Tiếng Việt)**  
Tăng dung lượng bộ nhớ và luồng add macro trong RAM.

**Test condition (English)**  
Prepare a macro file with at least 1025 distinct keys.

**Điều kiện test (Tiếng Việt)**  
Tạo file macro có tối thiểu 1025 key khác nhau.

**Validation code (English + Vietnamese)**  

```bash
mkdir -p tests/macros
python3 - <<'PY'
with open('tests/macros/macro_1025.txt', 'w', encoding='utf-8') as f:
    for i in range(1025):
        f.write(f'k{i}:value_{i}\n')
PY
```

Use setup import path (or engine loader utility) to load `tests/macros/macro_1025.txt`.

**Expected result (English)**  
- All rows are loaded (no silent truncation at 1024).
- No crash or out-of-bounds symptoms.

**Kết quả mong đợi (Tiếng Việt)**  
- Tất cả dòng được nạp (không bị cắt âm thầm ở mốc 1024).
- Không crash, không lỗi truy cập bộ nhớ.

---

### A2. Large table save and reload round-trip

**Part tested (English)**  
Persistence consistency for large datasets.

**Phần được test (Tiếng Việt)**  
Tính nhất quán save/reload với tập dữ liệu lớn.

**Test condition (English)**  
Save a table with >1024 entries, reload it, compare count and sample values.

**Điều kiện test (Tiếng Việt)**  
Lưu bảng >1024 entries, nạp lại, đối chiếu số lượng và một số giá trị mẫu.

**Validation code**

```bash
cp tests/macros/macro_1025.txt tests/macros/macro_1025_saved.txt
wc -l tests/macros/macro_1025_saved.txt
```

Then import/export once in setup macro dialog and diff:

```bash
diff -u tests/macros/macro_1025.txt tests/macros/macro_1025_saved.txt | sed -n '1,80p'
```

**Expected result (English)**  
- Entry count preserved.
- No unexplained entry loss.

**Kết quả mong đợi (Tiếng Việt)**  
- Số lượng dòng được giữ nguyên.
- Không mất dòng bất thường.

---

## Test Group B — Duplicate-Key Policy (`last wins`)

### B1. Duplicate keys in same file

**Part tested (English)**  
Duplicate normalization during load and model conversion.

**Phần được test (Tiếng Việt)**  
Chuẩn hóa key trùng khi load và khi đổi model.

**Test condition (English)**  
Create file with repeated key where last occurrence should be effective.

**Điều kiện test (Tiếng Việt)**  
Tạo file có key lặp lại, dòng cuối cùng phải có hiệu lực.

**Validation code**

```bash
mkdir -p tests/macros
cat > tests/macros/macro_dup.txt <<'EOF'
sig:old
hello:world
sig:new
EOF
```

Load and verify `sig` expands to `new`.

**Expected result (English)**  
Lookup/UI shows `sig -> new`, not `sig -> old`.

**Kết quả mong đợi (Tiếng Việt)**  
Tra cứu/UI hiện `sig -> new`, không phải `sig -> old`.

---

## Test Group C — Failure and Error Reporting

### C1. Invalid line parse handling

**Part tested (English)**  
`addItem` parse failure path and summary error behavior.

**Phần được test (Tiếng Việt)**  
Nhánh lỗi parse `addItem` và thông báo tổng hợp lỗi.

**Test condition (English)**  
Include malformed row without `:` and import/export via setup.

**Điều kiện test (Tiếng Việt)**  
Chèn dòng lỗi không có dấu `:` và import/export từ setup.

**Validation code**

```bash
mkdir -p tests/macros
cat > tests/macros/macro_bad.txt <<'EOF'
ok:value
broken_line_without_colon
ok2:value2
EOF
```

**Expected result (English)**  
- Import/export failure is surfaced with user-visible message when write is incomplete.
- Operation does not silently claim full success.

**Kết quả mong đợi (Tiếng Việt)**  
- Lỗi được hiện cho người dùng khi ghi không đầy đủ.
- Không được báo thành công 100% một cách im lặng.

---

## Test Group D — Build and Header Compatibility

### D1. Full build still succeeds

**Part tested (English)**  
Wrapper include alignment and canonical include path compatibility.

**Phần được test (Tiếng Việt)**  
Tính tương thích sau khi chuẩn hóa include wrapper.

**Validation code**

```bash
cmake -S . -B build
cmake --build build -j
```

**Expected result (English)**  
All targets compile and link successfully.

**Kết quả mong đợi (Tiếng Việt)**  
Tất cả target build và link thành công.

---

## Quick Regression Checklist (English)

- [ ] >1024 entries load/save correctly  
- [ ] Duplicate keys resolve with `last wins`  
- [ ] Parse/convert failures are visible and actionable  
- [ ] Project builds cleanly after changes

## Checklist hồi quy nhanh (Tiếng Việt)

- [ ] Nạp/lưu >1024 entries đúng  
- [ ] Key trùng xử lý theo `last wins`  
- [ ] Lỗi parse/convert hiện rõ, có hướng xử lý  
- [ ] Toàn bộ project build sạch sau thay đổi

