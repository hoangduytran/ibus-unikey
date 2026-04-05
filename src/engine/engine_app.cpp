/**
 * @file engine_app.cpp
 * @brief Implements the Unikey IBus engine application startup and component
 *        registration.
 *
 * This module handles command-line options, locale initialization, IBus
 * component construction, and the main engine event loop.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libintl.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>

#include <ibus.h>

#include "engine.h"
#include "engine_app.h"
#include "unikey.h"

#define _(string) gettext(string)

/**
 * @brief Shared IBus connection instance used by the engine.
 *
 * Initialized in start_component() and reused for the lifetime of the app.
 */
static IBusBus* bus = NULL;

/**
 * @brief Factory used to register and manage engine descriptors.
 */
static IBusFactory* factory = NULL;

/**
 * @brief When TRUE, print engine XML metadata instead of starting the engine.
 */
static gboolean xml = FALSE;

/**
 * @brief When TRUE, the application is being run by IBus directly.
 */
static gboolean ibus = FALSE;

/**
 * @brief When TRUE, verbose output is enabled for debugging.
 */
static gboolean verbose = FALSE;

/**
 * @brief Command-line options recognized by the engine application.
 *
 * These options are parsed by GLib's GOptionContext and control whether the
 * application prints engine metadata, runs under IBus, or enables verbosity.
 */
static const GOptionEntry entries[] =
{
    { "xml",     'x', 0, G_OPTION_ARG_NONE, &xml,     "generate xml for engines", NULL },
    { "ibus",    'i', 0, G_OPTION_ARG_NONE, &ibus,    "component is executed by ibus", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "verbose", NULL },
    { NULL },
};

static IBusComponent* ibus_unikey_get_component();

/**
 * @brief Callback invoked when the IBus bus disconnects.
 *
 * @param bus Unused IBusBus instance pointer supplied by the signal.
 * @param user_data Opaque user data supplied by g_signal_connect().
 */
static void ibus_disconnected_cb(IBusBus* bus, gpointer user_data)
{
    ibus_quit();
}

/**
 * @brief Starts the Unikey IBus engine component.
 *
 * This function initializes the IBus context, registers the Unikey engine
 * component or request name depending on the invocation mode, then enters the
 * IBus main loop.
 */
static void start_component(void)
{
    GList* engines;
    GList* p;
    IBusComponent* component;

    ibus_init();

    bus = ibus_bus_new();
    g_signal_connect(bus, "disconnected", G_CALLBACK(ibus_disconnected_cb), NULL);

    component = ibus_unikey_get_component();

    factory = ibus_factory_new(ibus_bus_get_connection(bus));

    engines = ibus_component_get_engines(component);
    for (p = engines; p != NULL; p = p->next)
    {
        IBusEngineDesc* engine = (IBusEngineDesc*)p->data;
        ibus_factory_add_engine(factory, ibus_engine_desc_get_name(engine), IBUS_TYPE_UNIKEY_ENGINE);
    }

    if (ibus)
        ibus_bus_request_name(bus, "org.freedesktop.IBus.Unikey", 0);
    else
        ibus_bus_register_component(bus, component);

    g_object_unref(component);

    ibus_unikey_init(bus);
    ibus_main();
    ibus_unikey_exit();
}

/**
 * @brief Prints the Unikey engine XML metadata.
 *
 * This is used by IBus when querying available engine components.
 */
static void print_engines_xml(void)
{
    IBusComponent* component;
    GString* output;

    ibus_init();

    component = ibus_unikey_get_component();
    output = g_string_new("");

    ibus_component_output_engines(component, output, 0);

    fprintf(stdout, "%s", output->str);

    g_string_free(output, TRUE);
}

/**
 * @brief Main entrypoint for the Unikey engine application.
 *
 * Parses command-line options, initializes locale support, and either prints
 * engine metadata or starts the runtime component.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of argument strings.
 * @return Always returns 0 on normal termination.
 */
int ibus_unikey_engine_app_main(int argc, char** argv)
{
    GError* error = NULL;
    GOptionContext* context;

    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    context = g_option_context_new("- ibus unikey engine component");

    g_option_context_add_main_entries(context, entries, "ibus-unikey");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("Option parsing failed: %s\n", error->message);
        exit(-1);
    }

    if (xml)
    {
        print_engines_xml();
        return 0;
    }

    start_component();

    return 0;
}

/**
 * @brief Long description text shown in the IBus engine descriptor.
 *
 * This string is localized through gettext and describes usage instructions
 * for the Unikey input method engine.
 */
#define IU_DESC _("Vietnamese Input Method Engine for IBus using Unikey Engine\n\
Usage:\n\
  - Choose input method, output charset, options in language bar.\n\
  - There are 4 input methods: Telex, Vni, STelex (simple telex) \
and STelex2 (which same as STelex, the difference is it use w as ư).\n\
  - And 7 output charsets: Unicode (UTF-8), TCVN3, VNI Win, VIQR, CString, NCR Decimal and NCR Hex.\n\
  - Use <Shift>+<Space> or <Shift>+<Shift> to restore keystrokes.\n\
  - Use <Control> to commit a word.\n")

/**
 * @brief Creates and returns the IBus component representing Unikey.
 *
 * The caller owns the returned IBusComponent and must release it with
 * g_object_unref() after use.
 *
 * @return New IBusComponent instance describing the Unikey engine.
 */
static IBusComponent* ibus_unikey_get_component()
{
    IBusComponent* component;
    IBusEngineDesc* engine;

    component = ibus_component_new("org.freedesktop.IBus.Unikey",
                                   "Unikey component",
                                   PACKAGE_VERSION,
                                   "GPLv3",
                                   "Vietnamese input group",
                                   PACKAGE_BUGREPORT,
                                   "",
                                   PACKAGE_NAME);

    engine = ibus_engine_desc_new_varargs ("name",        "Unikey",
                                           "longname",    "Unikey",
                                           "description", IU_DESC,
                                           "language",    "vi",
                                           "license",     "GPLv3",
                                           "author",      "Vietnamese input group",
                                           "icon",        PKGDATADIR "/icons/ibus-unikey.svg",
                                           "layout",      "*",
                                           "rank",        99,
                                           "setup",       LIBEXECDIR "/ibus-setup-unikey",
                                           NULL);

    ibus_component_add_engine(component, engine);

    return component;
}