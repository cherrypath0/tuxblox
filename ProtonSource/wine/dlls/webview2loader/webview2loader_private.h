#ifndef __WINE_WEBVIEW2LOADER_PRIVATE_H
#define __WINE_WEBVIEW2LOADER_PRIVATE_H

#include <windef.h>
#include <winbase.h>
#include <objbase.h>

/* Fire-and-forget worker thread: runs proc(ctx) on a new thread and
 * immediately closes the handle (we never join; each proc is responsible
 * for freeing ctx and, for the three async-completion call sites, for
 * invoking the caller's completion handler itself before returning).
 * Shared by every "async" WebView2 entry point (CreateCoreWebView2Environment-
 * WithOptions, CreateCoreWebView2Controller, Navigate) since real WebView2
 * returns immediately and reports completion via a callback/event later. */
BOOL start_async_work(LPTHREAD_START_ROUTINE proc, void *ctx);

/* Interface/object definitions land here in later tasks:
 * Task 5: ICoreWebView2Environment
 * Task 6: ICoreWebView2Controller
 * Task 7: ICoreWebView2
 * Task 8: ICoreWebView2_2 / ICoreWebView2CookieManager
 */

#endif /* __WINE_WEBVIEW2LOADER_PRIVATE_H */
