#include <cstring>
#include <libintl.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "keycons.h"
#include "macro_utils.h"

#define _(str) gettext(str)

// Case-fold a UTF-8 key for last-wins map (duplicates: last line wins)
static std::string macroKeyFoldForMap(const char *utf8)
{
    gchar *f = g_utf8_casefold(utf8, -1);
    std::string s(f ? f : "");
    g_free(f);
    return s;
}

// Append each macro entry from the macro table to the GTK list store.
// Duplicate keys: only the last occurrence in the table is shown (last wins).
// Macro text and keys are converted from VN standard charset to XUTF8.
// @param list destination list store
// @param macro source macro table
void list_store_append(GtkListStore *list, CMacroTable *macro)
{
    const int n = macro->getCount();
    if (n <= 0)
        return;

    gchar key[MAX_MACRO_KEY_LEN * 3];
    gchar value[MAX_MACRO_TEXT_LEN * 3];
    UKBYTE *p;
    int inLen, maxOutLen, ret;

    std::unordered_map<std::string, int> last_index;
    for (int i = 0; i < n; i++)
    {
        p = (UKBYTE *)macro->getKey(i);
        inLen = -1;
        maxOutLen = sizeof(key);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_XUTF8,
                        p, (UKBYTE *)key,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue;
        const std::string fk = macroKeyFoldForMap(key);
        last_index[fk] = i;
    }

    for (int i = 0; i < n; i++)
    {
        p = (UKBYTE *)macro->getKey(i);
        inLen = -1;
        maxOutLen = sizeof(key);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_XUTF8,
                        p, (UKBYTE *)key,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue;
        const std::string fold = macroKeyFoldForMap(key);
        if (last_index[fold] != i)
            continue;

        p = (UKBYTE *)macro->getText(i);
        inLen = -1;
        maxOutLen = sizeof(value);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_XUTF8,
                        p, (UKBYTE *)value,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue;

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

    struct Row
    {
        std::string k;
        std::string v;
    };
    std::vector<Row> rows;
    {
        GtkTreeIter iter;
        gboolean b = gtk_tree_model_get_iter_first(model, &iter);
        while (b == TRUE)
        {
            gchar *key, *value;
            gtk_tree_model_get(model, &iter, COL_KEY, &key, COL_VALUE, &value, -1);
            if (strcasecmp(key, STR_NULL_ITEM) != 0)
                rows.push_back(Row{std::string(key), std::string(value)});
            g_free(key);
            g_free(value);
            b = gtk_tree_model_iter_next(model, &iter);
        }
    }

    std::unordered_map<std::string, int> last_row;
    for (size_t i = 0; i < rows.size(); i++)
    {
        r.total++;
        last_row[macroKeyFoldForMap(rows[i].k.c_str())] = (int)i;
    }

    for (size_t i = 0; i < rows.size(); i++)
    {
        const std::string fk = macroKeyFoldForMap(rows[i].k.c_str());
        if (last_row[fk] != (int)i)
            continue;
        if (macro->addItem(rows[i].k.c_str(), rows[i].v.c_str(), CONV_CHARSET_XUTF8) < 0)
            r.failed++;
        else
            r.added++;
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
