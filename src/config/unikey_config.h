#if !defined(__IBUS_UNIKEY_CONFIG_H__)
#define __IBUS_UNIKEY_CONFIG_H__

#include <map>
#include <string>
#include <gio/gio.h>

#include "ukengine.h"

/**
 * @file unikey_config.h
 * @brief Defines Unikey configuration keys and the public GSettings helper API.
 *
 * This module centralizes the GSettings schema identifiers used by the engine,
 * provides lookup tables for input/output options, and exposes configuration
 * getters and setters for string and boolean values.
 */

/**
 * @brief Relative path of the user's Unikey macro file under $HOME.
 */
#define UNIKEY_MACRO_FILE ".ibus/unikey/macro"

/**
 * @brief Schema identifier for the Unikey IBus engine settings.
 */
#define UNIKEY_GSCHEMA_ID "org.freedesktop.ibus.engine.unikey"

#define CONFIG_INPUTMETHOD "input-method"
#define CONFIG_OUTPUTCHARSET "output-charset"
#define CONFIG_SPELLCHECK "spell-check"
#define CONFIG_AUTORESTORENONVN "auto-restore-non-vn"
#define CONFIG_MODERNSTYLE "modern-style"
#define CONFIG_FREEMARKING "free-marking"
#define CONFIG_MACROENABLED "macro-enabled"
#define CONFIG_STANDALONEW "standalone-w-as-uw"

/** Last directory used by macro import/export file choosers (UTF-8 path or empty). */
#define CONFIG_MACRO_LASTWORKINGDIR "macro-last-working-dir"

/**
 * @brief Maps persisted engine input method strings to runtime enum values.
 *
 * The map key is the GSettings string identifier, the value is a pair where
 * the first element is the corresponding internal UkInputMethod enum and the
 * second element is the human-readable display name.
 */
const std::map<const std::string, std::pair<UkInputMethod, const gchar *>> input_method_map{
    {"telex", {UkTelex, "Extend Telex"}},
    {"vni", {UkVni, "VNI"}},
    {"stelex", {UkSimpleTelex, "STelex"}},
    {"stelex2", {UkSimpleTelex2, "STelex 2"}},
};

/**
 * @brief Maps persisted output charset names to internal conversion constants.
 *
 * The pair contains the converter identifier and a human-readable label used
 * for UI presentation.
 */
const std::map<const std::string, std::pair<unsigned int, const gchar *>> output_charset_map{
    {"unicode", {CONV_CHARSET_XUTF8, "Unicode"}},
    {"tcvn3", {CONV_CHARSET_TCVN3, "TCVN3"}},
    {"vni-win", {CONV_CHARSET_VNIWIN, "VNI Win"}},
    {"viqr", {CONV_CHARSET_VIQR, "VIQR"}},
    {"bk-hcm2", {CONV_CHARSET_BKHCM2, "BK HCM 2"}},
    {"cstr", {CONV_CHARSET_UNI_CSTRING, "CString"}},
    {"ncr-dec", {CONV_CHARSET_UNIREF, "NCR Decimal"}},
    {"ncr-hex", {CONV_CHARSET_UNIREF_HEX, "NCR Hex"}},
};

/**
 * @brief Builds the absolute path to the user's Unikey macro file.
 *
 * @note The returned string is newly allocated by g_build_filename() and must
 *       be freed by the caller with g_free().
 *
 * @return gchar* Path to the macro file inside the user's home directory.
 */
#define get_macro_file() (g_build_filename(g_getenv("HOME"), UNIKEY_MACRO_FILE, NULL))

/**
 * @brief Initializes the GSettings backend for Unikey configuration access.
 *
 * Must be called before any other configuration helper is used.
 */
void ibus_unikey_config_init();

/**
 * @brief Registers a callback for GSettings change notifications.
 *
 * The callback is invoked whenever a setting in the Unikey schema is changed.
 *
 * @param cb Callback function receiving the changed key name and user data.
 * @param user_data Opaque pointer passed through to the callback.
 */
void ibus_unikey_config_on_changed(void (*cb)(gchar *, gpointer), gpointer user_data);

/**
 * @brief Retrieves a string value from the Unikey settings.
 *
 * @param name Name of the configuration key.
 * @param result Output pointer that receives a newly allocated string value.
 *               Caller must free the returned string with g_free().
 * @return TRUE when the value exists and was retrieved successfully.
 *         FALSE when the key is missing or cannot be read.
 */
gboolean ibus_unikey_config_get_string(const gchar *name, gchar **result);

/**
 * @brief Stores a string value in the Unikey settings.
 *
 * @param name Name of the configuration key.
 * @param value Null-terminated string to store.
 */
void ibus_unikey_config_set_string(const gchar *name, const gchar *value);

/**
 * @brief Retrieves a boolean value from the Unikey settings.
 *
 * @param name Name of the configuration key.
 * @param result Output pointer that receives the boolean value.
 * @return TRUE when the value exists and was retrieved successfully.
 *         FALSE when the key is missing or cannot be read.
 */
gboolean ibus_unikey_config_get_boolean(const gchar *name, gboolean *result);

/**
 * @brief Stores a boolean value in the Unikey settings.
 *
 * @param name Name of the configuration key.
 * @param value Boolean value to store.
 */
void ibus_unikey_config_set_boolean(const gchar *name, gboolean value);

#endif
