#include "signal_handlers.h"
#include "controller/setup_controller.h"
#include "unikey_config.h"

void on_main_window_destroy(GtkWidget* w, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMainWindowDestroy();
}

gboolean on_main_window_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (auto controller = global_setup_controller())
        return controller->handleMainWindowKeyPress(widget, event);
    return FALSE;
}

void on_btn_close_clicked(GtkButton* btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleBtnClose();
}

void on_setting_toggled(GtkToggleButton* btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleSettingToggled(btn);
}

void on_setting_realize(GtkToggleButton* btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleSettingRealize(btn);
}

void on_btn_macroedit_clicked(GtkButton* btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroEdit();
}

void on_input_method_changed(GtkComboBox* cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleInputMethodChanged(cbb);
}

void on_input_method_realize(GtkComboBox* cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleComboBoxRealize(cbb, CONFIG_INPUTMETHOD);
}

void on_output_charset_changed(GtkComboBox* cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleOutputCharsetChanged(cbb);
}

void on_output_charset_realize(GtkComboBox* cbb, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleComboBoxRealize(cbb, CONFIG_OUTPUTCHARSET);
}

gboolean on_macro_dialog_delete(GtkWidget* wid, GdkEvent* ev, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        return controller->handleMacroDialogDelete();
    return TRUE;
}

void macro_dialog_hide(GtkButton* btn, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroDialogHide();
}

void on_btn_macro_del_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroDel();
}

void on_btn_macro_clear_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroClear();
}

void on_btn_macro_import_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroImport();
}

void on_btn_macro_export_clicked(GtkButton *button, gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleMacroExport();
}

void on_cell_key_edited(GtkCellRendererText *celltext,
                    const gchar *string_path,
                    const gchar *newkey,
                    gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleCellKeyEdited(celltext, string_path, newkey);
}

void on_cell_value_edited(GtkCellRendererText *celltext,
                     const gchar *string_path,
                     const gchar *newvalue,
                     gpointer user_data)
{
    if (auto controller = global_setup_controller())
        controller->handleCellValueEdited(celltext, string_path, newvalue);
}
