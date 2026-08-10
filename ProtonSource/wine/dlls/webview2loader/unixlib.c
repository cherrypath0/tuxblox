#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winternl.h>

#include <wine/unixlib.h>

#include "unixlib.h"

/* Task 4 replaces this body with the real dlopen/gtk_init sequence. */
static NTSTATUS unix_init_impl(void *args)
{
    return STATUS_NOT_SUPPORTED;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    unix_init_impl,
};
