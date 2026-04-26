#include "setup_controller.h"

#include <cstring>
#include <gdk/gdkkeysyms.h>
#include <libintl.h>

#include "macro_utils.h"
#include "mactab.h"
#include "unikey_config.h"

static SetupController* s_global_controller = nullptr;

// Return the globally registered setup controller instance.
// @return registered global SetupController pointer
SetupController* global_setup_controller()
{
    return s_global_controller;
}

// Register the global setup controller instance.
// @param c controller instance to register globally
void global_setup_controller_set(SetupController* c)
{
    s_global_controller = c;
}

#define _(str) gettext(str)

// Show a modal error for macro engine failures (uses CMacroTable::getLastErrorMessage()).
static void run_macro_error_dialog(GtkWindow *parent, CMacroTable *macro, const char *summary)
{
    const char *d = macro->getLastErrorMessage();
    if (d == NULL || d[0] == '\0')
        d = _("(no details)");
    gchar *text = g_strdup_printf("%s\n%s", summary, d);
    GtkWidget *dlg = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        "%s", text);
    g_free(text);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

// Construct the setup controller using the UI view and settings store.
// @param view reference to the view wrapper used to access UI widgets
// @param store reference to persistent settings storage backend
SetupController::SetupController(SetupView& view, SettingsStore& store)
    : m_view(view), m_store(store)
{
}

// Initialize the controller. This method is currently a placeholder
// for any future setup-specific initialization.
void SetupController::init()
{
}

// Handle key press events in the main setup window.
// @param widget widget receiving the key event
// @param event key event details
// @return TRUE when the event is handled and should not be propagated
//         FALSE otherwise
gboolean SetupController::handleMainWindowKeyPress(GtkWidget *widget, GdkEventKey *event)
{
    if (event->keyval == GDK_KEY_Escape) {
        gtk_main_quit();
        return true;
    }
    return false;
}

// Handle the main window being destroyed by terminating the GTK main loop.
void SetupController::handleMainWindowDestroy()
{
    gtk_main_quit();
}

// Handle the close button being pressed by terminating the GTK main loop.
void SetupController::handleBtnClose()
{
    gtk_main_quit();
}

// Update the stored configuration for the selected combo box value.
// @param cbb combo box containing the selected value
// @param key configuration key to update
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

// Handle changes to the input method selection.
// @param cbb combo box containing input method choices
void SetupController::handleInputMethodChanged(GtkComboBox* cbb)
{
    cbbConfigUpdate(cbb, CONFIG_INPUTMETHOD);
}

// Handle changes to the output charset selection.
// @param cbb combo box containing output charset choices
void SetupController::handleOutputCharsetChanged(GtkComboBox* cbb)
{
    cbbConfigUpdate(cbb, CONFIG_OUTPUTCHARSET);
}

// Initialize the input method combo box from stored settings.
// @param cbb combo box containing input method choices
void SetupController::handleInputMethodRealize(GtkComboBox* cbb)
{
    cbbConfigSetActive(cbb, CONFIG_INPUTMETHOD);
}

// Initialize the output charset combo box from stored settings.
// @param cbb combo box containing output charset choices
void SetupController::handleOutputCharsetRealize(GtkComboBox* cbb)
{
    cbbConfigSetActive(cbb, CONFIG_OUTPUTCHARSET);
}

// Set the active combo box item based on a stored configuration value.
// @param cbb combo box to update
// @param key configuration key to read the value from
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

// Persist a GUI toggle setting to configuration storage.
// @param btn toggle button for the setting
void SetupController::handleSettingToggled(GtkToggleButton* btn)
{
    const gchar* key = gtk_widget_get_name(GTK_WIDGET(btn));
    key = key + 4; // skip "cfg_"

    gboolean b = gtk_toggle_button_get_active(btn);
    ibus_unikey_config_set_boolean(key, b);
}

// Initialize a GUI toggle button from stored configuration.
// @param btn toggle button for the setting
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

// Show the macro editor dialog and save macro definitions if confirmed.
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
        UnikeyMacroTableFillResult sync = unikey_store_to_macro(store, &macro);
        if (sync.failed > 0)
        {
            run_macro_error_dialog(
                GTK_WINDOW(m_view.getMacroDialog()),
                &macro,
                _("Not all macros could be saved. The macro file was not written."));
        }
        else
        {
            GFile* f = g_file_get_parent(g_file_new_for_path(macrofile));
            if (g_file_query_exists(f, NULL) == FALSE)
            {
                g_file_make_directory_with_parents(f, NULL, NULL);
            }
            g_object_unref(f);

            if (!macro.writeToFile(macrofile))
            {
                run_macro_error_dialog(
                    GTK_WINDOW(m_view.getMacroDialog()),
                    &macro,
                    _("Could not write the macro file."));
            }
        }
    }

    g_free(macrofile);
}

// Hide the macro dialog and report the delete event handled.
// @return TRUE always to indicate event handling
gboolean SetupController::handleMacroDialogDelete()
{
    gtk_widget_hide(m_view.getMacroDialog());
    return true;
}

// Hide the macro dialog without action.
void SetupController::handleMacroDialogHide()
{
    gtk_widget_hide(m_view.getMacroDialog());
}

// Handle editing a macro key cell.
// @param celltext cell renderer for the edited text cell
// @param string_path string path to the edited row
// @param newkey new key text entered by user
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

// Handle editing a macro value cell.
// @param celltext cell renderer for the edited text cell
// @param string_path string path to the edited row
// @param newvalue new value text entered by user
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

// Remove the currently selected macro entry if present.
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

// Clear all macro entries and restore the empty-placeholder row.
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

// Show a file chooser to import macro definitions from disk.
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
        if (!macro.loadFromFile(fn))
        {
            run_macro_error_dialog(
                GTK_WINDOW(m_view.getMacroDialog()),
                &macro,
                _("The macro file could not be fully imported."));
        }
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

// Show a file chooser to export macro definitions to disk.
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
        UnikeyMacroTableFillResult fill = unikey_gtk_model_fill_macro_table(model, &macro, FALSE);

        auto fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file));
        if (fill.failed > 0)
        {
            run_macro_error_dialog(
                GTK_WINDOW(m_view.getMacroDialog()),
                &macro,
                _("Not all macros could be written to the export file."));
        }
        else if (!macro.writeToFile(fn))
        {
            run_macro_error_dialog(
                GTK_WINDOW(m_view.getMacroDialog()),
                &macro,
                _("Could not write the export file."));
        }
        g_free(fn);
    }

    gtk_widget_destroy(file);
}
