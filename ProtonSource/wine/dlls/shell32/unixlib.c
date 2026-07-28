#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#ifdef SONAME_LIBDBUS_1
#include <dbus/dbus.h>
#endif

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winternl.h>

#include <wine/debug.h>
#include <wine/unixlib.h>

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

#ifdef SONAME_LIBDBUS_1

#define DBUS_FUNCS \
    DO_FUNC(dbus_bus_add_match); \
    DO_FUNC(dbus_bus_get); \
    DO_FUNC(dbus_connection_add_filter); \
    DO_FUNC(dbus_connection_read_write_dispatch); \
    DO_FUNC(dbus_connection_remove_filter); \
    DO_FUNC(dbus_connection_send_with_reply_and_block); \
    DO_FUNC(dbus_connection_unref); \
    DO_FUNC(dbus_error_free); \
    DO_FUNC(dbus_error_init); \
    DO_FUNC(dbus_message_get_args); \
    DO_FUNC(dbus_message_get_path); \
    DO_FUNC(dbus_message_is_signal); \
    DO_FUNC(dbus_message_iter_append_basic); \
    DO_FUNC(dbus_message_iter_append_fixed_array); \
    DO_FUNC(dbus_message_iter_close_container); \
    DO_FUNC(dbus_message_iter_get_arg_type); \
    DO_FUNC(dbus_message_iter_get_basic); \
    DO_FUNC(dbus_message_iter_init); \
    DO_FUNC(dbus_message_iter_init_append); \
    DO_FUNC(dbus_message_iter_next); \
    DO_FUNC(dbus_message_iter_open_container); \
    DO_FUNC(dbus_message_iter_recurse); \
    DO_FUNC(dbus_message_new_method_call); \
    DO_FUNC(dbus_message_unref)

#define DO_FUNC(f) typeof(f) (*p_##f)
DBUS_FUNCS;
#undef DO_FUNC

static BOOL load_dbus_functions(void)
{
    void *handle = dlopen(SONAME_LIBDBUS_1, RTLD_NOW);

    if (!handle)
    {
        WARN("failed to load %s: %s\n", SONAME_LIBDBUS_1, dlerror());
        return FALSE;
    }

#define DO_FUNC(f) if (!(p_##f = dlsym(handle, #f))) \
    { WARN("failed to load symbol %s\n", #f); return FALSE; }
    DBUS_FUNCS;
#undef DO_FUNC
    return TRUE;
}

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_FILECHOOSER_IFACE "org.freedesktop.portal.FileChooser"
#define PORTAL_REQUEST_IFACE "org.freedesktop.portal.Request"

struct pending_response
{
    const char *request_path;
    BOOL done;
    dbus_uint32_t response_code;
    char *first_uri; /* malloc'd, NULL if none returned */
};

static void append_string_option(DBusMessageIter *options, const char *key, const char *value)
{
    DBusMessageIter entry, variant;

    p_dbus_message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    p_dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    p_dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    p_dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value);
    p_dbus_message_iter_close_container(&entry, &variant);
    p_dbus_message_iter_close_container(options, &entry);
}

static void append_bool_option(DBusMessageIter *options, const char *key, dbus_bool_t value)
{
    DBusMessageIter entry, variant;

    p_dbus_message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    p_dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    p_dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
    p_dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
    p_dbus_message_iter_close_container(&entry, &variant);
    p_dbus_message_iter_close_container(options, &entry);
}

/* current_folder/current_file are "ay": a nul-terminated raw path, not a URI. */
static void append_path_option(DBusMessageIter *options, const char *key, const char *path)
{
    DBusMessageIter entry, variant, bytes;
    int len = strlen(path) + 1;

    p_dbus_message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    p_dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    p_dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "ay", &variant);
    p_dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "y", &bytes);
    p_dbus_message_iter_append_fixed_array(&bytes, DBUS_TYPE_BYTE, &path, len);
    p_dbus_message_iter_close_container(&variant, &bytes);
    p_dbus_message_iter_close_container(&entry, &variant);
    p_dbus_message_iter_close_container(options, &entry);
}

static DBusHandlerResult response_filter(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
    struct pending_response *pending = user_data;
    const char *path;
    DBusMessageIter args, results, entry, variant, uris;

    if (!p_dbus_message_is_signal(msg, PORTAL_REQUEST_IFACE, "Response"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    path = p_dbus_message_get_path(msg);
    if (!path || strcmp(path, pending->request_path))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    p_dbus_message_iter_init(msg, &args);
    p_dbus_message_iter_get_basic(&args, &pending->response_code);
    p_dbus_message_iter_next(&args);

    p_dbus_message_iter_recurse(&args, &results);
    while (p_dbus_message_iter_get_arg_type(&results) == DBUS_TYPE_DICT_ENTRY)
    {
        const char *key;

        p_dbus_message_iter_recurse(&results, &entry);
        p_dbus_message_iter_get_basic(&entry, &key);
        p_dbus_message_iter_next(&entry);

        if (!strcmp(key, "uris"))
        {
            p_dbus_message_iter_recurse(&entry, &variant);
            p_dbus_message_iter_recurse(&variant, &uris);
            if (p_dbus_message_iter_get_arg_type(&uris) == DBUS_TYPE_STRING)
            {
                const char *uri;
                p_dbus_message_iter_get_basic(&uris, &uri);
                free(pending->first_uri);
                pending->first_uri = strdup(uri);
            }
        }
        p_dbus_message_iter_next(&results);
    }

    pending->done = TRUE;
    return DBUS_HANDLER_RESULT_HANDLED;
}

/* Calls FileChooser.OpenFile and blocks for the Response signal. Returns FALSE
 * only if the portal itself couldn't be reached (caller should fall back to
 * Wine's native dialog); returns TRUE with *out_uri == NULL if the user
 * cancelled. *out_uri is malloc'd on success, caller frees it. */
static BOOL portal_open_file(const char *title, const char *initial_dir_path, BOOL directory,
                              char **out_uri)
{
    DBusError error;
    DBusConnection *conn;
    DBusMessage *msg, *reply;
    DBusMessageIter args, options;
    const char *parent_window = "";
    const char *handle_token = "tuxblox1";
    const char *request_path;
    struct pending_response pending = { 0 };
    char match_rule[512];

    *out_uri = NULL;
    if (!load_dbus_functions()) return FALSE;

    p_dbus_error_init(&error);
    conn = p_dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (!conn) { p_dbus_error_free(&error); return FALSE; }

    msg = p_dbus_message_new_method_call(PORTAL_BUS_NAME, PORTAL_OBJECT_PATH,
                                          PORTAL_FILECHOOSER_IFACE, "OpenFile");
    p_dbus_message_iter_init_append(msg, &args);
    p_dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parent_window);
    p_dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title);
    p_dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &options);
    append_string_option(&options, "handle_token", handle_token);
    append_bool_option(&options, "directory", directory);
    append_bool_option(&options, "multiple", FALSE);
    if (initial_dir_path && initial_dir_path[0])
        append_path_option(&options, "current_folder", initial_dir_path);
    p_dbus_message_iter_close_container(&args, &options);

    reply = p_dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
    p_dbus_message_unref(msg);
    if (!reply) { p_dbus_error_free(&error); p_dbus_connection_unref(conn); return FALSE; }

    if (!p_dbus_message_get_args(reply, &error, DBUS_TYPE_OBJECT_PATH, &request_path,
                                  DBUS_TYPE_INVALID))
    {
        p_dbus_error_free(&error);
        p_dbus_message_unref(reply);
        p_dbus_connection_unref(conn);
        return FALSE;
    }

    pending.request_path = request_path;
    snprintf(match_rule, sizeof(match_rule),
             "type='signal',interface='%s',member='Response',path='%s'",
             PORTAL_REQUEST_IFACE, request_path);
    p_dbus_bus_add_match(conn, match_rule, NULL);
    if (!p_dbus_connection_add_filter(conn, response_filter, &pending, NULL))
    {
        WARN("failed to register dbus filter for portal response\n");
        p_dbus_message_unref(reply);
        p_dbus_connection_unref(conn);
        return FALSE;
    }

    /* read_write_dispatch() returns FALSE once the connection has been
     * fully disconnected (portal backend crash, compositor restart, bus
     * restart while the dialog is open, etc). Without checking this we'd
     * either busy-spin or block forever with no way for the caller to ever
     * regain control. Treat a dead connection the same as any other
     * portal-unreachable failure below, rather than reporting a false
     * "reachable, cancelled" result. */
    while (!pending.done)
    {
        if (!p_dbus_connection_read_write_dispatch(conn, -1))
        {
            WARN("dbus connection closed while waiting for portal response\n");
            break;
        }
    }

    p_dbus_connection_remove_filter(conn, response_filter, &pending);
    p_dbus_message_unref(reply);
    p_dbus_connection_unref(conn);

    if (!pending.done)
    {
        free(pending.first_uri);
        return FALSE;
    }

    if (pending.response_code == 0 && pending.first_uri)
        *out_uri = pending.first_uri;
    else
        free(pending.first_uri);
    return TRUE;
}

NTSTATUS portal_pick_folder(void *args)
{
    struct portal_pick_folder_params *params = args;
    char *initial_dir_unix = NULL, *uri = NULL;
    WCHAR *dos_path;
    const char *path;
    char title_utf8[256] = "Select Folder";

    params->cancelled = TRUE;

    if (params->initial_dir)
        ntdll_get_unix_file_name(params->initial_dir, &initial_dir_unix, FILE_OPEN_IF);

    if (!portal_open_file(title_utf8, initial_dir_unix, TRUE, &uri))
    {
        free(initial_dir_unix);
        return STATUS_NOT_SUPPORTED;
    }
    free(initial_dir_unix);

    if (!uri) return STATUS_SUCCESS; /* reachable, user cancelled */

    path = uri;
    if (!strncmp(path, "file://", 7)) path += 7;

    /* ntdll_get_dos_file_name returns STATUS_NO_SUCH_FILE (nonzero) but still
     * fills in a usable buffer when the leaf component doesn't exist yet with
     * disposition FILE_SUPERSEDE (0) - check the output pointer, not the
     * NTSTATUS, so a not-yet-existing target isn't mistaken for a hard
     * failure (see the equivalent fix and longer explanation in
     * dlls/comdlg32/unixlib.c's portal_open_file/portal_save_file). */
    ntdll_get_dos_file_name(path, &dos_path, 0);
    if (dos_path)
    {
        lstrcpynW(params->out_path, dos_path, params->out_path_len);
        free(dos_path);
        params->cancelled = FALSE;
    }
    free(uri);
    return STATUS_SUCCESS;
}

#else /* SONAME_LIBDBUS_1 */

NTSTATUS portal_pick_folder(void *args)
{
    struct portal_pick_folder_params *params = args;
    params->cancelled = TRUE;
    return STATUS_NOT_SUPPORTED;
}

#endif

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    portal_pick_folder,
};
