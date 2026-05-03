// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_INTERCHANGE_COMMON_H
#define IBUS_UNIKEY_MACRO_INTERCHANGE_COMMON_H

#include <glib.h>

#include <setup/macro_file_io.h>

#include <string>
#include <vector>

#include <ukengine/mapping/charset.h>

class CMacroTable;

namespace macro_interchange {

GQuark error_quark();

/** Sets @a err when non-null; safe when @a err is null. */
void fail(GError **err, const char *msg);

bool read_file_utf8(const gchar *path, std::string *out, GError **err);

std::string path_suffix_lower(const gchar *path);

int compare_std_vn_keys(const StdVnChar *a, const StdVnChar *b);

bool vn_std_to_utf8_grow(const StdVnChar *src, std::vector<char> &buf, bool *ok);

bool utf8_from_std_keytext(const StdVnChar *vn, std::string *utf8_out);

/** Row indices sorted by folded StdVn trigger key (same order as text export). */
std::vector<int> sorted_macro_row_indices(const CMacroTable *table);

} // namespace macro_interchange

#endif /* IBUS_UNIKEY_MACRO_INTERCHANGE_COMMON_H */
