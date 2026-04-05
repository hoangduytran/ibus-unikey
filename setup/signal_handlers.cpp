#include "signal_handlers.h"
#include "controller/setup_controller.h"
#include "unikey_config.h"

// Bridge GTK main window destroy signal to the setup controller.
void on_main_window_destroy(GtkWidget *w, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMainWindowDestroy();
}

// Bridge GTK key press events to the setup controller.
// @return controller handling result, or FALSE if no controller is present.
gboolean on_main_window_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (auto controller = global_setup_controller())
        return controller->handleMainWindowKeyPress(widget, event);
    return FALSE;
}

// Bridge the close button clicked signal to the setup controller.
void on_btn_close_clicked(GtkButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleBtnClose();
}

// Bridge setting toggle events to the setup controller.
void on_setting_toggled(GtkToggleButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleSettingToggled(btn);
}

// Bridge setting toggle realization events to the setup controller.
void on_setting_realize(GtkToggleButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleSettingRealize(btn);
}

// Bridge macro edit button clicks to the setup controller.
void on_btn_macroedit_clicked(GtkButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroEdit();
}

// Bridge input method combo box changes to the setup controller.
void on_input_method_changed(GtkComboBox *cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleInputMethodChanged(cbb);
}

// Bridge input method combo box realization to the setup controller.
void on_input_method_realize(GtkComboBox *cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleInputMethodRealize(cbb);
}

// Bridge output charset combo box changes to the setup controller.
void on_output_charset_changed(GtkComboBox *cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleOutputCharsetChanged(cbb);
}

// Bridge output charset combo box realization to the setup controller.
void on_output_charset_realize(GtkComboBox *cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleOutputCharsetRealize(cbb);
}

// Bridge macro dialog delete events to the setup controller.
// @return the controller delete handling result, or TRUE if no controller is present.
gboolean on_macro_dialog_delete(GtkWidget *wid, GdkEvent *ev, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        return controller->handleMacroDialogDelete();
    return TRUE;
}

// Bridge hide dialog requests to the setup controller.
void macro_dialog_hide(GtkButton *btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroDialogHide();
}

// Bridge macro delete button clicks to the setup controller.
void on_btn_macro_del_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroDel();
}

// Bridge macro clear button clicks to the setup controller.
void on_btn_macro_clear_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroClear();
}

// Bridge macro import button clicks to the setup controller.
void on_btn_macro_import_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroImport();
}

// Bridge macro export button clicks to the setup controller.
void on_btn_macro_export_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroExport();
}

// Bridge macro key cell edits to the setup controller.
void on_cell_key_edited(GtkCellRendererText *celltext,
                        const gchar *string_path,
                        const gchar *newkey,
                        gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleCellKeyEdited(celltext, string_path, newkey);
}

// Bridge macro value cell edits to the setup controller.
void on_cell_value_edited(GtkCellRendererText *celltext,
                          const gchar *string_path,
                          const gchar *newvalue,
                          gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleCellValueEdited(celltext, string_path, newvalue);
}
