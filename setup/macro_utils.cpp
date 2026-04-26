#include <cstring>
#include <libintl.h>

#include "keycons.h"
#include "macro_utils.h"

#define _(str) gettext(str)

// Search the list store for an existing macro key.
// @param store list store containing macro rows
// @param check_key key string to search for
// @return TRUE if the key exists in the store, otherwise FALSE
gboolean list_store_check_exists(GtkListStore *store, gchar *check_key)
{
    GtkTreeIter iter;
    gchar *key;
    auto model = GTK_TREE_MODEL(store);
    auto b = gtk_tree_model_get_iter_first(model, &iter);
    while (b)
    {
        gtk_tree_model_get(model, &iter, COL_KEY, &key, -1);
        if (strcasecmp(key, check_key) == 0)
        {
            g_free(key);
            return true;
        }
        g_free(key);

        b = gtk_tree_model_iter_next(model, &iter);
    }

    return false;
}

// Append each macro entry from the macro table to the GTK list store.
// Existing list contents are preserved and duplicate keys are skipped.
// Macro text and keys are converted from VN standard charset to XUTF8.
// @param list destination list store
// @param macro source macro table
void list_store_append(GtkListStore *list, CMacroTable *macro)
{
    gchar key[MAX_MACRO_KEY_LEN * 3];
    gchar value[MAX_MACRO_TEXT_LEN * 3];
    UKBYTE *p;

    for (int i = 0; i < macro->getCount(); i++)
    {
        // Convert the stored macro key to XUTF8 for display.
        p = (UKBYTE *)macro->getKey(i);
        int inLen = -1;
        int maxOutLen = sizeof(key);
        int ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_XUTF8,
                            p, (UKBYTE *)key,
                            &inLen, &maxOutLen);
        if (ret != 0)
            continue;

        if (list_store_check_exists(list, key))
            continue;

        // Convert the stored macro value to XUTF8 for display.
        p = (UKBYTE *)macro->getText(i);
        inLen = -1;
        maxOutLen = sizeof(value);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_XUTF8,
                        p, (UKBYTE *)value,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue;

        // Append the converted macro row to the list store.
        GtkTreeIter iter;
        gtk_list_store_append(list, &iter);
        gtk_list_store_set(list, &iter, COL_KEY, key, COL_VALUE, value, -1);
    }
}

// Populate the list store from the macro table and ensure a placeholder row.
// @param macro source macro table
// @param store destination GTK list store
void unikey_macro_to_store(CMacroTable *macro, GtkListStore *store)
{
    gtk_list_store_clear(store);

    list_store_append(store, macro);
    list_store_add_null_item(store);
}

UnikeyMacroTableFillResult unikey_gtk_model_fill_macro_table(GtkTreeModel *model, CMacroTable *macro, gboolean reset_table_first)
{
    UnikeyMacroTableFillResult r;
    r.total = 0;
    r.added = 0;
    r.failed = 0;
    if (reset_table_first)
        macro->resetContent();

    GtkTreeIter iter;
    gboolean b = gtk_tree_model_get_iter_first(model, &iter);
    while (b == TRUE)
    {
        gchar *key, *value;
        gtk_tree_model_get(model, &iter, COL_KEY, &key, COL_VALUE, &value, -1);

        if (strcasecmp(key, STR_NULL_ITEM) != 0)
        {
            r.total++;
            if (macro->addItem(key, value, CONV_CHARSET_XUTF8) < 0)
                r.failed++;
            else
                r.added++;
        }
        g_free(key);
        g_free(value);

        b = gtk_tree_model_iter_next(model, &iter);
    }
    return r;
}

UnikeyMacroTableFillResult unikey_store_to_macro(GtkListStore *store, CMacroTable *macro)
{
    return unikey_gtk_model_fill_macro_table(GTK_TREE_MODEL(store), macro, TRUE);
}

// Add a placeholder row to the list store when no empty row already exists.
// @param list target GTK list store
void list_store_add_null_item(GtkListStore *list)
{
    GtkTreeIter iter;

    auto n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list), NULL);
    if (n > 0)
    {
        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list), &iter, NULL, n - 1);
        gchar *key;
        gtk_tree_model_get(GTK_TREE_MODEL(list), &iter, COL_KEY, &key, -1);
        if (strcmp(key, STR_NULL_ITEM) == 0)
        {
            g_free(key);
            return;
        }
        g_free(key);
    }

    // If the last item is a real macro or the list is empty, add a placeholder row.
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       COL_KEY, STR_NULL_ITEM,
                       COL_VALUE, STR_NULL_ITEM,
                       -1);
}
