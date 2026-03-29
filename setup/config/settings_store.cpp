#include "settings_store.h"
#include "unikey_config.h"

void SettingsStore::setInputMethod(const std::string &value)
{
    ibus_unikey_config_set_string(CONFIG_INPUTMETHOD, value.c_str());
}

std::string SettingsStore::getInputMethod() const
{
    gchar *value = nullptr;
    if (ibus_unikey_config_get_string(CONFIG_INPUTMETHOD, &value))
    {
        std::string out(value);
        g_free(value);
        return out;
    }
    return std::string();
}

void SettingsStore::setOutputCharset(const std::string &value)
{
    ibus_unikey_config_set_string(CONFIG_OUTPUTCHARSET, value.c_str());
}

std::string SettingsStore::getOutputCharset() const
{
    gchar *value = nullptr;
    if (ibus_unikey_config_get_string(CONFIG_OUTPUTCHARSET, &value))
    {
        std::string out(value);
        g_free(value);
        return out;
    }
    return std::string();
}

void SettingsStore::setBoolean(const std::string &key, bool value)
{
    ibus_unikey_config_set_boolean(key.c_str(), value);
}

bool SettingsStore::getBoolean(const std::string &key, bool &out) const
{
    gboolean b;
    if (ibus_unikey_config_get_boolean(key.c_str(), &b))
    {
        out = (b != FALSE);
        return true;
    }
    return false;
}

void SettingsStore::setString(const std::string &key, const std::string &value)
{
    ibus_unikey_config_set_string(key.c_str(), value.c_str());
}

bool SettingsStore::getString(const std::string &key, std::string &out) const
{
    gchar* s = nullptr;
    if (ibus_unikey_config_get_string(key.c_str(), &s))
    {
        out = s;
        g_free(s);
        return true;
    }
    return false;
}
