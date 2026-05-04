/**
 * @file engine.cpp
 * @brief Implements the internal Unikey IBus engine logic.
 *
 * Provides private engine registration, key event processing, preedit buffer
 * management, and integration with Unikey configuration options.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <libintl.h>
#include <stdlib.h>

#include <sys/wait.h>
#include <string.h>
#include <ibus.h>

#include "unikey.h"
#include "vnconv.h"

#include "engine_private.h"
#include "unikey_config.h"

/** Shorthand macro for gettext localization. */
#define _(string) gettext(string)

/** Size of the temporary buffer used for legacy charset conversion. */
#define CONVERT_BUF_SIZE 1024

/** Symbols that count as word breaks when deciding whether to commit text. */
static unsigned char WordBreakSyms[] =
    {
        ',', ';', ':', '.', '"', '\'', '!', '?', ' ',
        '<', '>', '=', '+', '-', '*', '/', '\\',
        '_', '~', '`', '@', '#', '$', '%', '^', '&', '(', ')', '{', '}', '[', ']',
        '|'};

/** Cached parent engine class used to chain up to default implementations. */
static IBusEngineClass *parent_class = NULL;

/** Current focused Unikey engine instance. */
static IBusUnikeyEngine *unikey = NULL;

/**
 * @brief Returns the GType for the Unikey engine.
 *
 * Registers the engine type with the IBus type system on first call and
 * returns the resulting type identifier.
 *
 * @return GType for IBusUnikeyEngine.
 */
GType ibus_unikey_engine_get_type(void)
{
    static GType type = 0;

    static const GTypeInfo type_info = {
        sizeof(IBusUnikeyEngineClass),
        (GBaseInitFunc)NULL,
        (GBaseFinalizeFunc)NULL,
        (GClassInitFunc)ibus_unikey_engine_class_init,
        NULL,
        NULL,
        sizeof(IBusUnikeyEngine),
        0,
        (GInstanceInitFunc)ibus_unikey_engine_init,
    };

    if (type == 0)
    {
        type = g_type_register_static(IBUS_TYPE_ENGINE,
                                      "IBusUnikeyEngine",
                                      &type_info,
                                      (GTypeFlags)0);
    }

    return type;
}

/**
 * @brief Initialize the Unikey engine runtime.
 *
 * @param bus Initialized IBus connection used by the engine.
 */
void ibus_unikey_init(IBusBus *bus)
{
    UnikeySetup();
    ibus_unikey_config_init();

    ibus_unikey_config_on_changed(ibus_unikey_config_value_changed, NULL);
}

/**
 * @brief Shutdown and clean up the Unikey engine runtime.
 */
void ibus_unikey_exit()
{
    UnikeyCleanup();
}

/**
 * @brief Initialize the Unikey engine class and hook GObject callbacks.
 *
 * @param klass Engine class structure to initialize.
 */
static void ibus_unikey_engine_class_init(IBusUnikeyEngineClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS(klass);
    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);

    parent_class = (IBusEngineClass *)g_type_class_peek_parent(klass);

    object_class->constructor = ibus_unikey_engine_constructor;
    ibus_object_class->destroy = (IBusObjectDestroyFunc)ibus_unikey_engine_destroy;

    engine_class->process_key_event = ibus_unikey_engine_process_key_event;
    engine_class->reset = ibus_unikey_engine_reset;
    engine_class->enable = ibus_unikey_engine_enable;
    engine_class->disable = ibus_unikey_engine_disable;
    engine_class->focus_in = ibus_unikey_engine_focus_in;
    engine_class->focus_out = ibus_unikey_engine_focus_out;
    engine_class->property_activate = ibus_unikey_engine_property_activate;
}

/**
 * @brief Initialize a new Unikey engine instance.
 *
 * @param unikey Engine instance being initialized.
 */
static void ibus_unikey_engine_init(IBusUnikeyEngine *unikey)
{
    ibus_unikey_engine_load_config(unikey);

    UnikeySetInputMethod(unikey->im);
    UnikeySetOutputCharset(unikey->oc);
    UnikeySetOptions(&unikey->ukopt);

    unikey->preeditstr = new std::string();
    ibus_unikey_engine_create_property_list(unikey);
}

/**
 * @brief Find a property object by key in a property list.
 *
 * @param list Property list to search.
 * @param key Property key to match.
 * @return Matching IBusProperty, or NULL if not found.
 */
static IBusProperty *find_prop_from_list(IBusPropList *list, const char *key)
{
    for (guint i = 0; i < list->properties->len; i++)
    {
        IBusProperty *prop = ibus_prop_list_get(list, i);
        if (prop == NULL)
            return NULL;
        if (strcmp(ibus_property_get_key(prop), key) == 0)
            return prop;
    }
    return NULL;
}

/**
 * @brief Synchronize IBus property states with the current engine config.
 *
 * @param unikey Engine instance whose property list should be updated.
 */
static void ibus_unikey_engine_update_property_list(IBusUnikeyEngine *unikey)
{
    bool b;
    IBusProperty *prop;

    b = unikey->ukopt.spellCheckEnabled;
    prop = find_prop_from_list(unikey->prop_list, CONFIG_SPELLCHECK);
    if (prop != NULL)
    {
        ibus_property_set_state(prop,
                                (b == 1) ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED);
    }

    b = unikey->ukopt.autoNonVnRestore;
    prop = find_prop_from_list(unikey->prop_list, CONFIG_AUTORESTORENONVN);
    if (prop != NULL)
    {
        ibus_property_set_state(prop,
                                (b == 1) ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED);
    }

    b = unikey->ukopt.macroEnabled;
    prop = find_prop_from_list(unikey->prop_list, CONFIG_MACROENABLED);
    if (prop != NULL)
    {
        ibus_property_set_state(prop,
                                (b == 1) ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED);
    }
}

/**
 * @brief Load engine configuration values from GSettings.
 *
 * @param unikey Engine instance to configure.
 */
static void ibus_unikey_engine_load_config(IBusUnikeyEngine *unikey)
{
    gchar *str;
    gboolean b;

    auto im = input_method_map.at("telex").first;
    if (ibus_unikey_config_get_string(CONFIG_INPUTMETHOD, &str))
    {
        auto p = input_method_map.at(std::string(str));
        im = p.first;
        g_free(str);
    }
    unikey->im = im;

    auto oc = output_charset_map.at("unicode").first;
    if (ibus_unikey_config_get_string(CONFIG_OUTPUTCHARSET, &str))
    {
        auto p = output_charset_map.at(std::string(str));
        oc = p.first;
        g_free(str);
    }
    unikey->oc = oc;

    if (ibus_unikey_config_get_boolean(CONFIG_FREEMARKING, &b))
        unikey->ukopt.freeMarking = b;

    if (ibus_unikey_config_get_boolean(CONFIG_MODERNSTYLE, &b))
        unikey->ukopt.modernStyle = b;

    if (ibus_unikey_config_get_boolean(CONFIG_MACROENABLED, &b))
        unikey->ukopt.macroEnabled = b;

    if (ibus_unikey_config_get_boolean(CONFIG_SPELLCHECK, &b))
        unikey->ukopt.spellCheckEnabled = b;

    if (ibus_unikey_config_get_boolean(CONFIG_AUTORESTORENONVN, &b))
        unikey->ukopt.autoNonVnRestore = b;

    if (ibus_unikey_config_get_boolean(CONFIG_STANDALONEW, &b))
        unikey->process_w_at_begin = b;

    // load macro
    gchar *fn = get_macro_file();
    UnikeyLoadMacroTable(fn);
    g_free(fn);
}

/**
 * @brief Construct a new Unikey engine object.
 *
 * @param type GType of the object being created.
 * @param n_construct_params Number of construction parameters.
 * @param construct_params Array of construction parameters.
 * @return GObject* Newly constructed engine object.
 */
static GObject *ibus_unikey_engine_constructor(GType type,
                                               guint n_construct_params,
                                               GObjectConstructParam *construct_params)
{
    IBusUnikeyEngine *unikey;

    unikey = (IBusUnikeyEngine *)
                 G_OBJECT_CLASS(parent_class)
                     ->constructor(type,
                                   n_construct_params,
                                   construct_params);

    return (GObject *)unikey;
}

/**
 * @brief Destroy a Unikey engine instance.
 *
 * @param unikey Engine instance to destroy.
 */
static void ibus_unikey_engine_destroy(IBusUnikeyEngine *unikey)
{
    delete unikey->preeditstr;
    g_object_unref(unikey->prop_list);

    IBUS_OBJECT_CLASS(parent_class)->destroy((IBusObject *)unikey);
}

/**
 * @brief Reset the engine preedit buffer and hide any pending text.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_buffer_reset(IBusEngine *engine)
{
    unikey = (IBusUnikeyEngine *)engine;

    ibus_engine_hide_preedit_text(engine);
    unikey->preeditstr->clear();
    UnikeyResetBuf();
}

/**
 * @brief Commit the current preedit text to the input context.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_buffer_commit(IBusEngine *engine)
{
    unikey = (IBusUnikeyEngine *)engine;

    if (unikey->preeditstr->length() > 0)
    {
        IBusText *text;
        text = ibus_text_new_from_static_string(unikey->preeditstr->c_str());
        ibus_engine_commit_text(engine, text);
    }

    ibus_unikey_buffer_reset(engine);
}

/**
 * @brief Handle engine focus enter events.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_focus_in(IBusEngine *engine)
{
    unikey = (IBusUnikeyEngine *)engine;
    ibus_engine_register_properties(engine, unikey->prop_list);

    parent_class->focus_in(engine);
}

/**
 * @brief Handle engine focus leave events.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_focus_out(IBusEngine *engine)
{
    ibus_unikey_buffer_reset(engine);
    parent_class->focus_out(engine);
}

/**
 * @brief Reset the engine state when the input context changes.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_reset(IBusEngine *engine)
{
    ibus_unikey_buffer_reset(engine);
    parent_class->reset(engine);
}

/**
 * @brief Enable the engine when it becomes active.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_enable(IBusEngine *engine)
{
    parent_class->enable(engine);
}

/**
 * @brief Disable the engine when it becomes inactive.
 *
 * @param engine IBus engine instance.
 */
static void ibus_unikey_engine_disable(IBusEngine *engine)
{
    parent_class->disable(engine);
}

/**
 * @brief Callback invoked when a configuration value changes.
 *
 * @param name Name of the changed setting.
 * @param user_data Opaque user data supplied by the signal.
 */
static void ibus_unikey_config_value_changed(gchar *name, gpointer user_data)
{
    ibus_unikey_engine_load_config(unikey);

    UnikeySetInputMethod(unikey->im);
    UnikeySetOutputCharset(unikey->oc);
    UnikeySetOptions(&unikey->ukopt);

    ibus_unikey_engine_update_property_list(unikey);
}

/**
 * @brief Handle activation of a property in the IBus property list.
 *
 * @param engine IBus engine instance.
 * @param prop_name Name of the activated property.
 * @param prop_state New property state.
 */
static void ibus_unikey_engine_property_activate(IBusEngine *engine,
                                                 const gchar *prop_name,
                                                 guint prop_state)
{
    unikey = (IBusUnikeyEngine *)engine;

    if (strcmp(prop_name, "more-settings") == 0)
    {
        int ret = 0;

        ret = system(LIBEXECDIR "/ibus-setup-unikey &");
        if (ret == -1)
        {
            g_print("Failed to open ibus-setup-unikey");
        }
        return;
    }

    if (strcmp(prop_name, CONFIG_SPELLCHECK) == 0)
    {
        unikey->ukopt.spellCheckEnabled = prop_state > 0;
        ibus_unikey_config_set_boolean(prop_name, prop_state > 0);
    }
    else if (strcmp(prop_name, CONFIG_AUTORESTORENONVN) == 0)
    {
        unikey->ukopt.autoNonVnRestore = prop_state > 0;
        ibus_unikey_config_set_boolean(prop_name, prop_state > 0);
    }
    else if (strcmp(prop_name, CONFIG_MACROENABLED) == 0)
    {
        unikey->ukopt.macroEnabled = prop_state > 0;
        ibus_unikey_config_set_boolean(prop_name, prop_state > 0);
    }
}

/**
 * @brief Create and publish the IBus property list for the engine.
 *
 * @param unikey Engine instance that will own the property list.
 */
static void ibus_unikey_engine_create_property_list(IBusUnikeyEngine *unikey)
{
    IBusProperty *prop;
    IBusText *label;

    if (unikey->prop_list != NULL)
        return;
    unikey->prop_list = ibus_prop_list_new();
    g_object_ref_sink(unikey->prop_list);

    // spellcheck property
    label = ibus_text_new_from_static_string(_("Enable spell check"));
    prop = ibus_property_new(CONFIG_SPELLCHECK,
                             PROP_TYPE_TOGGLE,
                             label,
                             "",
                             NULL,
                             TRUE,
                             TRUE,
                             (unikey->ukopt.spellCheckEnabled == 1) ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED,
                             NULL);
    if (ibus_prop_list_update_property(unikey->prop_list, prop) == false)
        ibus_prop_list_append(unikey->prop_list, prop);

    // auto restore property
    label = ibus_text_new_from_static_string(_("Auto restore non Vietnamese word"));
    prop = ibus_property_new(CONFIG_AUTORESTORENONVN,
                             PROP_TYPE_TOGGLE,
                             label,
                             "",
                             NULL,
                             TRUE,
                             TRUE,
                             (unikey->ukopt.autoNonVnRestore == 1) ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED,
                             NULL);
    if (ibus_prop_list_update_property(unikey->prop_list, prop) == false)
        ibus_prop_list_append(unikey->prop_list, prop);

    // macroEnabled property
    label = ibus_text_new_from_static_string(_("Enable Macro"));
    prop = ibus_property_new(CONFIG_MACROENABLED,
                             PROP_TYPE_TOGGLE,
                             label,
                             "",
                             NULL,
                             TRUE,
                             TRUE,
                             (unikey->ukopt.macroEnabled == 1) ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED,
                             NULL);
    if (ibus_prop_list_update_property(unikey->prop_list, prop) == false)
        ibus_prop_list_append(unikey->prop_list, prop);

    // more setting property
    label = ibus_text_new_from_static_string(_("More settings..."));
    prop = ibus_property_new("more-settings",
                             PROP_TYPE_NORMAL,
                             label,
                             "",
                             NULL,
                             TRUE,
                             TRUE,
                             PROP_STATE_UNCHECKED,
                             NULL);
    if (ibus_prop_list_update_property(unikey->prop_list, prop) == false)
        ibus_prop_list_append(unikey->prop_list, prop);
}

/**
 * @brief Update the visible preedit string for input composition.
 *
 * @param engine IBus engine instance.
 * @param string UTF-8 string to display.
 * @param visible Whether the preedit text should be visible.
 */
static void ibus_unikey_engine_update_preedit_string(IBusEngine *engine, const gchar *string, gboolean visible)
{
    IBusText *text;

    text = ibus_text_new_from_static_string(string);

    // underline text
    ibus_text_append_attribute(text, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_SINGLE, 0, -1);

    // update and display text
    ibus_engine_update_preedit_text_with_mode(engine, text, ibus_text_get_length(text), visible, IBUS_ENGINE_PREEDIT_COMMIT);
}

/**
 * @brief Erase a number of characters from the preedit buffer.
 *
 * Correctly handles UTF-8 multi-byte characters by skipping trailing bytes.
 *
 * @param engine IBus engine instance.
 * @param count Number of characters to remove.
 */
static void ibus_unikey_engine_erase_chars(IBusEngine *engine, int count)
{
    int i = unikey->preeditstr->length();

    while (i > 0 && count > 0)
    {
        unsigned char code = unikey->preeditstr->at(i - 1);

        // count down if code is the first byte of utf-8 char
        // REF: http://en.wikipedia.org/wiki/UTF-8
        if (code >> 6 != 2)
        { // ignore 10xxxxxx
            count--;
        }
        i--;
    }
    unikey->preeditstr->erase(i);
}

/**
 * @brief Convert legacy Latin-encoded data to UTF-8.
 *
 * This function is used when the output charset is not already UTF-8.
 *
 * @param dst Destination buffer for UTF-8 data.
 * @param src Source buffer containing Latin-encoded bytes.
 * @param inSize Number of input bytes.
 * @param pOutSize Pointer to the remaining destination buffer size.
 * @return Non-zero if the conversion completed without overflowing the buffer.
 */
int latinToUtf(unsigned char *dst, unsigned char *src, int inSize, int *pOutSize)
{
    int i;
    int outLeft;
    unsigned char ch;

    outLeft = *pOutSize;

    for (i = 0; i < inSize; i++)
    {
        ch = *src++;
        if (ch < 0x80)
        {
            outLeft -= 1;
            if (outLeft >= 0)
                *dst++ = ch;
        }
        else
        {
            outLeft -= 2;
            if (outLeft >= 0)
            {
                *dst++ = (0xC0 | ch >> 6);
                *dst++ = (0x80 | (ch & 0x3F));
            }
        }
    }

    *pOutSize = outLeft;
    return (outLeft >= 0);
}

static gboolean ibus_unikey_engine_process_key_event_preedit(IBusEngine *engine,
                                                             guint keyval,
                                                             guint keycode,
                                                             guint modifiers);

namespace {

/**
 * @brief Ignore key-up events for preedit (IBus sends press and release).
 */
gboolean ibus_key_event_is_release(guint modifiers)
{
    return (modifiers & IBUS_RELEASE_MASK) != 0;
}

/**
 * @brief Keys that commit preedit and must be handled by the client (navigation, shortcuts).
 */
gboolean ibus_key_should_commit_and_bypass_to_client(guint keyval, guint modifiers)
{
    const gboolean modifier_ctrl_or_alt = (modifiers & IBUS_CONTROL_MASK) != 0 ||
                                          (modifiers & IBUS_MOD1_MASK) != 0;
    const gboolean key_is_control_left_or_right =
        keyval == IBUS_Control_L || keyval == IBUS_Control_R;
    const gboolean key_is_tab = keyval == IBUS_Tab;
    const gboolean key_is_return = keyval == IBUS_Return;
    const gboolean key_is_delete = keyval == IBUS_Delete;
    const gboolean key_is_kp_enter = keyval == IBUS_KP_Enter;
    const gboolean key_in_main_nav_cluster =
        keyval >= IBUS_Home && keyval <= IBUS_Insert;
    const gboolean key_in_kp_nav_cluster =
        keyval >= IBUS_KP_Home && keyval <= IBUS_KP_Delete;

    return modifier_ctrl_or_alt || key_is_control_left_or_right || key_is_tab ||
           key_is_return || key_is_delete || key_is_kp_enter ||
           key_in_main_nav_cluster || key_in_kp_nav_cluster;
}

/**
 * @brief Modifier-only keys that never insert text (Caps…Hyper, or lone Shift press).
 */
gboolean ibus_key_is_modifier_only_no_text(guint keyval, guint modifiers)
{
    const gboolean key_in_caps_through_hyper =
        keyval >= IBUS_Caps_Lock && keyval <= IBUS_Hyper_R;
    const gboolean bare_shift_key = (modifiers & IBUS_SHIFT_MASK) == 0 &&
                                    (keyval == IBUS_Shift_L || keyval == IBUS_Shift_R);
    return key_in_caps_through_hyper || bare_shift_key;
}

gboolean ibus_keyval_is_printable_or_bare_shift(guint keyval)
{
    const gboolean in_printable_ascii =
        keyval >= IBUS_space && keyval <= IBUS_asciitilde;
    const gboolean is_shift_key =
        keyval == IBUS_Shift_L || keyval == IBUS_Shift_R;
    return in_printable_ascii || is_shift_key;
}

gboolean ibus_keyval_is_keypad_mul_through_9(guint keyval)
{
    return keyval >= IBUS_KP_Multiply && keyval <= IBUS_KP_9;
}

/**
 * @brief Append `UnikeyBuf` / `UnikeyBufChars` to preedit (UTF-8 or legacy→UTF-8).
 */
void ibus_unikey_preedit_append_unikey_output_bytes(IBusEngine *engine)
{
    if (UnikeyBufChars <= 0)
        return;
    IBusUnikeyEngine *uk = (IBusUnikeyEngine *)engine;
    if (uk->oc == CONV_CHARSET_XUTF8)
    {
        uk->preeditstr->append((const gchar *)UnikeyBuf, UnikeyBufChars);
        return;
    }
    static unsigned char buf[CONVERT_BUF_SIZE];
    int bufSize = CONVERT_BUF_SIZE;
    latinToUtf(buf, UnikeyBuf, UnikeyBufChars, &bufSize);
    uk->preeditstr->append((const gchar *)buf, CONVERT_BUF_SIZE - bufSize);
}

/**
 * @brief Commit preedit when last preedit byte and keyval both match a word-break symbol.
 * @return TRUE if committed.
 */
gboolean ibus_unikey_preedit_try_commit_word_break(IBusEngine *engine, guint keyval)
{
    IBusUnikeyEngine *uk = (IBusUnikeyEngine *)engine;
    if (uk->preeditstr->length() == 0)
        return FALSE;
    static guint i;
    const char last_preedit_byte = uk->preeditstr->at(uk->preeditstr->length() - 1);
    for (i = 0; i < sizeof(WordBreakSyms); i++)
    {
        const gboolean break_matches_tail_and_key =
            WordBreakSyms[i] == (unsigned char)last_preedit_byte &&
            WordBreakSyms[i] == keyval;
        if (break_matches_tail_and_key)
        {
            ibus_unikey_buffer_commit(engine);
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Telex/STelex2: pass standalone `w`/`W` through when option disables `ư` mapping.
 */
gboolean ibus_unikey_try_telex_standalone_w(IBusEngine *engine, guint keyval)
{
    IBusUnikeyEngine *uk = unikey;
    const gboolean im_allows_standalone_w_rule =
        uk->im == UkTelex || uk->im == UkSimpleTelex2;
    const gboolean standalone_w_as_uw_disabled = !uk->process_w_at_begin;
    const gboolean at_word_start = UnikeyAtWordBeginning();
    const gboolean key_is_ascii_w = keyval == IBUS_w || keyval == IBUS_W;
    if (!im_allows_standalone_w_rule || !standalone_w_as_uw_disabled || !at_word_start ||
        !key_is_ascii_w)
        return FALSE;

    UnikeyPutChar(keyval);
    uk->preeditstr->append(keyval == IBUS_w ? "w" : "W");
    ibus_unikey_engine_update_preedit_string(engine, uk->preeditstr->c_str(), TRUE);
    return TRUE;
}

/**
 * @brief Backspace path: Unikey backspace, trim preedit, re-append engine output if any.
 */
gboolean ibus_unikey_handle_backspace_preedit(IBusEngine *engine)
{
    UnikeyBackspacePress();

    const gboolean engine_did_not_request_backspace =
        UnikeyBackspaces == 0 || unikey->preeditstr->empty();
    if (engine_did_not_request_backspace)
        return FALSE;

    if (unikey->preeditstr->length() <= (guint)UnikeyBackspaces)
        ibus_unikey_buffer_reset(engine);
    else
    {
        ibus_unikey_engine_erase_chars(engine, UnikeyBackspaces);
        ibus_unikey_engine_update_preedit_string(engine, unikey->preeditstr->c_str(), TRUE);
    }

    if (UnikeyBufChars > 0)
    {
        ibus_unikey_preedit_append_unikey_output_bytes(engine);
        ibus_unikey_engine_update_preedit_string(engine, unikey->preeditstr->c_str(), TRUE);
    }
    return TRUE;
}

/**
 * @brief Printable / shift path: filter or restore, apply Unikey output, word-break commit.
 */
gboolean ibus_unikey_handle_printable_preedit(IBusEngine *engine, guint keyval,
                                              guint modifiers)
{
    UnikeySetCapsState(modifiers & IBUS_SHIFT_MASK, modifiers & IBUS_LOCK_MASK);

    if (ibus_unikey_try_telex_standalone_w(engine, keyval))
        return TRUE;

    const gboolean shift_plus_space_mid_word =
        unikey->last_key_with_shift == false && (modifiers & IBUS_SHIFT_MASK) != 0 &&
        keyval == IBUS_space && !UnikeyAtWordBeginning();
    const gboolean bare_shift_key_event =
        keyval == IBUS_Shift_L || keyval == IBUS_Shift_R;
    if (shift_plus_space_mid_word || bare_shift_key_event)
        UnikeyRestoreKeyStrokes();
    else
        UnikeyFilter(keyval);

    if (UnikeyBackspaces > 0)
    {
        if (unikey->preeditstr->length() <= (guint)UnikeyBackspaces)
            unikey->preeditstr->clear();
        else
            ibus_unikey_engine_erase_chars(engine, UnikeyBackspaces);
    }

    if (UnikeyBufChars > 0)
        ibus_unikey_preedit_append_unikey_output_bytes(engine);
    else if (keyval != IBUS_Shift_L && keyval != IBUS_Shift_R)
    {
        static int n;
        static char s[6];
        n = g_unichar_to_utf8(keyval, s);
        unikey->preeditstr->append(s, n);
    }

    if (ibus_unikey_preedit_try_commit_word_break(engine, keyval))
        return TRUE;

    ibus_unikey_engine_update_preedit_string(engine, unikey->preeditstr->c_str(), TRUE);
    return TRUE;
}

} // namespace

/**
 * @brief Process a raw key event for the Unikey engine.
 *
 * This wrapper updates state tracking and delegates handling to the preedit
 * processor.
 *
 * @param engine IBus engine instance.
 * @param keyval Unicode key value.
 * @param keycode Platform-specific scancode.
 * @param modifiers Modifier mask from IBus.
 * @return TRUE if the event was consumed, FALSE otherwise.
 */
static gboolean ibus_unikey_engine_process_key_event(IBusEngine *engine,
                                                     guint keyval,
                                                     guint keycode,
                                                     guint modifiers)
{
    static gboolean tmp;

    unikey = (IBusUnikeyEngine *)engine;

    tmp = ibus_unikey_engine_process_key_event_preedit(engine, keyval, keycode, modifiers);

    const gboolean keyval_in_printable_ascii =
        keyval >= IBUS_space && keyval <= IBUS_asciitilde;
    if (keyval_in_printable_ascii)
        unikey->last_key_with_shift = modifiers & IBUS_SHIFT_MASK;
    else
        unikey->last_key_with_shift = FALSE;

    return tmp;
}

/**
 * @brief Handle key events for preedit composition and commit logic.
 *
 * @param engine IBus engine instance.
 * @param keyval Unicode key value.
 * @param keycode Platform-specific scancode.
 * @param modifiers Modifier mask from IBus.
 * @return TRUE if the event was consumed, FALSE otherwise.
 */
static gboolean ibus_unikey_engine_process_key_event_preedit(IBusEngine *engine,
                                                             guint keyval,
                                                             guint keycode,
                                                             guint modifiers)
{
    if (ibus_key_event_is_release(modifiers))
        return false;

    if (ibus_key_should_commit_and_bypass_to_client(keyval, modifiers))
    {
        ibus_unikey_buffer_commit(engine);
        return false;
    }

    if (ibus_key_is_modifier_only_no_text(keyval, modifiers))
        return false;

    if (keyval == IBUS_BackSpace)
        return ibus_unikey_handle_backspace_preedit(engine);

    if (ibus_keyval_is_keypad_mul_through_9(keyval))
    {
        ibus_unikey_buffer_commit(engine);
        return false;
    }

    if (ibus_keyval_is_printable_or_bare_shift(keyval))
        return ibus_unikey_handle_printable_preedit(engine, keyval, modifiers);

    ibus_unikey_buffer_commit(engine);
    return false;
}
