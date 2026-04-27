#include "signal_handlers.h"
#include "controller/setup_controller.h"

/**
 * @brief Bridge GTK main-window destroy signal to setup controller.
 * @param w Destroyed window widget.
 * @param user_data Optional signal user data.
 */
void on_main_window_destroy(GtkWidget *w, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMainWindowDestroy();
}

/**
 * @brief Bridge GTK key-press event to setup controller.
 * @param widget Source widget.
 * @param event Key event payload.
 * @param data Optional signal user data.
 * @return Controller handling result, or FALSE when controller is unavailable.
 */
gboolean on_main_window_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (auto controller = global_setup_controller())
        return controller->handleMainWindowKeyPress(widget, event);
    return FALSE;
}

/**
 * @brief Bridge close-button clicked signal to setup controller.
 * @param btn Close button widget.
 * @param user_data Optional signal user data.
 */
void on_btn_close_clicked(GtkButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleBtnClose();
}

/**
 * @brief Bridge setting-toggle change signal to setup controller.
 * @param btn Toggle button widget.
 * @param user_data Optional signal user data.
 */
void on_setting_toggled(GtkToggleButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleSettingToggled(btn);
}

/**
 * @brief Bridge setting-toggle realize signal to setup controller.
 * @param btn Toggle button widget.
 * @param user_data Optional signal user data.
 */
void on_setting_realize(GtkToggleButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleSettingRealize(btn);
}

/**
 * @brief Bridge top-level macro-edit button click to setup controller.
 * @param btn Macro edit button widget.
 * @param user_data Optional signal user data.
 */
void on_btn_macroedit_clicked(GtkButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroEdit();
}

/**
 * @brief Bridge input-method combo change signal to setup controller.
 * @param cbb Input-method combo box.
 * @param user_data Optional signal user data.
 */
void on_input_method_changed(GtkComboBox *cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleInputMethodChanged(cbb);
}

/**
 * @brief Bridge input-method combo realize signal to setup controller.
 * @param cbb Input-method combo box.
 * @param user_data Optional signal user data.
 */
void on_input_method_realize(GtkComboBox *cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleInputMethodRealize(cbb);
}

/**
 * @brief Bridge output-charset combo change signal to setup controller.
 * @param cbb Output-charset combo box.
 * @param user_data Optional signal user data.
 */
void on_output_charset_changed(GtkComboBox *cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleOutputCharsetChanged(cbb);
}

/**
 * @brief Bridge output-charset combo realize signal to setup controller.
 * @param cbb Output-charset combo box.
 * @param user_data Optional signal user data.
 */
void on_output_charset_realize(GtkComboBox *cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleOutputCharsetRealize(cbb);
}

/**
 * @brief Bridge macro-dialog delete-event signal to setup controller.
 * @param wid Macro dialog widget.
 * @param ev Delete-event payload.
 * @param user_data Optional signal user data.
 * @return Controller handling result, or TRUE when controller is unavailable.
 */
gboolean on_macro_dialog_delete(GtkWidget *wid, GdkEvent *ev, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        return controller->handleMacroDialogDelete();
    return TRUE;
}

/**
 * @brief Bridge macro-dialog hide request to setup controller.
 * @param btn Source button widget.
 * @param user_data Optional signal user data.
 */
void macro_dialog_hide(GtkButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroDialogHide();
}

/**
 * @brief Bridge macro delete action button click to setup controller.
 * @param button Delete button widget.
 * @param user_data Optional signal user data.
 */
void on_btn_macro_del_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroDel();
}

/**
 * @brief Bridge macro add action button click to setup controller.
 * @param button Add button widget.
 * @param user_data Optional signal user data.
 */
void on_btn_macro_add_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroAdd();
}

/**
 * @brief Bridge macro edit action button click to setup controller.
 * @param button Edit button widget.
 * @param user_data Optional signal user data.
 */
void on_btn_macro_edit_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroEditSelected();
}

/**
 * @brief Bridge macro clear action button click to setup controller.
 * @param button Clear button widget.
 * @param user_data Optional signal user data.
 */
void on_btn_macro_clear_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroClear();
}

/**
 * @brief Bridge macro return action button click to setup controller.
 * @param button Return button widget.
 * @param user_data Optional signal user data.
 */
void on_btn_macro_return_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroReturnToDefaultList();
}

/**
 * @brief Bridge macro import action button click to setup controller.
 * @param button Import button widget.
 * @param user_data Optional signal user data.
 */
void on_btn_macro_import_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroImport();
}

/**
 * @brief Bridge macro export action button click to setup controller.
 * @param button Export button widget.
 * @param user_data Optional signal user data.
 */
void on_btn_macro_export_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroExport();
}

/**
 * @brief Bridge search-entry activate (Enter) to setup controller.
 * @param entry Search entry widget.
 * @param user_data Optional signal user data.
 */
void on_macro_search_activate(GtkEntry *entry, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroSearchActivate();
}

/**
 * @brief Bridge search-entry key events to setup controller.
 * @param widget Search entry widget.
 * @param event Key event payload.
 * @param user_data Optional signal user data.
 * @return Controller handling result, or FALSE when unavailable.
 */
gboolean on_macro_search_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        return controller->handleMacroSearchKeyPress(widget, event);
    return FALSE;
}

/**
 * @brief Bridge macro-table row activation to setup controller.
 * @param tree_view Macro table tree view.
 * @param path Activated row path.
 * @param column Activated column.
 * @param user_data Optional signal user data.
 */
void on_tree_macro_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroTreeRowActivated(path);
}

/**
 * @brief Bridge legacy key-cell edited signal to setup controller.
 * @param celltext Edited cell renderer.
 * @param string_path GTK row path string.
 * @param newkey New key text entered by user.
 * @param user_data Optional signal user data.
 */
void on_cell_key_edited(GtkCellRendererText *celltext,
                        const gchar *string_path,
                        const gchar *newkey,
                        gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleCellKeyEdited(celltext, string_path, newkey);
}

/**
 * @brief Bridge legacy value-cell edited signal to setup controller.
 * @param celltext Edited cell renderer.
 * @param string_path GTK row path string.
 * @param newvalue New replacement text entered by user.
 * @param user_data Optional signal user data.
 */
void on_cell_value_edited(GtkCellRendererText *celltext,
                          const gchar *string_path,
                          const gchar *newvalue,
                          gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleCellValueEdited(celltext, string_path, newvalue);
}
