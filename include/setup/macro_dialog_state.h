// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
#ifndef IBUS_UNIKEY_MACRO_DIALOG_STATE_H
#define IBUS_UNIKEY_MACRO_DIALOG_STATE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

gchar *macro_dialog_get_last_import_dir(void);
gchar *macro_dialog_get_last_export_dir(void);

void macro_dialog_remember_import_file(const gchar *path);
void macro_dialog_remember_export_file(const gchar *path);

#ifdef __cplusplus
}
#endif

#endif /* IBUS_UNIKEY_MACRO_DIALOG_STATE_H */
