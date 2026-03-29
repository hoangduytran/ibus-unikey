#include "setup_controller.h"
#include "mactab.h"
#include "macro_utils.h"
#include "unikey_config.h"
#include <libintl.h>
#include <gdk/gdkkeysyms.h>

static SetupController* s_global_controller = nullptr;

SetupController* global_setup_controller() { return s_global_controller; }
void global_setup_controller_set(SetupController* c) { s_global_controller = c; }

#define _(str) gettext(str)

SetupController::SetupController(SetupView& view, SettingsStore& store)
    : m_view(view), m_store(store)
{
}

void SetupController::init()
{
    // Nothing to do beyond view init in this minimal refactor.
}

void SetupController::handleMainWindowDestroy() { gtk_main_quit(); }

gboolean SetupController::handleMainWindowKeyPress(GtkWidget *widget, GdkEventKey *event)
{
    if (event->keyval == GDK_KEY_Escape) {
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

void SetupController::handleBtnClose() { gtk_main_quit(); }

void SetupController::cbbConfigUpdate(GtkComboBox* cbb, const char* key)
{
    GValue val = {0};
    GtkTreeIter iter;
    GtkTreeModel* model = gtk_combo_box_get_model(cbb);

    if (!gtk_combo_box_get_active_iter(cbb, &iter))
        return;

    gtk_tree_model_get_value(model, &iter, 0, &val);
    m_store.setString(key, g_value_get_string(&val));
    g_value_unset(&val);
}

void SetupController::handleInputMethodChanged(GtkComboBox* cbb) { cbbConfigUpdate(cbb, CONFIG_INPUTMETHOD); }
void SetupController::handleOutputCharsetChanged(GtkComboBox* cbb) { cbbConfigUpdate(cbb, CONFIG_OUTPUTCHARSET); }

void SetupController::cbbConfigSetActive(GtkComboBox* cbb, const char* key)
{
    std::string im = (key == CONFIG_INPUTMETHOD ? m_store.getInputMethod() : m_store.getOutputCharset());
    if (im.empty())
        return;

    GValue val = {0};
    GtkTreeIter iter;
    GtkTreeModel* model = gtk_combo_box_get_model(cbb);
    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    do {
        gtk_tree_model_get_value(model, &iter, 0, &val);
        const gchar* item = g_value_get_string(&val);
        if (item && im == item) {
            gtk_combo_box_set_active_iter(cbb, &iter);
            g_value_unset(&val);
            return;
        }
        g_value_unset(&val);
    } while (gtk_tree_model_iter_next(model, &iter));
}

void SetupController::handleComboBoxRealize(GtkComboBox* cbb, const std::string& key)
{
    cbbConfigSetActive(cbb, key.c_str());
}

void SetupController::handleSettingToggled(GtkToggleButton* btn)
{
    const gchar* name = gtk_widget_get_name(GTK_WIDGET(btn));
    if (!name || strncmp(name, "cfg_", 4) != 0)
        return;

    gchar *key = g_strdup(name + 4);
    gboolean b = gtk_toggle_button_get_active(btn);
    m_store.setBoolean(key, b);
    g_free(key);
}

void SetupController::handleSettingRealize(GtkToggleButton* btn)
{
    const gchar* name = gtk_widget_get_name(GTK_WIDGET(btn));
    if (!name || strncmp(name, "cfg_", 4) != 0)
        return;

    gchar *key = g_strdup(name + 4);
    bool value;
    if (m_store.getBoolean(key, value))
        gtk_toggle_button_set_active(btn, value);
    g_free(key);
}

void SetupController::handleMacroEdit()
{
    gchar* macrofile = get_macro_file();
    CMacroTable macro;

    macro.init();
    macro.loadFromFile(macrofile);

    auto store = GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));
    unikey_macro_to_store(&macro, store);

    m_view.showMacroDialog();

    int ret = gtk_dialog_run(GTK_DIALOG(m_view.getMacroDialog()));
    if (ret == GTK_RESPONSE_OK)
    {
        unikey_store_to_macro(store, &macro);

        GFile* f = g_file_get_parent(g_file_new_for_path(macrofile));
        if (!g_file_query_exists(f, NULL))
        {
            g_file_make_directory_with_parents(f, NULL, NULL);
        }
        g_object_unref(f);

        macro.writeToFile(macrofile);
    }

    g_free(macrofile);
}

gboolean SetupController::handleMacroDialogDelete()
{
    m_view.hideMacroDialog();
    return TRUE;
}

void SetupController::handleMacroDialogHide() { m_view.hideMacroDialog(); }

void SetupController::handleCellKeyEdited(GtkCellRendererText *celltext, const gchar *string_path, const gchar *newkey)
{
    GtkTreeIter iter;
    gchar *oldkey;
    gchar nkey[MAX_MACRO_KEY_LEN];

    auto model = gtk_tree_view_get_model(m_view.getMacroTree());

    strncpy(nkey, newkey, MAX_MACRO_KEY_LEN - 1);
    nkey[MAX_MACRO_KEY_LEN - 1] = '\0';

    if (strcmp(nkey, STR_NULL_ITEM) == 0 || (strlen(STR_NULL_ITEM) != 0 && strlen(nkey) == 0))
        return;

    if (list_store_check_exists(GTK_LIST_STORE(model), nkey))
        return;

    gtk_tree_model_get_iter_from_string(model, &iter, string_path);
    gtk_tree_model_get(model, &iter, COL_KEY, &oldkey, -1);
    if (strcmp(oldkey, STR_NULL_ITEM) == 0)
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COL_KEY, nkey, COL_VALUE, MACRO_DEFAULT_VALUE, -1);
    else
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COL_KEY, nkey, -1);
    g_free(oldkey);

    list_store_add_null_item(GTK_LIST_STORE(model));
}

void SetupController::handleCellValueEdited(GtkCellRendererText *celltext, const gchar *string_path, const gchar *newvalue)
{
    GtkTreeIter iter;
    gchar *key;
    gchar value[MAX_MACRO_TEXT_LEN];

    auto model = gtk_tree_view_get_model(m_view.getMacroTree());
    gtk_tree_model_get_iter_from_string(model, &iter, string_path);
    gtk_tree_model_get(model, &iter, COL_KEY, &key, -1);

    strncpy(value, newvalue, MAX_MACRO_TEXT_LEN - 1);
    value[MAX_MACRO_TEXT_LEN - 1] = '\0';

    if (strcmp(key, STR_NULL_ITEM) != 0)
    {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COL_VALUE, value, -1);
    }
    g_free(key);
}

void SetupController::handleMacroDel()
{
    GtkTreeIter iter;
    auto select = gtk_tree_view_get_selection(m_view.getMacroTree());
    if (gtk_tree_selection_get_selected(select, NULL, &iter) == TRUE)
    {
        auto store = GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));
        gchar *key;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_KEY, &key, -1);
        if (strcmp(key, STR_NULL_ITEM) != 0)
        {
            gtk_list_store_remove(store, &iter);
        }
        gtk_tree_selection_select_iter(select, &iter);
        g_free(key);
    }
}

void SetupController::handleMacroClear()
{
    auto store = GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));
    gtk_list_store_clear(store);
    list_store_add_null_item(store);

    auto select = gtk_tree_view_get_selection(m_view.getMacroTree());
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
    {
        gtk_tree_selection_select_iter(select, &iter);
    }
}

void SetupController::handleMacroImport()
{
    GtkWidget *file = gtk_file_chooser_dialog_new(_("Import macro"),
                                       GTK_WINDOW(m_view.getMacroDialog()),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       dgettext("gtk30", "_Cancel"), GTK_RESPONSE_CANCEL,
                                       dgettext("gtk30", "_Open"), GTK_RESPONSE_OK,
                                       NULL);

    if (gtk_dialog_run(GTK_DIALOG(file)) == GTK_RESPONSE_OK)
    {
        gchar* fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file));
        CMacroTable macro;
        macro.init();
        macro.loadFromFile(fn);
        g_free(fn);

        auto store = GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));

        GtkTreeIter iter;
        auto n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, n - 1);
        gtk_list_store_remove(store, &iter);

        list_store_append(store, &macro);
        list_store_add_null_item(store);

        auto select = gtk_tree_view_get_selection(m_view.getMacroTree());
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
            gtk_tree_selection_select_iter(select, &iter);
    }

    gtk_widget_destroy(file);
}

void SetupController::handleMacroExport()
{
    GtkWidget *file = gtk_file_chooser_dialog_new(_("Export macro"),
                                       GTK_WINDOW(m_view.getMacroDialog()),
                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                       dgettext("gtk30", "_Cancel"), GTK_RESPONSE_CANCEL,
                                       dgettext("gtk30", "_Save"), GTK_RESPONSE_OK,
                                       NULL);

    if (gtk_dialog_run(GTK_DIALOG(file)) == GTK_RESPONSE_OK)
    {
        CMacroTable macro;
        macro.init();

        auto model = GTK_TREE_MODEL(gtk_tree_view_get_model(m_view.getMacroTree()));
        GtkTreeIter iter;
        gchar *key, *value;
        gboolean b = gtk_tree_model_get_iter_first(model, &iter);
        while (b == TRUE)
        {
            gtk_tree_model_get(model, &iter, COL_KEY, &key, COL_VALUE, &value, -1);
            if (strcasecmp(key, STR_NULL_ITEM) != 0)
            {
                macro.addItem(key, value, CONV_CHARSET_XUTF8);
            }
            g_free(key);
            g_free(value);
            b = gtk_tree_model_iter_next(model, &iter);
        }

        gchar* fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file));
        macro.writeToFile(fn);
        g_free(fn);
    }

    gtk_widget_destroy(file);
}
