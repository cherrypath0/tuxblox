#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

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

WINE_DEFAULT_DEBUG_CHANNEL(commdlg);
WINE_DECLARE_DEBUG_CHANNEL(tuxblox);

#ifdef SONAME_LIBDBUS_1

#define DBUS_FUNCS \
    DO_FUNC(dbus_bus_add_match); \
    DO_FUNC(dbus_bus_get); \
    DO_FUNC(dbus_bus_get_unique_name); \
    DO_FUNC(dbus_bus_remove_match); \
    DO_FUNC(dbus_error_is_set); \
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
    const char *request_path;   /* the path subscribed to before the call was made */
    const char *reply_path;     /* the path the portal handed back, when it is a different one */
    BOOL done;
    dbus_uint32_t response_code;
    char **uris;     /* malloc'd array of malloc'd strings, NULL if none returned */
    UINT uri_count;
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

/* filters option type is "a(sa(us))": array of (display-name, array of (type, glob-pattern)) */
static void append_filters_option(DBusMessageIter *options, UINT count,
                                   const char **names_utf8, const char **patterns_utf8)
{
    DBusMessageIter entry, variant, arr, filter_struct, pattern_arr, pattern_struct;
    const char *key = "filters";
    UINT i;
    dbus_uint32_t glob_type = 0; /* 0 = glob pattern, per portal spec */

    p_dbus_message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    p_dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    p_dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant);
    p_dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(sa(us))", &arr);
    for (i = 0; i < count; i++)
    {
        /* Windows filter patterns pack multiple globs into one string
         * joined with ';' (e.g. "*.png;*.jpg;*.jpeg"), but the portal wants
         * each glob as its own (type, pattern) entry. Passing the whole
         * ';'-joined string through as a single glob matches nothing (a
         * filename never literally contains ';'), which is why the portal
         * picker showed folders (filters don't apply to them) but no files
         * at all whenever a filter listed more than one extension. */
        char *patterns_copy = strdup(patterns_utf8[i] ? patterns_utf8[i] : "");
        char *saveptr, *pattern;

        p_dbus_message_iter_open_container(&arr, DBUS_TYPE_STRUCT, NULL, &filter_struct);
        p_dbus_message_iter_append_basic(&filter_struct, DBUS_TYPE_STRING, &names_utf8[i]);
        p_dbus_message_iter_open_container(&filter_struct, DBUS_TYPE_ARRAY, "(us)", &pattern_arr);
        if (patterns_copy)
        {
            for (pattern = strtok_r(patterns_copy, ";", &saveptr); pattern;
                 pattern = strtok_r(NULL, ";", &saveptr))
            {
                p_dbus_message_iter_open_container(&pattern_arr, DBUS_TYPE_STRUCT, NULL, &pattern_struct);
                p_dbus_message_iter_append_basic(&pattern_struct, DBUS_TYPE_UINT32, &glob_type);
                p_dbus_message_iter_append_basic(&pattern_struct, DBUS_TYPE_STRING, &pattern);
                p_dbus_message_iter_close_container(&pattern_arr, &pattern_struct);
            }
            free(patterns_copy);
        }
        p_dbus_message_iter_close_container(&filter_struct, &pattern_arr);
        p_dbus_message_iter_close_container(&arr, &filter_struct);
    }
    p_dbus_message_iter_close_container(&variant, &arr);
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
    if (!path) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    if (strcmp(path, pending->request_path) &&
        (!pending->reply_path || strcmp(path, pending->reply_path)))
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
            while (p_dbus_message_iter_get_arg_type(&uris) == DBUS_TYPE_STRING)
            {
                const char *uri;
                char **new_uris = realloc(pending->uris, (pending->uri_count + 1) * sizeof(*new_uris));

                if (new_uris)
                {
                    p_dbus_message_iter_get_basic(&uris, &uri);
                    pending->uris = new_uris;
                    if ((pending->uris[pending->uri_count] = strdup(uri)))
                        pending->uri_count++;
                }
                p_dbus_message_iter_next(&uris);
            }
        }
        p_dbus_message_iter_next(&results);
    }

    pending->done = TRUE;
    return DBUS_HANDLER_RESULT_HANDLED;
}

/* The object path the portal will give this request, worked out the way the
 * portal spec says it is built: the caller's bus name with the leading ':'
 * dropped and every '.' turned into '_', then the handle_token. Knowing it in
 * advance is what lets the Response be subscribed to before the call is made.
 * NULL if the bus name cannot be read, which the caller treats as
 * portal-unreachable. */
static char *expected_request_path(DBusConnection *conn, const char *token)
{
    const char *unique = p_dbus_bus_get_unique_name(conn);
    char *path, *p;

    size_t prefix;

    if (!unique) return NULL;
    if (*unique == ':') unique++;
    if (!(path = malloc(strlen(PORTAL_OBJECT_PATH) + strlen(unique) + strlen(token) + 16)))
        return NULL;
    prefix = sprintf(path, PORTAL_OBJECT_PATH "/request/");
    strcpy(path + prefix, unique);
    for (p = path + prefix; *p; p++) if (*p == '.') *p = '_';
    sprintf(p, "/%s", token);
    return path;
}

/* Calls FileChooser.OpenFile or FileChooser.SaveFile and blocks for the
 * Response signal. Returns FALSE only if the portal itself couldn't be
 * reached (caller should fall back to Wine's native dialog); returns TRUE
 * with *out_count == 0 if the user cancelled. On success *out_uris is a
 * malloc'd array of malloc'd strings, *out_count entries; caller frees
 * each entry and the array. */
static BOOL portal_open_or_save(const char *method, const char *title, const char *initial_dir_path,
                                 const char *initial_name, BOOL directory, BOOL multiple,
                                 UINT filter_count, const char **names_utf8, const char **patterns_utf8,
                                 char ***out_uris, UINT *out_count)
{
    DBusError error;
    DBusConnection *conn;
    DBusMessage *msg, *reply;
    DBusMessageIter args, options;
    const char *parent_window = "";
    static LONG token_seq;
    char handle_token[32];
    char *expected_path;
    char *reply_match = NULL;
    const char *request_path;
    struct pending_response pending = { 0 };
    char match_rule[512];
    BOOL ret = FALSE;

    *out_uris = NULL;
    *out_count = 0;
    if (!load_dbus_functions()) return FALSE;

    p_dbus_error_init(&error);
    conn = p_dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (!conn) { p_dbus_error_free(&error); return FALSE; }

    /* A token of its own per dialog, as the portal spec requires: the Request
     * object is named after it, and two dialogs open at once under one name
     * collide on that object. */
    sprintf(handle_token, "tuxblox%u", (unsigned int)InterlockedIncrement(&token_seq));
    if (!(expected_path = expected_request_path(conn, handle_token)))
    {
        p_dbus_connection_unref(conn);
        return FALSE;
    }

    /* Subscribed to before the call is made, and with the error argument passed
     * so the bus has acknowledged the rule before anything is sent. The portal
     * may emit Response the instant the method returns, and the bus drops a
     * signal that no rule yet asks for -- so a rule added afterwards, or added
     * without waiting for the bus, loses the Response of a portal that answers
     * quickly and leaves the wait below running for ever on the thread that
     * opened the dialog. Measured: a portal answering with no delay hung the
     * caller indefinitely, every time. */
    pending.request_path = expected_path;
    snprintf(match_rule, sizeof(match_rule),
             "type='signal',interface='%s',member='Response',path='%s'",
             PORTAL_REQUEST_IFACE, expected_path);
    p_dbus_bus_add_match(conn, match_rule, &error);
    if (p_dbus_error_is_set(&error))
    {
        WARN("failed to subscribe to the portal response: %s\n", error.message);
        p_dbus_error_free(&error);
        free(expected_path);
        p_dbus_connection_unref(conn);
        return FALSE;
    }
    if (!p_dbus_connection_add_filter(conn, response_filter, &pending, NULL))
    {
        WARN("failed to register dbus filter for portal response\n");
        p_dbus_bus_remove_match(conn, match_rule, NULL);
        free(expected_path);
        p_dbus_connection_unref(conn);
        return FALSE;
    }

    msg = p_dbus_message_new_method_call(PORTAL_BUS_NAME, PORTAL_OBJECT_PATH,
                                          PORTAL_FILECHOOSER_IFACE, method);
    p_dbus_message_iter_init_append(msg, &args);
    p_dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parent_window);
    p_dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title);
    p_dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &options);
    append_string_option(&options, "handle_token", handle_token);
    if (!strcmp(method, "OpenFile"))
    {
        append_bool_option(&options, "directory", directory);
        append_bool_option(&options, "multiple", multiple);
    }
    if (initial_dir_path && initial_dir_path[0])
        append_path_option(&options, "current_folder", initial_dir_path);
    if (initial_name && initial_name[0])
        append_string_option(&options, "current_name", initial_name);
    if (filter_count)
        append_filters_option(&options, filter_count, names_utf8, patterns_utf8);
    p_dbus_message_iter_close_container(&args, &options);

    reply = p_dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
    p_dbus_message_unref(msg);
    if (!reply) { p_dbus_error_free(&error); goto done; }

    if (!p_dbus_message_get_args(reply, &error, DBUS_TYPE_OBJECT_PATH, &request_path,
                                  DBUS_TYPE_INVALID))
    {
        p_dbus_error_free(&error);
        p_dbus_message_unref(reply);
        goto done;
    }

    /* A portal is free to hand back a path other than the one its own naming
     * rule gives -- it renames around a token already in use, and an older one
     * may not follow the rule at all -- so whatever it named is listened to as
     * well rather than waiting for a signal that is never coming. The window
     * this one reopens is the small one between the call returning and here,
     * which is why the predicted path is still the one subscribed to first. */
    if (strcmp(request_path, expected_path))
    {
        pending.reply_path = request_path;
        if ((reply_match = malloc(sizeof(match_rule))))
        {
            snprintf(reply_match, sizeof(match_rule),
                     "type='signal',interface='%s',member='Response',path='%s'",
                     PORTAL_REQUEST_IFACE, request_path);
            p_dbus_bus_add_match(conn, reply_match, NULL);
        }
    }

    /* read_write_dispatch() returns FALSE once the connection has been
     * fully disconnected (portal backend crash, compositor restart, bus
     * restart while the dialog is open, etc). Without checking this we'd
     * either busy-spin or block forever with no way for the caller to ever
     * regain control. Treat a dead connection the same as any other
     * portal-unreachable failure below, rather than reporting a false
     * "reachable, cancelled" result. */
    /* This wait has no timeout (dbus_connection_read_write_dispatch's second
     * arg is -1), because the thing it is waiting for is a person deciding
     * what to call a file, which has no deadline worth setting. Everything
     * that could make the Response never arrive is therefore handled above
     * rather than here. Bracketed with tuxblox trace markers (item 6
     * investigation) so a WINEDEBUG=+tuxblox capture can show whether a hang
     * is stuck in here specifically -- a BEGIN with no matching END means yes. */
    TRACE_(tuxblox)("portal wait BEGIN method=%s title=%s\n", method, title);
    while (!pending.done)
    {
        if (!p_dbus_connection_read_write_dispatch(conn, -1))
        {
            WARN("dbus connection closed while waiting for portal response\n");
            break;
        }
    }
    TRACE_(tuxblox)("portal wait END method=%s done=%d code=%u\n",
                    method, pending.done, (unsigned int)pending.response_code);

    p_dbus_message_unref(reply);
    ret = pending.done;

done:
    /* The session connection is shared and outlives every dialog, so each rule
     * has to come off again: a token of its own per dialog means a rule of its
     * own per dialog, and a long session would otherwise walk into the bus's
     * own limit on how many one connection may hold. */
    p_dbus_connection_remove_filter(conn, response_filter, &pending);
    p_dbus_bus_remove_match(conn, match_rule, NULL);
    if (reply_match)
    {
        p_dbus_bus_remove_match(conn, reply_match, NULL);
        free(reply_match);
    }
    free(expected_path);
    p_dbus_connection_unref(conn);

    if (ret && pending.response_code == 0 && pending.uri_count)
    {
        *out_uris = pending.uris;
        *out_count = pending.uri_count;
    }
    else
    {
        UINT i;
        for (i = 0; i < pending.uri_count; i++) free(pending.uris[i]);
        free(pending.uris);
    }
    return ret;
}

static char *wcs_to_utf8(const WCHAR *src)
{
    ULONG srclen, retlen;
    char *ret;

    if (!src) return NULL;
    srclen = wcslen(src);
    if (!(ret = malloc(srclen * 3 + 1))) return NULL;
    retlen = ntdll_wcstoumbs(src, srclen, ret, srclen * 3, FALSE);
    ret[retlen] = 0;
    return ret;
}

/* Converts title/initial_dir/initial_name/filters from params (WCHAR) to
 * UTF-8, runs the portal round-trip, and frees the conversions again.
 * out_uris and out_count follow portal_open_or_save's contract. */
static BOOL run_portal_request(const char *method, struct portal_open_save_params *params,
                                BOOL directory, BOOL multiple, char ***out_uris, UINT *out_count)
{
    char *initial_dir_unix = NULL, *initial_name_utf8, *title_utf8;
    char *names_utf8[PORTAL_MAX_FILTERS] = { 0 }, *patterns_utf8[PORTAL_MAX_FILTERS] = { 0 };
    const char *names_ptr[PORTAL_MAX_FILTERS], *patterns_ptr[PORTAL_MAX_FILTERS];
    UINT filter_count = params->filter_count < PORTAL_MAX_FILTERS ? params->filter_count : PORTAL_MAX_FILTERS;
    UINT i;
    BOOL ok;

    title_utf8 = wcs_to_utf8(params->title);
    if (params->initial_dir)
    {
        char *host;

        ntdll_get_unix_file_name(params->initial_dir, &initial_dir_unix, FILE_OPEN_IF);
        /* The folder on the computer that one stands for, so the window opens on the user's own Documents rather than on the drive's copy of the path to it. */
        if (initial_dir_unix && (host = ntdll_host_folder_to_show(initial_dir_unix)))
        {
            free(initial_dir_unix);
            initial_dir_unix = host;
        }
    }
    initial_name_utf8 = wcs_to_utf8(params->initial_name);

    for (i = 0; i < filter_count; i++)
    {
        names_utf8[i] = wcs_to_utf8(params->filters[i].name);
        patterns_utf8[i] = wcs_to_utf8(params->filters[i].pattern);
        names_ptr[i] = names_utf8[i] ? names_utf8[i] : "";
        patterns_ptr[i] = patterns_utf8[i] ? patterns_utf8[i] : "";
    }

    ok = portal_open_or_save(method, title_utf8 ? title_utf8 : "", initial_dir_unix, initial_name_utf8,
                              directory, multiple, filter_count, names_ptr, patterns_ptr,
                              out_uris, out_count);

    free(title_utf8);
    free(initial_dir_unix);
    free(initial_name_utf8);
    for (i = 0; i < filter_count; i++)
    {
        free(names_utf8[i]);
        free(patterns_utf8[i]);
    }
    return ok;
}

/* Writes the OFN_EXPLORER result shape into out_buf: a single full path,
 * single nul-terminated, if only one file was picked; otherwise the
 * directory once, then each filename, double-nul terminated (matching
 * what GetOpenFileNameW's own OFN_ALLOWMULTISELECT|OFN_EXPLORER dialog
 * path produces). Returns FALSE (nothing written) if out_buf is too
 * small, having first set *required_len to the WCHAR count that would
 * have been needed, so callers can surface the same "needed size" hint
 * Wine's native FNERR_BUFFERTOOSMALL path already gives via lpstrFile[0]. */
static BOOL write_open_result(WCHAR * const *paths, UINT count, WCHAR *out_buf, SIZE_T out_buf_len,
                               SIZE_T *required_len)
{
    SIZE_T pos, dir_len;
    UINT i;
    WCHAR *sep;

    *required_len = 0;
    if (!count) return TRUE;

    if (count == 1)
    {
        SIZE_T len = wcslen(paths[0]) + 1;
        *required_len = len;
        if (len > out_buf_len) return FALSE;
        memcpy(out_buf, paths[0], len * sizeof(WCHAR));
        return TRUE;
    }

    sep = wcsrchr(paths[0], '\\');
    dir_len = sep ? (SIZE_T)(sep - paths[0]) : 0;

    /* Pre-pass: compute the total WCHAR count needed (dir + nul + each name
     * + nul + one final nul for the double-nul terminator) before writing
     * anything, so *required_len is populated even when out_buf is too
     * small to hold any of it. */
    pos = dir_len + 1;
    for (i = 0; i < count; i++)
    {
        WCHAR *name_sep = wcsrchr(paths[i], '\\');
        WCHAR *name = name_sep ? name_sep + 1 : paths[i];
        pos += wcslen(name) + 1;
    }
    *required_len = pos + 1;
    if (*required_len > out_buf_len) return FALSE;

    memcpy(out_buf, paths[0], dir_len * sizeof(WCHAR));
    out_buf[dir_len] = 0;
    pos = dir_len + 1;

    for (i = 0; i < count; i++)
    {
        WCHAR *name_sep = wcsrchr(paths[i], '\\');
        WCHAR *name = name_sep ? name_sep + 1 : paths[i];
        SIZE_T name_len = wcslen(name) + 1;

        memcpy(out_buf + pos, name, name_len * sizeof(WCHAR));
        pos += name_len;
    }

    out_buf[pos] = 0; /* extra nul: double-nul terminates the list */
    return TRUE;
}

static int uri_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* The portal answers with URIs, so anything a URI cannot hold arrives
 * percent-escaped: a space as %20, a non-ASCII character as its UTF-8 bytes.
 * Without this the escapes reach the application as part of the name and it
 * saves to "My%20Place.rbxl". Decoding in place is safe because an escape is
 * always longer than the byte it stands for. A truncated or invalid escape is
 * copied through verbatim, as is %00 - a nul would cut the path short. */
static void unescape_uri_path(char *path)
{
    char *out = path;

    while (*path)
    {
        int high, low;

        if (*path == '%' && (high = uri_hex_digit(path[1])) >= 0 &&
            (low = uri_hex_digit(path[2])) >= 0 && (high || low))
        {
            *out++ = high * 16 + low;
            path += 3;
        }
        else *out++ = *path++;
    }
    *out = 0;
}

NTSTATUS portal_open_file(void *args)
{
    struct portal_open_save_params *params = args;
    char **uris = NULL;
    UINT uri_count = 0, i, valid_count = 0;
    WCHAR **dos_paths;

    params->cancelled = TRUE;
    params->truncated = FALSE;
    params->required_len = 0;

    if (!run_portal_request("OpenFile", params, params->directory, params->multiple, &uris, &uri_count))
        return STATUS_NOT_SUPPORTED;

    /* Only safe to clear out_buf now: initial_name may alias out_buf (as it
     * does for GetSaveFileNameW, which points both at the same lpstrFile
     * buffer), and run_portal_request has already read/converted it above. */
    if (params->out_buf && params->out_buf_len)
        memset(params->out_buf, 0, params->out_buf_len * sizeof(WCHAR));

    if (!uri_count) return STATUS_SUCCESS; /* reachable, user cancelled */

    if ((dos_paths = calloc(uri_count, sizeof(*dos_paths))))
    {
        for (i = 0; i < uri_count; i++)
        {
            char *path = uris[i];
            WCHAR *dos_path = NULL, *localized;

            if (!strncmp(path, "file://", 7))
            {
                path += 7;
                unescape_uri_path(path);
            }
            /* ntdll_get_dos_file_name returns STATUS_NO_SUCH_FILE (nonzero)
             * but still fills in a usable buffer when the leaf component
             * doesn't exist yet with disposition FILE_SUPERSEDE (0) - check
             * the output pointer, not the NTSTATUS, so a not-yet-existing
             * target (the common case for a save, and possible here too)
             * isn't mistaken for a hard failure. */
            ntdll_get_dos_file_name(path, &dos_path, 0);
            if (dos_path && (localized = ntdll_localize_external_path(path, dos_path, TRUE)))
            {
                free(dos_path);
                dos_path = localized;
            }
            if (dos_path)
                dos_paths[valid_count++] = dos_path;
        }

        if (valid_count)
        {
            if (write_open_result(dos_paths, valid_count, params->out_buf, params->out_buf_len,
                                   &params->required_len))
                params->cancelled = FALSE;
            else
                params->truncated = TRUE;
        }

        for (i = 0; i < valid_count; i++) free(dos_paths[i]);
        free(dos_paths);
    }

    for (i = 0; i < uri_count; i++) free(uris[i]);
    free(uris);
    return STATUS_SUCCESS;
}

NTSTATUS portal_save_file(void *args)
{
    struct portal_open_save_params *params = args;
    char **uris = NULL;
    UINT uri_count = 0, i;

    params->cancelled = TRUE;
    params->truncated = FALSE;
    params->required_len = 0;

    if (!run_portal_request("SaveFile", params, FALSE, FALSE, &uris, &uri_count))
        return STATUS_NOT_SUPPORTED;

    /* Only safe to clear out_buf now: initial_name aliases out_buf here
     * (both point at the caller's lpstrFile buffer), and run_portal_request
     * has already read/converted it above. */
    if (params->out_buf && params->out_buf_len)
        memset(params->out_buf, 0, params->out_buf_len * sizeof(WCHAR));

    if (uri_count)
    {
        char *path = uris[0];
        WCHAR *dos_path = NULL, *localized;

        if (!strncmp(path, "file://", 7))
        {
            path += 7;
            unescape_uri_path(path);
        }
        /* See the comment in portal_open_file: check the output pointer,
         * not the NTSTATUS - a save target normally doesn't exist yet, and
         * that alone must not be treated as a hard failure here. */
        ntdll_get_dos_file_name(path, &dos_path, 0);
        if (dos_path && (localized = ntdll_localize_external_path(path, dos_path, TRUE)))
        {
            free(dos_path);
            dos_path = localized;
        }
        if (dos_path)
        {
            SIZE_T len = wcslen(dos_path) + 1;

            params->required_len = len;
            if (len <= params->out_buf_len)
            {
                memcpy(params->out_buf, dos_path, len * sizeof(WCHAR));
                params->cancelled = FALSE;
            }
            else params->truncated = TRUE;
            free(dos_path);
        }
    }

    for (i = 0; i < uri_count; i++) free(uris[i]);
    free(uris);
    return STATUS_SUCCESS;
}

#else /* SONAME_LIBDBUS_1 */

NTSTATUS portal_open_file(void *args)
{
    struct portal_open_save_params *params = args;
    params->cancelled = TRUE;
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS portal_save_file(void *args)
{
    struct portal_open_save_params *params = args;
    params->cancelled = TRUE;
    return STATUS_NOT_SUPPORTED;
}

#endif

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    portal_open_file,
    portal_save_file,
};
