#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <string>

class SettingsStore {
public:
    SettingsStore() = default;

    void setString(const std::string& key, const std::string& value);
    bool getString(const std::string& key, std::string& out) const;

    void setBoolean(const std::string& key, bool value);
    bool getBoolean(const std::string& key, bool& out) const;
};

#endif // SETTINGS_STORE_H
