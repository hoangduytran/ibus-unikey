// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_EXPORT_FILENAME_H
#define IBUS_UNIKEY_MACRO_EXPORT_FILENAME_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

gboolean macro_export_filename_has_extension_conflict(const gchar *basename_utf8,
                                                      const gchar *preferred_extension_utf8);

gchar *macro_export_filename_complete(const gchar *path_utf8, const gchar *preferred_extension_utf8);

#ifdef __cplusplus
}
#endif

#endif /* IBUS_UNIKEY_MACRO_EXPORT_FILENAME_H */
