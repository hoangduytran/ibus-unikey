#include "settings_store.h"

#include "unikey_config.h"

// Persist the string value for the given configuration key.
// This forwards the request to the underlying Unikey config API.
void SettingsStore::setString(const std::string &key, const std::string &value)
{
    ibus_unikey_config_set_string(key.c_str(), value.c_str());
}

// Retrieve a string value from the configuration store.
// Returns true when the value exists and has been loaded into out.
bool SettingsStore::getString(const std::string &key, std::string &out) const
{
    gchar *value = NULL;
    if (ibus_unikey_config_get_string(key.c_str(), &value))
    {
        out = value;
        g_free(value);
        return true;
    }
    return false;
}

// Persist the boolean value for the given configuration key.
// This forwards the request to the underlying Unikey config API.
void SettingsStore::setBoolean(const std::string &key, bool value)
{
    ibus_unikey_config_set_boolean(key.c_str(), value);
}

// Retrieve a boolean value from the configuration store.
// Returns true when the value exists and has been loaded into out.
bool SettingsStore::getBoolean(const std::string &key, bool &out) const
{
    gboolean value;
    if (ibus_unikey_config_get_boolean(key.c_str(), &value))
    {
        out = value;
        return true;
    }
    return false;
}
