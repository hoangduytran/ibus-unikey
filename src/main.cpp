/**
 * @file main.cpp
 * @brief Entrypoint launcher for the Unikey IBus engine application.
 *
 * Delegates startup to the engine application module so that command-line
 * parsing and IBus initialization remain separated from the standard C entry
 * point.
 */
#include "engine/engine_app.h"

/**
 * @brief Application entrypoint.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of UTF-8 argument strings.
 * @return Exit status returned by the engine application.
 */
int main(int argc, char **argv)
{
    return ibus_unikey_engine_app_main(argc, argv);
}
