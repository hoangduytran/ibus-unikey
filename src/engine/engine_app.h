/**
 * @file engine_app.h
 * @brief Declares the IBus engine application entrypoint for Unikey.
 *
 * This header exposes the public startup entrypoint used by the Unikey IBus
 * engine launcher. The implementation handles locale setup, option parsing,
 * and dispatches either XML metadata output or component startup.
 */
#ifndef ENGINE_APP_H
#define ENGINE_APP_H

/**
 * @brief Main application entrypoint for the Unikey IBus engine.
 *
 * @param argc Number of command-line arguments.
 * @param argv Null-terminated array of UTF-8 argument strings.
 * @return Exit status code. Returns 0 on success; option parsing failures
 *         terminate the process with a non-zero status.
 */
int ibus_unikey_engine_app_main(int argc, char** argv);

#endif // ENGINE_APP_H
