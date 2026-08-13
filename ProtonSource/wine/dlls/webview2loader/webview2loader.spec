@ stdcall CreateCoreWebView2EnvironmentWithOptions(wstr wstr ptr ptr)
@ stdcall GetAvailableCoreWebView2BrowserVersionString(wstr ptr)
@ stdcall CompareBrowserVersions(wstr wstr ptr)

# Test-support only -- NOT part of the real WebView2Loader.dll API surface.
# Exists so tests/webview2loader.c can verify DeleteAllCookies actually
# reduced the real cookie count rather than only checking its return code;
# see __wine_test_webview2loader_count_cookies's own comment in webview.c
# (which also explains why there is deliberately no matching "add a cookie"
# export here -- that one was reviewed and removed as a real capability
# risk, not just test scaffolding).
@ stdcall __wine_test_webview2loader_count_cookies(ptr)
@ stdcall __wine_test_webview2loader_window_is_visible(ptr)
@ stdcall __wine_test_webview2loader_sync_window_geometry(ptr ptr long)
@ stdcall __wine_test_webview2loader_get_window_geometry(ptr ptr)
