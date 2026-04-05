#ifndef __SETUP_MACRO_UTIL_H__
#define __SETUP_MACRO_UTIL_H__

#include <gtk/gtk.h>
#include "mactab.h"

// Column indices used by macro list store rows.
enum
{
    COL_KEY = 0,
    COL_VALUE
};

// Placeholder text shown for empty macro table rows.
#define STR_NULL_ITEM "..."

// Default macro text value used when a new key is added without a user value.
#define MACRO_DEFAULT_VALUE "(replace text)"

// Check whether the given key already exists in the list store.
// @param store list store to search
// @param check_key key string to search for
// @return TRUE when the key exists, FALSE otherwise
gboolean list_store_check_exists(GtkListStore *store, gchar *check_key);

// Ensure the list store always contains an empty placeholder row.
// If the last row is already the placeholder, no action is taken.
// @param list targeted list store
void list_store_add_null_item(GtkListStore *list);

// Append all macro entries from the macro table into the list store.
// Existing entries in the list store are preserved.
// @param list target GTK list store for macro entries
// @param macro source macro table to copy from
void list_store_append(GtkListStore *list, CMacroTable *macro);

// Convert the macro table entries into GTK list store rows.
// This clears the store first, appends all macro entries, and adds a placeholder row.
// @param macro macro table source
// @param store destination GTK list store
void unikey_macro_to_store(CMacroTable *macro, GtkListStore *store);

// Convert GTK list store rows back into the macro table.
// Rows with placeholder keys are skipped.
// @param store source GTK list store
// @param macro destination macro table
void unikey_store_to_macro(GtkListStore *store, CMacroTable *macro);

#endif
