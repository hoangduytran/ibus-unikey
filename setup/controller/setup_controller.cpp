#include "setup_controller.h"

#include <cstring>
#include <gdk/gdkkeysyms.h>
#include <libintl.h>

#include "macro_utils.h"
#include "mactab.h"
#include "unikey_config.h"

static SetupController* s_global_controller = nullptr;

SetupController* global_setup_controller()
{
    return s_global_controller;
}

void global_setup_controller_set(SetupController* c)
{
    s_global_controller = c;
}

#define _(str) gettext(str)

SetupController::SetupController(SetupView& view, SettingsStore& store)
    : m_view(view), m_store(store)
{
}

void SetupController::init()
{
}

gboolean SetupController::handleMainWindowKeyPress(GtkWidget *widget, GdkEventKey *event)
{
    if (event->keyval == GDK_KEY_Escape) {
        gtk_main_quit();
        return true;
    }
    return false;
}

void SetupController::handleMainWindowDestroy()
{
    gtk_main_quit();
}

void SetupController::handleBtnClose()
{
    gtk_main_quit();
}

void SetupController::cbbConfigUpdate(GtkComboBox* cbb, const char* key)
{
    GValue val = {0};
    GtkTreeIter iter;
    GtkTreeModel* model;

    model = gtk_combo_box_get_model(cbb);
    gtk_combo_box_get_active_iter(cbb, &iter);
    gtk_tree_model_get_value(model, &iter, 0, &val);

    ibus_unikey_config_set_string(key, g_value_get_string(&val));

    g_value_unset(&val);
}

void SetupController::handleInputMethodChanged(GtkComboBox* cbb)
{
    cbbConfigUpdate(cbb, CONFIG_INPUTMETHOD);
}

void SetupController::handleOutputCharsetChanged(GtkComboBox* cbb)
{
    cbbConfigUpdate(cbb, CONFIG_OUTPUTCHARSET);
}

void SetupController::handleInputMethodRealize(GtkComboBox* cbb)
{
    cbbConfigSetActive(cbb, CONFIG_INPUTMETHOD);
}

void SetupController::handleOutputCharsetRealize(GtkComboBox* cbb)
{
    cbbConfigSetActive(cbb, CONFIG_OUTPUTCHARSET);
}

void SetupController::cbbConfigSetActive(GtkComboBox* cbb, const char* key)
{
    GValue val = {0};
    GtkTreeIter iter;
    GtkTreeModel* model;

    gchar *im;
    if (!ibus_unikey_config_get_string(key, &im))
    {
        return;
    }

    model = gtk_combo_box_get_model(cbb);
    gtk_tree_model_get_iter_first(model, &iter);
    do
    {
        gtk_tree_model_get_value(model, &iter, 0, &val);
        if (strcmp(im, g_value_get_string(&val)) == 0)
        {
            gtk_combo_box_set_active_iter(cbb, &iter);
            g_value_unset(&val);
            break;
        }
        g_value_unset(&val);
    } while (gtk_tree_model_iter_next(model, &iter));

    g_free(im);
}

void SetupController::handleSettingToggled(GtkToggleButton* btn)
{
    const gchar* key = gtk_widget_get_name(GTK_WIDGET(btn));
    key = key + 4; // skip "cfg_"

    gboolean b = gtk_toggle_button_get_active(btn);
    ibus_unikey_config_set_boolean(key, b);
}

void SetupController::handleSettingRealize(GtkToggleButton* btn)
{
    const gchar* key = gtk_widget_get_name(GTK_WIDGET(btn));
    key = key + 4; // skip "cfg_"

    gboolean b;
    if (ibus_unikey_config_get_boolean(key, &b))
    {
        gtk_toggle_button_set_active(btn, b);
    }
}

void SetupController::handleMacroEdit()
{
    gchar* macrofile = get_macro_file();

    CMacroTable macro;
    macro.init();
    macro.loadFromFile(macrofile);

    auto store = GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));
    unikey_macro_to_store(&macro, store);

    gtk_widget_show_all(m_view.getMacroDialog());
    gtk_window_present(GTK_WINDOW(m_view.getMacroDialog()));

    int ret = gtk_dialog_run(GTK_DIALOG(m_view.getMacroDialog()));
    if (ret == GTK_RESPONSE_OK)
    {
        unikey_store_to_macro(store, &macro);

        GFile* f = g_file_get_parent(g_file_new_for_path(macrofile));
        if (g_file_query_exists(f, NULL) == FALSE)
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
    gtk_widget_hide(m_view.getMacroDialog());
    return true;
}

void SetupController::handleMacroDialogHide()
{
    gtk_widget_hide(m_view.getMacroDialog());
}

void SetupController::handleCellKeyEdited(GtkCellRendererText *celltext,
                    const gchar *string_path,
                    const gchar *newkey)
{
    GtkTreeIter iter;
    gchar *oldkey;
    gchar nkey[MAX_MACRO_KEY_LEN];

    auto model = gtk_tree_view_get_model(m_view.getMacroTree());

    strncpy(nkey, newkey, MAX_MACRO_KEY_LEN-1);
    nkey[MAX_MACRO_KEY_LEN-1] = '\0';

    if (strcmp(nkey, STR_NULL_ITEM) == 0
        || (strlen(STR_NULL_ITEM) != 0 && strlen(nkey) == 0))
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

void SetupController::handleCellValueEdited(GtkCellRendererText *celltext,
                     const gchar *string_path,
                     const gchar *newvalue)
{
    GtkTreeIter iter;
    gchar *key;
    gchar value[MAX_MACRO_TEXT_LEN];

    auto model = gtk_tree_view_get_model(m_view.getMacroTree());
    gtk_tree_model_get_iter_from_string(model, &iter, string_path);
    gtk_tree_model_get(model, &iter, COL_KEY, &key, -1);

    strncpy(value, newvalue, MAX_MACRO_TEXT_LEN-1);
    value[MAX_MACRO_TEXT_LEN-1] = '\0';

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
        gtk_tree_selection_select_iter(select, &iter); // select current index
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
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    gtk_tree_selection_select_iter(select, &iter);
}

void SetupController::handleMacroImport()
{
    auto file = gtk_file_chooser_dialog_new(_("Import macro"),
                                       GTK_WINDOW(m_view.getMacroDialog()),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       dgettext("gtk30", "_Cancel"), GTK_RESPONSE_CANCEL,
                                       dgettext("gtk30", "_Open"), GTK_RESPONSE_OK,
                                       NULL);

    if (gtk_dialog_run(GTK_DIALOG(file)) == GTK_RESPONSE_OK)
    {

        auto fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file));
        CMacroTable macro;
        macro.init();
        macro.loadFromFile(fn);
        g_free(fn);

        auto store = GTK_LIST_STORE(gtk_tree_view_get_model(m_view.getMacroTree()));

        GtkTreeIter iter;
        auto n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);     // get number of iter
        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, n-1); // get last iter
        gtk_list_store_remove(store, &iter); // remove last iter (...)

        list_store_append(store, &macro);
        list_store_add_null_item(store); // add iter (...)

        // select first iter
        auto select = gtk_tree_view_get_selection(m_view.getMacroTree());
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
        gtk_tree_selection_select_iter(select, &iter);
    }

    gtk_widget_destroy(file);
}

void SetupController::handleMacroExport()
{
    auto file = gtk_file_chooser_dialog_new(_("Export macro"),
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
        gchar* key, *value;
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

        auto fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file));
        macro.writeToFile(fn);
        g_free(fn);
    }

    gtk_widget_destroy(file);
}