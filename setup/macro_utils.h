#ifndef __SETUP_MACRO_UTIL_H__
#define __SETUP_MACRO_UTIL_H__

#include <gtk/gtk.h>
#include <ukengine/mapping/mactab.h>

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

/**
 * @brief Append macro entries from a CMacroTable into a list store.
 *
 * Duplicate keys (after UTF-8 casefold) keep only the last row from the table (last wins).
 * Existing list rows are left in place; caller usually merges into an existing list when importing.
 *
 * @param list destination GTK list store
 * @param macro source engine macro table
 */
void list_store_append(GtkListStore *list, CMacroTable *macro);

// Convert the macro table entries into GTK list store rows.
// This clears the store first, appends all macro entries, and adds a placeholder row.
// @param macro macro table source
// @param store destination GTK list store
void unikey_macro_to_store(CMacroTable *macro, GtkListStore *store);

/**
 * @brief Counters when syncing GtkTreeModel rows into CMacroTable.
 */
struct UnikeyMacroTableFillResult
{
    int total;  /**< non-placeholder rows scanned */
    int added;  /**< successful addItem calls (after last-wins merge, one add per surviving row) */
    int failed; /**< addItem returned -1 */
};

/**
 * @brief Read macro rows from a tree model and fill CMacroTable.
 *
 * Duplicate keys (UTF-8 casefold) only keep the last row in model order (last wins) before
 * calling addItem. Set @a reset_table_first to clear the table first (e.g. full list-store sync);
 * if false, only call after macro->init() on an empty table for export.
 *
 * @param model GTK tree model (macro dialog columns)
 * @param macro destination table
 * @param reset_table_first if TRUE, call resetContent() first
 */
UnikeyMacroTableFillResult unikey_gtk_model_fill_macro_table(GtkTreeModel *model, CMacroTable *macro, gboolean reset_table_first);

/**
 * @brief Replace engine table from a list store: reset, fill with last-wins, return counts.
 */
UnikeyMacroTableFillResult unikey_store_to_macro(GtkListStore *store, CMacroTable *macro);

#endif
