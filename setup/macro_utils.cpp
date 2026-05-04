/**
 * @file macro_utils.cpp
 * @brief GtkListStore helpers: mirror `CMacroTable` in the macro dialog with last-wins duplicate policy.
 */

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <ukengine/mapping/keycons.h>
#include <ukengine/mapping/vnconv.h>
#include "macro_utils.h"

/**
 * @brief Case-fold a UTF-8 key for last-wins maps (glib / Unicode semantics).
 * @param trigger_utf8 Null-terminated display key.
 */
static std::string macroKeyFoldForMap(const char *trigger_utf8) {
  gchar *casefolded_utf8 = g_utf8_casefold(trigger_utf8, -1);
  std::string folded_std_string(casefolded_utf8 ? casefolded_utf8 : "");
  g_free(casefolded_utf8);
  return folded_std_string;
}

/** @brief Convert NUL-terminated VNSTANDARD table text to UTF-8 using a growable buffer. */
static bool vnStdToUtf8Display(const StdVnChar *std_source, std::string &display_utf8_out) {
  std::vector<char> conversion_buffer(512);
  for (int resize_attempt = 0; resize_attempt < 24; resize_attempt++) {
    int input_length = -1;
    int max_output_bytes = (int)conversion_buffer.size();
    const int convert_result =
        VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_XUTF8, (UKBYTE *)std_source,
                  (UKBYTE *)conversion_buffer.data(), &input_length, &max_output_bytes);
    if (convert_result == 0) {
      display_utf8_out.assign(conversion_buffer.data(), (size_t)max_output_bytes);
      while (!display_utf8_out.empty() && display_utf8_out.back() == '\0')
        display_utf8_out.pop_back();
      return true;
    }
    if (conversion_buffer.size() > (size_t)64 * 1024 * 1024)
      return false;
    conversion_buffer.resize(conversion_buffer.size() * 2);
  }
  return false;
}

void list_store_append(GtkListStore *list, CMacroTable *macro) {
  const int macro_row_count = macro->getCount();
  if (macro_row_count <= 0)
    return;

  std::unordered_map<std::string, int> last_row_index_by_folded_key;
  for (int row_index = 0; row_index < macro_row_count; row_index++) {
    std::string trigger_display_utf8;
    if (!vnStdToUtf8Display(macro->getKey(row_index), trigger_display_utf8))
      continue;
    const std::string folded_trigger_key = macroKeyFoldForMap(trigger_display_utf8.c_str());
    last_row_index_by_folded_key[folded_trigger_key] = row_index;
  }

  for (int row_index = 0; row_index < macro_row_count; row_index++) {
    std::string trigger_display_utf8;
    if (!vnStdToUtf8Display(macro->getKey(row_index), trigger_display_utf8))
      continue;
    const std::string folded_trigger_key = macroKeyFoldForMap(trigger_display_utf8.c_str());
    if (last_row_index_by_folded_key[folded_trigger_key] != row_index)
      continue;

    std::string expansion_display_utf8;
    if (!vnStdToUtf8Display(macro->getText(row_index), expansion_display_utf8))
      continue;

    GtkTreeIter new_row_iter;
    gtk_list_store_append(list, &new_row_iter);
    gtk_list_store_set(list, &new_row_iter, COL_KEY, trigger_display_utf8.c_str(), COL_VALUE,
                       expansion_display_utf8.c_str(), -1);
  }
}

void unikey_macro_to_store(CMacroTable *macro, GtkListStore *store) {
  gtk_list_store_clear(store);

  list_store_append(store, macro);
  list_store_add_null_item(store);
}

UnikeyMacroTableFillResult unikey_gtk_model_fill_macro_table(GtkTreeModel *model, CMacroTable *macro,
                                                             gboolean reset_table_first) {
  UnikeyMacroTableFillResult fill_result;
  fill_result.total = 0;
  fill_result.added = 0;
  fill_result.failed = 0;
  if (reset_table_first)
    macro->resetContent();

  struct ModelRowUtf8 {
    std::string trigger_text;
    std::string expansion_text;
  };
  std::vector<ModelRowUtf8> model_rows;
  {
    GtkTreeIter row_iter;
    gboolean has_next_row = gtk_tree_model_get_iter_first(model, &row_iter);
    while (has_next_row == TRUE) {
      gchar *trigger_column, *expansion_column;
      gtk_tree_model_get(model, &row_iter, COL_KEY, &trigger_column, COL_VALUE, &expansion_column, -1);
      if (strcasecmp(trigger_column, STR_NULL_ITEM) != 0)
        model_rows.push_back(
            ModelRowUtf8{std::string(trigger_column), std::string(expansion_column)});
      g_free(trigger_column);
      g_free(expansion_column);
      has_next_row = gtk_tree_model_iter_next(model, &row_iter);
    }
  }

  std::unordered_map<std::string, int> last_visible_row_index_by_folded_key;
  for (size_t row_index = 0; row_index < model_rows.size(); row_index++) {
    fill_result.total++;
    last_visible_row_index_by_folded_key[macroKeyFoldForMap(model_rows[row_index].trigger_text.c_str())] =
        (int)row_index;
  }

  for (size_t row_index = 0; row_index < model_rows.size(); row_index++) {
    const std::string folded_trigger_key =
        macroKeyFoldForMap(model_rows[row_index].trigger_text.c_str());
    if (last_visible_row_index_by_folded_key[folded_trigger_key] != (int)row_index)
      continue;
    if (macro->addItem(model_rows[row_index].trigger_text.c_str(), model_rows[row_index].expansion_text.c_str(),
                       CONV_CHARSET_XUTF8) < 0)
      fill_result.failed++;
    else
      fill_result.added++;
  }
  return fill_result;
}

UnikeyMacroTableFillResult unikey_store_to_macro(GtkListStore *store, CMacroTable *macro) {
  return unikey_gtk_model_fill_macro_table(GTK_TREE_MODEL(store), macro, TRUE);
}

void list_store_add_null_item(GtkListStore *list) {
  GtkTreeIter tail_row_iter;

  const auto child_count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list), NULL);
  if (child_count > 0) {
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list), &tail_row_iter, NULL, child_count - 1);
    gchar *tail_trigger;
    gtk_tree_model_get(GTK_TREE_MODEL(list), &tail_row_iter, COL_KEY, &tail_trigger, -1);
    if (strcmp(tail_trigger, STR_NULL_ITEM) == 0) {
      g_free(tail_trigger);
      return;
    }
    g_free(tail_trigger);
  }

  // If the last item is a real macro or the list is empty, add a placeholder row for new entry UX.
  gtk_list_store_append(list, &tail_row_iter);
  gtk_list_store_set(list, &tail_row_iter,
                     COL_KEY, STR_NULL_ITEM,
                     COL_VALUE, STR_NULL_ITEM,
                     -1);
}
