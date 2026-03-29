#ifndef SETUP_CONTROLLER_H
#define SETUP_CONTROLLER_H

#include <gtk/gtk.h>

#include "ui/setup_view.h"
#include "config/settings_store.h"

class SetupController {
public:
    SetupController(SetupView& view, SettingsStore& store);

    void init();

    void handleMainWindowDestroy();
    gboolean handleMainWindowKeyPress(GtkWidget *widget, GdkEventKey *event);
    void handleBtnClose();

    void handleSettingToggled(GtkToggleButton* btn);
    void handleSettingRealize(GtkToggleButton* btn);

    void handleInputMethodChanged(GtkComboBox* cbb);
    void handleOutputCharsetChanged(GtkComboBox* cbb);
    void handleInputMethodRealize(GtkComboBox* cbb);
    void handleOutputCharsetRealize(GtkComboBox* cbb);

    void handleMacroEdit();
    gboolean handleMacroDialogDelete();
    void handleMacroDialogHide();

    void handleCellKeyEdited(GtkCellRendererText *celltext, const gchar *string_path, const gchar *newkey);
    void handleCellValueEdited(GtkCellRendererText *celltext, const gchar *string_path, const gchar *newvalue);
    void handleMacroDel();
    void handleMacroClear();
    void handleMacroImport();
    void handleMacroExport();

private:
    void cbbConfigUpdate(GtkComboBox* cbb, const char* key);
    void cbbConfigSetActive(GtkComboBox* cbb, const char* key);

    SetupView& m_view;
    SettingsStore& m_store;
};

SetupController* global_setup_controller();
void global_setup_controller_set(SetupController* c);

#endif // SETUP_CONTROLLER_H
