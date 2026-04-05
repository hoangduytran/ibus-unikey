#ifndef SETUP_CONTROLLER_H
#define SETUP_CONTROLLER_H

#include <gtk/gtk.h>

#include "ui/setup_view.h"
#include "config/settings_store.h"

// Coordinates setup UI events and persistent configuration state
// for the Unikey setup application.
class SetupController
{
public:
    // Construct a controller bound to the view and settings store.
    // @param view UI view wrapper used for event handling and widget access
    // @param store persistent settings storage used by configuration handlers
    SetupController(SetupView &view, SettingsStore &store);

    // Initialize the controller. This is currently a placeholder for
    // controller initialization that may be extended later.
    void init();

    // Handle the main setup window being destroyed.
    // This should trigger application shutdown.
    void handleMainWindowDestroy();

    // Handle key presses in the main window.
    // @param widget widget that received the key event
    // @param event key event details
    // @return TRUE when the event is handled and should not propagate further
    gboolean handleMainWindowKeyPress(GtkWidget *widget, GdkEventKey *event);

    // Handle the close button being clicked.
    void handleBtnClose();

    // Handle a toggle setting being changed by the user.
    // @param btn toggle button associated with the setting
    void handleSettingToggled(GtkToggleButton *btn);

    // Initialize a toggle button state from stored settings.
    // @param btn toggle button associated with the setting
    void handleSettingRealize(GtkToggleButton *btn);

    // Handle changes to the input method selection.
    // @param cbb combo box widget containing input method options
    void handleInputMethodChanged(GtkComboBox *cbb);

    // Handle changes to the output charset selection.
    // @param cbb combo box widget containing output charset options
    void handleOutputCharsetChanged(GtkComboBox *cbb);

    // Initialize input method combo box selection from settings.
    // @param cbb combo box widget containing input method options
    void handleInputMethodRealize(GtkComboBox *cbb);

    // Initialize output charset combo box selection from settings.
    // @param cbb combo box widget containing output charset options
    void handleOutputCharsetRealize(GtkComboBox *cbb);

    // Open the macro editor dialog and allow macro editing.
    void handleMacroEdit();

    // Handle deletion from the macro dialog.
    // @return TRUE when the delete event is handled
    gboolean handleMacroDialogDelete();

    // Hide the macro dialog without saving changes.
    void handleMacroDialogHide();

    // Handle editing the macro key cell in the macro table.
    // @param celltext renderer for the edited cell
    // @param string_path tree path string identifying the edited row
    // @param newkey new key string entered by the user
    void handleCellKeyEdited(GtkCellRendererText *celltext, const gchar *string_path, const gchar *newkey);

    // Handle editing the macro value cell in the macro table.
    // @param celltext renderer for the edited cell
    // @param string_path tree path string identifying the edited row
    // @param newvalue new value string entered by the user
    void handleCellValueEdited(GtkCellRendererText *celltext, const gchar *string_path, const gchar *newvalue);

    // Delete the currently selected macro entry.
    void handleMacroDel();

    // Clear all macro entries and reset the macro list.
    void handleMacroClear();

    // Import macro entries from a file.
    void handleMacroImport();

    // Export macro entries to a file.
    void handleMacroExport();

private:
    // Update the configuration key from the current combo box selection.
    // @param cbb combo box widget
    // @param key configuration key name to update
    void cbbConfigUpdate(GtkComboBox *cbb, const char *key);

    // Set the active combo box item from the configuration value.
    // @param cbb combo box widget
    // @param key configuration key name to read
    void cbbConfigSetActive(GtkComboBox *cbb, const char *key);

    SetupView &m_view;      // reference to the setup UI view
    SettingsStore &m_store; // reference to persistent settings storage
};

// Retrieve the global setup controller instance.
// @return pointer to the currently registered setup controller
SetupController *global_setup_controller();

// Register the global setup controller instance.
// @param c controller instance to set as global
void global_setup_controller_set(SetupController *c);

#endif // SETUP_CONTROLLER_H
