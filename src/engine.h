/**
 * @file engine.h
 * @brief Public IBus engine API for the Unikey input method.
 *
 * This header exposes the engine initialization, cleanup, and GType helper
 * functions required by the IBus application and engine registration.
 */
#ifndef __ENGINE_H__
#define __ENGINE_H__

#include <ibus.h>

/**
 * @brief GObject type identifier for the IBus Unikey engine.
 *
 * This macro is used when registering the engine type with the IBus
 * framework.
 */
#define IBUS_TYPE_UNIKEY_ENGINE (ibus_unikey_engine_get_type())

/**
 * @brief Initialize the Unikey engine runtime.
 *
 * @param bus Initialized IBus connection used by the engine.
 */
void ibus_unikey_init(IBusBus *bus);

/**
 * @brief Shutdown and cleanup the Unikey engine runtime.
 */
void ibus_unikey_exit();

/**
 * @brief Returns the GType for the IBus Unikey engine class.
 *
 * @return GType representing IBusUnikeyEngine.
 */
GType ibus_unikey_engine_get_type();

#endif
