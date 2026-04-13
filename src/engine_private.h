#ifndef __ENGINE_PRIVATE_H__
#define __ENGINE_PRIVATE_H__

#include <string>
#include <ibus.h>
#include "unikey.h"
#include "vnconv.h"

/**
 * @file engine_private.h
 * @brief Private Unikey engine implementation declarations.
 *
 * Contains the internal engine instance structure and static helper
 * prototypes used by src/engine.cpp.
 */

typedef struct _IBusUnikeyEngine IBusUnikeyEngine;
typedef struct _IBusUnikeyEngineClass IBusUnikeyEngineClass;

struct _IBusUnikeyEngine
{
    IBusEngine parent;

    /** Property list published to IBus for engine options. */
    IBusPropList *prop_list;

    /** Currently selected input method. */
    UkInputMethod im;

    /** Currently selected output charset conversion mode. */
    unsigned int oc;

    /** Current engine options loaded from configuration. */
    UnikeyOptions ukopt;

    /** Whether the engine should treat "w" as the Vietnamese character ư.
     *  This is configured by the standalone-w-as-uw option. */
    gboolean process_w_at_begin;

    /** Whether the last key event was a shifted key press. */
    gboolean last_key_with_shift;

    /** Preedit buffer for composing text before commit. */
    std::string *preeditstr;
};

struct _IBusUnikeyEngineClass
{
    IBusEngineClass parent;
};

/**
 * @brief Initialize the Unikey engine GObject class.
 *
 * @param kclass Class structure to initialize.
 */
static void ibus_unikey_engine_class_init(IBusUnikeyEngineClass *kclass);

/**
 * @brief Initialize a new Unikey engine instance.
 *
 * @param unikey Newly allocated engine instance.
 */
static void ibus_unikey_engine_init(IBusUnikeyEngine *unikey);

/**
 * @brief Custom GObject constructor for the Unikey engine.
 *
 * @param type GType for the object being created.
 * @param n_construct_params Number of construction parameters.
 * @param construct_params Array of construction parameters.
 * @return New GObject instance.
 */
static GObject *ibus_unikey_engine_constructor(GType type,
                                               guint n_construct_params,
                                               GObjectConstructParam *construct_params);

/**
 * @brief Destroy a Unikey engine instance.
 *
 * @param unikey Engine instance to destroy.
 */
static void ibus_unikey_engine_destroy(IBusUnikeyEngine *unikey);

/**
 * @brief Process a raw key event for the Unikey engine.
 *
 * @param engine IBus engine instance.
 * @param keyval Unicode key value received from IBus.
 * @param keycode Platform-specific scancode.
 * @param modifiers Modifier mask from IBus.
 * @return TRUE if the event was consumed, FALSE otherwise.
 */
static gboolean ibus_unikey_engine_process_key_event(IBusEngine *engine,
                                                     guint keyval,
                                                     guint keycode,
                                                     guint modifiers);

/**
 * @brief Handle engine focus entering.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_focus_in(IBusEngine *engine);

/**
 * @brief Handle engine focus leaving.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_focus_out(IBusEngine *engine);

/**
 * @brief Reset engine state for the current input context.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_reset(IBusEngine *engine);

/**
 * @brief Enable the engine when input becomes active.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_enable(IBusEngine *engine);

/**
 * @brief Disable the engine when input becomes inactive.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_disable(IBusEngine *engine);

/**
 * @brief Load configuration values into the engine instance.
 *
 * @param unikey Engine instance to configure.
 */
static void ibus_unikey_engine_load_config(IBusUnikeyEngine *unikey);

/**
 * @brief Callback for configuration changes.
 *
 * @param name Name of the changed setting.
 * @param user_data Opaque user data from the signal connection.
 */
static void ibus_unikey_config_value_changed(gchar *name, gpointer user_data);

/**
 * @brief Handle property activation events from the IBus property list.
 *
 * @param engine IBus engine instance.
 * @param prop_name Name of the activated property.
 * @param prop_state New state of the property.
 */
static void ibus_unikey_engine_property_activate(IBusEngine *engine,
                                                 const gchar *prop_name,
                                                 guint prop_state);

/**
 * @brief Process key events before preedit text handling.
 *
 * @param engine IBus engine instance.
 * @param keyval Unicode key value.
 * @param keycode Platform-specific scancode.
 * @param modifiers Modifier mask from IBus.
 * @return TRUE if the event was handled, FALSE otherwise.
 */
static gboolean ibus_unikey_engine_process_key_event_preedit(IBusEngine *engine,
                                                             guint keyval,
                                                             guint keycode,
                                                             guint modifiers);

/**
 * @brief Create and publish the engine property list.
 *
 * @param unikey Engine instance that owns the property list.
 */
static void ibus_unikey_engine_create_property_list(IBusUnikeyEngine *unikey);

/**
 * @brief Update the preedit string displayed by the engine.
 *
 * @param engine IBus engine instance.
 * @param string UTF-8 string to display.
 * @param visible Whether the preedit text should be visible.
 */
static void ibus_unikey_engine_update_preedit_string(IBusEngine *engine, const gchar *string, gboolean visible);

/**
 * @brief Erase characters from the engine preedit buffer.
 *
 * @param engine IBus engine instance.
 * @param num_chars Number of characters to remove.
 */
static void ibus_unikey_engine_erase_chars(IBusEngine *engine, int num_chars);

/**
 * @brief Convert a legacy Latin-encoded buffer to UTF-8.
 *
 * @param dst Destination buffer for UTF-8 data.
 * @param src Source Latin-encoded buffer.
 * @param inSize Number of bytes in the source buffer.
 * @param pOutSize Input/output size of the destination buffer.
 * @return Non-zero when conversion succeeded within the available buffer.
 */
static int latinToUtf(unsigned char *dst, unsigned char *src, int inSize, int *pOutSize);

#endif
