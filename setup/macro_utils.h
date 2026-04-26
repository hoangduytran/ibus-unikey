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

/**
 * @brief Result of filling a CMacroTable from GtkTreeModel rows (non-placeholder only).
 */
struct UnikeyMacroTableFillResult
{
    int total;  /**< non-placeholder rows seen */
    int added;  /**< addItem success count */
    int failed; /**< addItem returned -1 */
};

// Fill a macro table from a tree model. Set reset_table_first to clear the table first
// (e.g. full sync from a list store). If false, append into the current table (caller usually init()'d it).
UnikeyMacroTableFillResult unikey_gtk_model_fill_macro_table(GtkTreeModel *model, CMacroTable *macro, gboolean reset_table_first);

// Convert GTK list store rows back into the macro table (resets the table first).
// @return counts for UI to detect silent failures
UnikeyMacroTableFillResult unikey_store_to_macro(GtkListStore *store, CMacroTable *macro);

#endif
