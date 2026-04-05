#include <gtk/gtk.h>

extern "C"
{
    // Called when the main setup window is destroyed.
    // @param w window widget being destroyed
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_main_window_destroy(GtkWidget *w, gpointer user_data);

    // Handle key press events on the main window.
    // @param widget source widget for the event
    // @param event key event details
    // @param data optional user data from the signal connection
    // @return TRUE when the event is handled, FALSE to propagate
    G_MODULE_EXPORT gboolean on_main_window_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data);

    // Handle the close button click event.
    // @param btn close button widget
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_btn_close_clicked(GtkButton *btn, gpointer user_data);

    // Handle a setting toggle button change event.
    // @param btn toggle button widget
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_setting_toggled(GtkToggleButton *btn, gpointer user_data);

    // Initialize a setting toggle from stored configuration.
    // @param btn toggle button widget
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_setting_realize(GtkToggleButton *btn, gpointer user_data);

    // Handle the macro edit button click.
    // @param btn macro edit button widget
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_btn_macroedit_clicked(GtkButton *btn, gpointer user_data);

    // Handle changes to the input method combo box.
    // @param cbb combo box widget for input methods
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_input_method_changed(GtkComboBox *cbb, gpointer user_data);

    // Initialize the input method combo box from stored configuration.
    // @param cbb combo box widget for input methods
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_input_method_realize(GtkComboBox *cbb, gpointer user_data);

    // Handle changes to the output charset combo box.
    // @param cbb combo box widget for output charsets
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_output_charset_changed(GtkComboBox *cbb, gpointer user_data);

    // Initialize the output charset combo box from stored configuration.
    // @param cbb combo box widget for output charsets
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_output_charset_realize(GtkComboBox *cbb, gpointer user_data);

    // Handle delete events on the macro dialog window.
    // @param wid macro dialog widget
    // @param ev event details for the delete event
    // @param user_data optional user data from the signal connection
    // @return TRUE when the dialog delete event is handled
    G_MODULE_EXPORT gboolean on_macro_dialog_delete(GtkWidget *wid, GdkEvent *ev, gpointer user_data);

    // Hide the macro dialog in response to the hide action.
    // @param btn button widget that triggered the hide action
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void macro_dialog_hide(GtkButton *btn, gpointer user_data);

    // Handle the macro delete button click.
    // @param button delete button widget
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_btn_macro_del_clicked(GtkButton *button, gpointer user_data);

    // Handle the macro clear button click.
    // @param button clear button widget
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_btn_macro_clear_clicked(GtkButton *button, gpointer user_data);

    // Handle the macro import button click.
    // @param button import button widget
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_btn_macro_import_clicked(GtkButton *button, gpointer user_data);

    // Handle the macro export button click.
    // @param button export button widget
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_btn_macro_export_clicked(GtkButton *button, gpointer user_data);

    // Handle edits to macro key cells in the macro table.
    // @param celltext cell renderer for the edited key cell
    // @param string_path path string identifying the edited row
    // @param newkey new key text entered by the user
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_cell_key_edited(GtkCellRendererText *celltext,
                                            const gchar *string_path,
                                            const gchar *newkey,
                                            gpointer user_data);

    // Handle edits to macro value cells in the macro table.
    // @param celltext cell renderer for the edited value cell
    // @param string_path path string identifying the edited row
    // @param newvalue new value text entered by the user
    // @param user_data optional user data from the signal connection
    G_MODULE_EXPORT void on_cell_value_edited(GtkCellRendererText *celltext,
                                              const gchar *string_path,
                                              const gchar *newvalue,
                                              gpointer user_data);
}
