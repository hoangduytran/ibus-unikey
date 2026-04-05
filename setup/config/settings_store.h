#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <string>

// SettingsStore provides a simple wrapper around the Unikey configuration
// backend for storing and retrieving typed settings.
class SettingsStore
{
public:
    SettingsStore() = default;

    // Store a named string value in the configuration.
    // @param key configuration key name
    // @param value string value to persist
    void setString(const std::string &key, const std::string &value);

    // Load a string value from configuration.
    // @param key configuration key name
    // @param out receives the loaded string value if found
    // @return true when the setting exists and is successfully retrieved
    bool getString(const std::string &key, std::string &out) const;

    // Store a named boolean value in the configuration.
    // @param key configuration key name
    // @param value boolean value to persist
    void setBoolean(const std::string &key, bool value);

    // Load a boolean value from configuration.
    // @param key configuration key name
    // @param out receives the loaded boolean value if found
    // @return true when the setting exists and is successfully retrieved
    bool getBoolean(const std::string &key, bool &out) const;
};

#endif // SETTINGS_STORE_H
