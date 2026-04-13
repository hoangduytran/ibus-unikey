#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "unikey_config.h"

/**
 * @brief Shared GSettings instance used for Unikey configuration access.
 *
 * Initialized by ibus_unikey_config_init() and assumed valid by all other
 * functions in this module.
 */
static GSettings *settings;

/**
 * @brief Initializes the GSettings backend for Unikey configuration.
 *
 * This function must be called before any other configuration helper is used.
 */
void ibus_unikey_config_init()
{
    settings = g_settings_new(UNIKEY_GSCHEMA_ID);
}

/**
 * @brief Storage for callback data passed through the GSettings signal system.
 */
struct changed_data
{
    /** Callback to invoke when a config key changes. */
    void (*cb)(gchar *, gpointer);
    /** Opaque user data forwarded to the callback. */
    gpointer user_data;
};

/**
 * @brief Wraps the GSettings "changed" signal and forwards event data.
 *
 * @param settings Unused GSettings instance pointer from the signal.
 * @param name Name of the changed key.
 * @param user_data Opaque pointer to changed_data.
 */
static void settings_changed_wrap(GSettings *settings, gchar *name, gpointer user_data)
{
    auto data = (changed_data *)user_data;
    data->cb(name, data->user_data);
}

/**
 * @brief Registers a callback for Unikey configuration changes.
 *
 * @param cb Callback invoked when any schema key changes.
 * @param user_data Opaque pointer passed through to the callback.
 *
 * @note The callback data is currently allocated and never freed.
 *       A cleanup mechanism should be added before long-running use.
 */
void ibus_unikey_config_on_changed(void (*cb)(gchar *, gpointer), gpointer user_data)
{
    // REVIEW: Consider adding cleanup for the allocated changed_data instance.
    auto data = new changed_data{cb, user_data};
    g_signal_connect(settings, "changed", G_CALLBACK(settings_changed_wrap), data);
}

/**
 * @brief Reads a string value from the Unikey settings.
 *
 * @param name Key name to query.
 * @param result Output pointer to receive a duplicated string.
 *               Caller must free the resulting string with g_free().
 * @return TRUE if the value was found and parsed successfully.
 *         FALSE if the key is missing or the value cannot be read.
 */
gboolean ibus_unikey_config_get_string(const gchar *name, gchar **result)
{
    GVariant *value = NULL;
    value = g_settings_get_value(settings, name);
    if (value)
    {
        *result = g_variant_dup_string(value, NULL);
        g_variant_unref(value);
        return true;
    }
    return false;
}

/**
 * @brief Writes a string value to the Unikey settings.
 *
 * @param name Configuration key name.
 * @param value String value to store.
 */
void ibus_unikey_config_set_string(const gchar *name, const gchar *value)
{
    g_settings_set_value(settings, name, g_variant_new_string(value));
}

/**
 * @brief Reads a boolean value from the Unikey settings.
 *
 * @param name Key name to query.
 * @param result Output pointer that receives the boolean value.
 * @return TRUE if the value was found and parsed successfully.
 *         FALSE if the key is missing or the value cannot be read.
 */
gboolean ibus_unikey_config_get_boolean(const gchar *name, gboolean *result)
{
    GVariant *value = NULL;
    value = g_settings_get_value(settings, name);
    if (value)
    {
        *result = g_variant_get_boolean(value);
        g_variant_unref(value);
        return true;
    }
    return false;
}

/**
 * @brief Writes a boolean value to the Unikey settings.
 *
 * @param name Configuration key name.
 * @param value Boolean value to store.
 */
void ibus_unikey_config_set_boolean(const gchar *name, gboolean value)
{
    g_settings_set_value(settings, name, g_variant_new_boolean(value));
}
