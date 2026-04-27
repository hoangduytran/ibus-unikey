#include <gtk/gtk.h>

extern "C"
{
    /**
     * @brief Bridge main window destroy signal.
     * @param w Destroyed window widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_main_window_destroy(GtkWidget *w, gpointer user_data);

    /**
     * @brief Bridge key-press event from main window.
     * @param widget Source widget.
     * @param event Key event payload.
     * @param data Optional signal user data.
     * @return TRUE when handled; FALSE to continue propagation.
     */
    G_MODULE_EXPORT gboolean on_main_window_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data);

    /**
     * @brief Bridge close-button click event.
     * @param btn Close button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_btn_close_clicked(GtkButton *btn, gpointer user_data);

    /**
     * @brief Bridge setting toggle change event.
     * @param btn Toggle button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_setting_toggled(GtkToggleButton *btn, gpointer user_data);

    /**
     * @brief Bridge setting-toggle realize event.
     * @param btn Toggle button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_setting_realize(GtkToggleButton *btn, gpointer user_data);

    /**
     * @brief Bridge top-level macro edit button click.
     * @param btn Macro edit button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_btn_macroedit_clicked(GtkButton *btn, gpointer user_data);

    /**
     * @brief Bridge input-method combo change event.
     * @param cbb Input-method combo box.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_input_method_changed(GtkComboBox *cbb, gpointer user_data);

    /**
     * @brief Bridge input-method combo realize event.
     * @param cbb Input-method combo box.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_input_method_realize(GtkComboBox *cbb, gpointer user_data);

    /**
     * @brief Bridge output-charset combo change event.
     * @param cbb Output-charset combo box.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_output_charset_changed(GtkComboBox *cbb, gpointer user_data);

    /**
     * @brief Bridge output-charset combo realize event.
     * @param cbb Output-charset combo box.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_output_charset_realize(GtkComboBox *cbb, gpointer user_data);

    /**
     * @brief Bridge delete-event from macro dialog.
     * @param wid Macro dialog widget.
     * @param ev Delete event payload.
     * @param user_data Optional signal user data.
     * @return TRUE when event was handled.
     */
    G_MODULE_EXPORT gboolean on_macro_dialog_delete(GtkWidget *wid, GdkEvent *ev, gpointer user_data);

    /**
     * @brief Bridge macro dialog hide request.
     * @param btn Button widget source.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void macro_dialog_hide(GtkButton *btn, gpointer user_data);

    /**
     * @brief Bridge macro delete action button click.
     * @param button Delete button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_btn_macro_del_clicked(GtkButton *button, gpointer user_data);

    /**
     * @brief Bridge macro add action button click.
     * @param button Add button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_btn_macro_add_clicked(GtkButton *button, gpointer user_data);

    /**
     * @brief Bridge macro edit action button click.
     * @param button Edit button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_btn_macro_edit_clicked(GtkButton *button, gpointer user_data);

    /**
     * @brief Bridge macro clear action button click.
     * @param button Clear button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_btn_macro_clear_clicked(GtkButton *button, gpointer user_data);

    /**
     * @brief Bridge macro return action button click.
     * @param button Return button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_btn_macro_return_clicked(GtkButton *button, gpointer user_data);

    /**
     * @brief Bridge macro import action button click.
     * @param button Import button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_btn_macro_import_clicked(GtkButton *button, gpointer user_data);

    /**
     * @brief Bridge macro export action button click.
     * @param button Export button widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_btn_macro_export_clicked(GtkButton *button, gpointer user_data);

    /**
     * @brief Bridge Enter activation from macro search entry.
     * @param entry Search entry widget.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_macro_search_activate(GtkEntry *entry, gpointer user_data);

    /**
     * @brief Bridge key events from macro search entry.
     * @param widget Search entry widget.
     * @param event Key event payload.
     * @param user_data Optional signal user data.
     * @return TRUE when handled.
     */
    G_MODULE_EXPORT gboolean on_macro_search_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

    /**
     * @brief Bridge row activation from macro table.
     * @param tree_view Macro table tree view.
     * @param path Activated row path.
     * @param column Activated column.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_tree_macro_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);

    /**
     * @brief Bridge legacy key-cell edited signal (inline edit disabled).
     * @param celltext Edited cell renderer.
     * @param string_path GTK row path string.
     * @param newkey New key text.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_cell_key_edited(GtkCellRendererText *celltext,
                                            const gchar *string_path,
                                            const gchar *newkey,
                                            gpointer user_data);

    /**
     * @brief Bridge legacy value-cell edited signal (inline edit disabled).
     * @param celltext Edited cell renderer.
     * @param string_path GTK row path string.
     * @param newvalue New replacement text.
     * @param user_data Optional signal user data.
     */
    G_MODULE_EXPORT void on_cell_value_edited(GtkCellRendererText *celltext,
                                              const gchar *string_path,
                                              const gchar *newvalue,
                                              gpointer user_data);
}
