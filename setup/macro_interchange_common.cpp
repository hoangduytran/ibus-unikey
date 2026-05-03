// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <setup/macro_interchange_common.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include <ukengine/mapping/mactab.h>
#include <ukengine/mapping/vnconv.h>

namespace macro_interchange {

#define STD_TO_LOWER(x)                                                                            \
  (((x) >= VnStdCharOffset && (x) < (VnStdCharOffset + TOTAL_ALPHA_VNCHARS) && !((x) & 1))         \
       ? (x + 1)                                                                                   \
       : (x))

GQuark error_quark(void) { return g_quark_from_static_string("ibus-unikey-macro-interchange-error"); }

void fail(GError **err, const char *msg) {
  if (err)
    g_set_error(err, error_quark(), 0, "%s", msg);
}

bool read_file_utf8(const gchar *path, std::string *out, GError **err) {
  gchar *contents = nullptr;
  gsize len = 0;
  if (!g_file_get_contents(path, &contents, &len, err))
    return false;
  out->assign(contents, len);
  g_free(contents);
  return true;
}

std::string path_suffix_lower(const gchar *path) {
  if (!path)
    return "";
  const char *dot = strrchr(path, '.');
  if (!dot)
    return "";
  std::string s(dot);
  for (char &c : s)
    c = (char)tolower((unsigned char)c);
  return s;
}

int compare_std_vn_keys(const StdVnChar *a, const StdVnChar *b) {
  int i = 0;
  StdVnChar ls1, ls2;
  for (;; i++) {
    ls1 = STD_TO_LOWER(a[i]);
    ls2 = STD_TO_LOWER(b[i]);
    if (ls1 > ls2)
      return 1;
    if (ls1 < ls2)
      return -1;
    if (a[i] == 0)
      return (b[i] == 0) ? 0 : -1;
  }
}

bool vn_std_to_utf8_grow(const StdVnChar *src, std::vector<char> &buf, bool *ok) {
  *ok = false;
  buf.resize(256);
  for (int attempt = 0; attempt < 24; attempt++) {
    int inLen = -1;
    int maxOut = (int)buf.size();
    int ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_UNIUTF8, (UKBYTE *)src, (UKBYTE *)buf.data(),
                        &inLen, &maxOut);
    if (ret == 0) {
      buf.resize((size_t)maxOut);
      *ok = true;
      return true;
    }
    if (buf.size() > (size_t)64 * 1024 * 1024)
      return false;
    buf.resize(buf.size() * 2);
  }
  return false;
}

bool utf8_from_std_keytext(const StdVnChar *vn, std::string *utf8_out) {
  std::vector<char> buf;
  bool ok = false;
  if (!vn_std_to_utf8_grow(vn, buf, &ok) || !ok)
    return false;
  while (!buf.empty() && buf.back() == '\0')
    buf.pop_back();
  utf8_out->assign(buf.data(), buf.size());
  return true;
}

std::vector<int> sorted_macro_row_indices(const CMacroTable *table) {
  const int n = table->getCount();
  std::vector<int> order((size_t)n);
  for (int i = 0; i < n; i++)
    order[(size_t)i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return compare_std_vn_keys(table->getKey(a), table->getKey(b)) < 0;
  });
  return order;
}

} // namespace macro_interchange
