#include <glib.h>

#include "macro_export_filename.h"

static void test_appends_txt_extension_when_missing()
{
    gchar* completed = macro_export_filename_complete("/tmp/macros", ".txt");
    g_assert_cmpstr(completed, ==, "/tmp/macros.txt");
    g_free(completed);
}

static void test_appends_yaml_extension_when_missing()
{
    gchar* completed = macro_export_filename_complete("macro-backup", ".yaml");
    g_assert_cmpstr(completed, ==, "macro-backup.yaml");
    g_free(completed);
}

static void test_preserves_matching_extension()
{
    gchar* completed = macro_export_filename_complete("macro.plist", ".plist");
    g_assert_cmpstr(completed, ==, "macro.plist");
    g_free(completed);
}

static void test_appends_selected_extension_on_conflict()
{
    gchar* completed = macro_export_filename_complete("macro.plist", ".yaml");
    g_assert_cmpstr(completed, ==, "macro.plist.yaml");
    g_free(completed);
}

static void test_detects_extension_conflict()
{
    gboolean has_conflict = macro_export_filename_has_extension_conflict("macro.plist", ".yaml");
    g_assert_true(has_conflict);
}

static void test_ignores_matching_extension_conflict_check()
{
    gboolean has_conflict = macro_export_filename_has_extension_conflict("macro.yaml", ".yaml");
    g_assert_false(has_conflict);
}

static void test_hidden_dotfile_is_treated_as_extensionless()
{
    gchar* completed = macro_export_filename_complete(".macro-export", ".yaml");
    g_assert_cmpstr(completed, ==, ".macro-export.yaml");
    g_free(completed);
}

static void test_missing_preferred_extension_keeps_name()
{
    gchar* completed = macro_export_filename_complete("macro-export", NULL);
    g_assert_cmpstr(completed, ==, "macro-export");
    g_free(completed);
}

static void test_finalize_empty_basename_uses_default_stem()
{
    gchar* finalized = macro_export_filename_finalize_for_save("/tmp/", ".json", "macro");
    g_assert_cmpstr(finalized, ==, "/tmp/macro.json");
    g_free(finalized);
}

static void test_finalize_empty_default_stem_still_uses_macro()
{
    gchar* finalized = macro_export_filename_finalize_for_save("/tmp/", ".yaml", "");
    g_assert_cmpstr(finalized, ==, "/tmp/macro.yaml");
    g_free(finalized);

    finalized = macro_export_filename_finalize_for_save("/tmp/", ".yaml", NULL);
    g_assert_cmpstr(finalized, ==, "/tmp/macro.yaml");
    g_free(finalized);
}

static void test_finalize_dot_basename_substitutes_stem()
{
    gchar* finalized = macro_export_filename_finalize_for_save("/some/dir/.", ".plist", "macro");
    g_assert_cmpstr(finalized, ==, "/some/dir/macro.plist");
    g_free(finalized);
}

static void test_finalize_basename_only_appends_extension()
{
    gchar* finalized = macro_export_filename_finalize_for_save("macro", ".json", "macro");
    g_assert_cmpstr(finalized, ==, "macro.json");
    g_free(finalized);
}

int main(int argc, char** argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/ui/macros/export-filename/appends-txt", test_appends_txt_extension_when_missing);
    g_test_add_func("/ui/macros/export-filename/appends-yaml", test_appends_yaml_extension_when_missing);
    g_test_add_func("/ui/macros/export-filename/preserves-matching-extension", test_preserves_matching_extension);
    g_test_add_func("/ui/macros/export-filename/appends-on-conflict", test_appends_selected_extension_on_conflict);
    g_test_add_func("/ui/macros/export-filename/detects-conflict", test_detects_extension_conflict);
    g_test_add_func("/ui/macros/export-filename/ignores-matching-conflict", test_ignores_matching_extension_conflict_check);
    g_test_add_func("/ui/macros/export-filename/dotfile-is-extensionless", test_hidden_dotfile_is_treated_as_extensionless);
    g_test_add_func("/ui/macros/export-filename/missing-extension-keeps-name", test_missing_preferred_extension_keeps_name);
    g_test_add_func("/ui/macros/export-filename/finalize-empty-basename", test_finalize_empty_basename_uses_default_stem);
    g_test_add_func("/ui/macros/export-filename/finalize-null-default-stem", test_finalize_empty_default_stem_still_uses_macro);
    g_test_add_func("/ui/macros/export-filename/finalize-dot-basename", test_finalize_dot_basename_substitutes_stem);
    g_test_add_func("/ui/macros/export-filename/finalize-basename-only", test_finalize_basename_only_appends_extension);

    return g_test_run();
}