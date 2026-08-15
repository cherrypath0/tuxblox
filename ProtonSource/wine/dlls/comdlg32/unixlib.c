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
    const char *handle_token = "tuxblox1";
    const char *request_path;
    struct pending_response pending = { 0 };
    char match_rule[512];

    *out_uris = NULL;
    *out_count = 0;
    if (!load_dbus_functions()) return FALSE;

    p_dbus_error_init(&error);
    conn = p_dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (!conn) { p_dbus_error_free(&error); return FALSE; }

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
    /* This wait has no timeout (dbus_connection_read_write_dispatch's second
     * arg is -1): if the portal backend never surfaces a dialog or never
     * fires a Response signal for some reason, this blocks indefinitely.
     * Bracketed with tuxblox trace markers (item 6 investigation) so a
     * WINEDEBUG=+tuxblox capture can show whether a hang is stuck in here
     * specifically -- a BEGIN with no matching END means yes. */
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

    p_dbus_connection_remove_filter(conn, response_filter, &pending);
    p_dbus_message_unref(reply);
    p_dbus_connection_unref(conn);

    if (!pending.done)
    {
        UINT i;
        for (i = 0; i < pending.uri_count; i++) free(pending.uris[i]);
        free(pending.uris);
        return FALSE;
    }

    if (pending.response_code == 0 && pending.uri_count)
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
    return TRUE;
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
        ntdll_get_unix_file_name(params->initial_dir, &initial_dir_unix, FILE_OPEN_IF);
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

/* ntdll_get_dos_file_name() falls back to this "\\?\unix\<path>" device-path
 * form for any file outside every drive this prefix maps - exactly what
 * happens once the portal above lets the user pick a file from outside
 * WINEPREFIX via the native (non-Wine) file chooser. Apps like Roblox don't
 * understand that form and just report the file can't be opened, so
 * localize_external_path() below papers over it. */
static BOOL is_unix_fallback_path(const WCHAR *dos_path)
{
    /* Wide string literals are 32-bit wchar_t on this (unix-side) build, not
     * the 16-bit WCHAR the rest of Wine uses, so this has to be spelled out
     * as a char array like unix_prefixW in ntdll/unix/file.c is. */
    static const WCHAR prefix[] = {'\\','\\','?','\\','u','n','i','x','\\'};
    return dos_path && !wcsncmp(dos_path, prefix, ARRAY_SIZE(prefix));
}

/* Resolves "C:\users\user\files" (creating the "files" leaf if needed) to
 * its unix path. The username is hardcoded rather than derived from ntdll
 * (which would give the real *host* username, e.g. "cherry") because
 * GetUserNameA/W (dlls/advapi32/advapi.c) always return the literal string
 * "user" regardless of the host, and wineboot names the actual on-disk
 * profile directory to match that, not the host account - using the real
 * host username here would build a path that doesn't exist and can't be
 * resolved, silently disabling this whole mechanism. Returns NULL if the
 * directory can't be created. */
static char *get_external_files_dir_unix(void)
{
    /* See the comment on is_unix_fallback_path's prefix: no L"" here either. */
    static const WCHAR pathW[] =
        {'C',':','\\','u','s','e','r','s','\\','u','s','e','r','\\','f','i','l','e','s',0};
    char *unix_dir = NULL;

    /* FILE_OPEN_IF: only the leaf "files" component may not exist yet. */
    ntdll_get_unix_file_name(pathW, &unix_dir, FILE_OPEN_IF);

    if (unix_dir && mkdir(unix_dir, 0755) && errno != EEXIST)
    {
        free(unix_dir);
        unix_dir = NULL;
    }
    return unix_dir;
}

/* Picks "<base> (n)<ext>" inside dir_unix the way Windows/most file managers
 * do, skipping an existing entry unless it's already a symlink to
 * source_path - in which case that existing entry is reused rather than a
 * duplicate being created (so re-picking the same outside file repeatedly,
 * e.g. re-uploading the same image, doesn't pile up "name (1)", "name (2)",
 * ... forever). *out_reuse is set to TRUE when the returned path is such an
 * existing symlink already pointing at source_path. Returns NULL if no free
 * or matching name could be found. */
static char *pick_symlink_path(const char *dir_unix, const char *source_path, BOOL *out_reuse)
{
    const char *slash = strrchr(source_path, '/');
    const char *leaf = slash ? slash + 1 : source_path;
    const char *dot = strrchr(leaf, '.');
    SIZE_T base_len = (dot && dot != leaf) ? (SIZE_T)(dot - leaf) : strlen(leaf);
    const char *ext = (dot && dot != leaf) ? dot : "";
    UINT n;

    *out_reuse = FALSE;

    for (n = 0; n < 1000; n++)
    {
        char suffix[32] = "";
        char *candidate;
        struct stat st;
        char link_target[PATH_MAX];
        ssize_t link_len;

        if (n) snprintf(suffix, sizeof(suffix), " (%u)", n);
        if (!(candidate = malloc(strlen(dir_unix) + 1 + base_len + strlen(suffix) + strlen(ext) + 1)))
            return NULL;
        sprintf(candidate, "%s/%.*s%s%s", dir_unix, (int)base_len, leaf, suffix, ext);

        if (lstat(candidate, &st))
        {
            if (errno == ENOENT) return candidate; /* free slot */
            free(candidate);
            return NULL;
        }

        if ((link_len = readlink(candidate, link_target, sizeof(link_target) - 1)) > 0)
        {
            link_target[link_len] = 0;
            if (!strcmp(link_target, source_path))
            {
                *out_reuse = TRUE;
                return candidate; /* existing symlink to the same source */
            }
        }
        free(candidate);
    }
    return NULL;
}

/* If dos_path is the unix-fallback form above, symlinks source_unix_path
 * into this prefix's "C:\users\<user>\files" and returns the resulting,
 * properly-resolved dos path (caller frees); returns NULL - leaving
 * dos_path as the caller's problem, same as before this existed - if
 * dos_path already resolved to a normal in-prefix path, or if remapping
 * failed for any reason (unknown username, directory not creatable, all
 * 1000 name slots taken, symlink() itself failing). */
static WCHAR *localize_external_path(const char *source_unix_path, WCHAR *dos_path)
{
    char *dir_unix, *symlink_path;
    WCHAR *new_dos_path = NULL;
    BOOL reuse;

    if (!is_unix_fallback_path(dos_path)) return NULL;
    if (!(dir_unix = get_external_files_dir_unix())) return NULL;

    if ((symlink_path = pick_symlink_path(dir_unix, source_unix_path, &reuse)))
    {
        if (reuse || !symlink(source_unix_path, symlink_path))
            ntdll_get_dos_file_name(symlink_path, &new_dos_path, 0);
        free(symlink_path);
    }
    free(dir_unix);
    return new_dos_path;
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
            const char *path = uris[i];
            WCHAR *dos_path = NULL, *localized;

            if (!strncmp(path, "file://", 7)) path += 7;
            /* ntdll_get_dos_file_name returns STATUS_NO_SUCH_FILE (nonzero)
             * but still fills in a usable buffer when the leaf component
             * doesn't exist yet with disposition FILE_SUPERSEDE (0) - check
             * the output pointer, not the NTSTATUS, so a not-yet-existing
             * target (the common case for a save, and possible here too)
             * isn't mistaken for a hard failure. */
            ntdll_get_dos_file_name(path, &dos_path, 0);
            if (dos_path && (localized = localize_external_path(path, dos_path)))
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
        const char *path = uris[0];
        WCHAR *dos_path = NULL, *localized;

        if (!strncmp(path, "file://", 7)) path += 7;
        /* See the comment in portal_open_file: check the output pointer,
         * not the NTSTATUS - a save target normally doesn't exist yet, and
         * that alone must not be treated as a hard failure here. */
        ntdll_get_dos_file_name(path, &dos_path, 0);
        if (dos_path && (localized = localize_external_path(path, dos_path)))
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
