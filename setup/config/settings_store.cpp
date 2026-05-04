/**
 * @file settings_store.cpp
 * @brief Thin C++ wrapper over `ibus_unikey_config_*` for string and boolean GSettings keys.
 */

#include "settings_store.h"

#include "unikey_config.h"

void SettingsStore::setString(const std::string &key, const std::string &value) {
  ibus_unikey_config_set_string(key.c_str(), value.c_str());
}

bool SettingsStore::getString(const std::string &key, std::string &out) const {
  gchar *value = NULL;
  if (ibus_unikey_config_get_string(key.c_str(), &value)) {
    out = value;
    g_free(value);
    return true;
  }
  return false;
}

void SettingsStore::setBoolean(const std::string &key, bool value) {
  ibus_unikey_config_set_boolean(key.c_str(), value);
}

bool SettingsStore::getBoolean(const std::string &key, bool &out) const {
  gboolean value;
  if (ibus_unikey_config_get_boolean(key.c_str(), &value)) {
    out = value;
    return true;
  }
  return false;
}
