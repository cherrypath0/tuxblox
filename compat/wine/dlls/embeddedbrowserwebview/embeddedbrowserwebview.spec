# The surface Microsoft's WebView2 loader resolves once it has found the
# runtime. The five telemetry_client::IDataFieldVisitor C++ symbols the real
# DLL also exports are deliberately absent: nothing resolves them across the
# DLL boundary, so reproducing a mangled vtable export would say nothing.
@ stdcall CreateWebViewEnvironmentWithOptionsInternal(long long wstr ptr ptr)
@ stdcall CreateSharedWebViewEnvironmentInternal(ptr)
@ stdcall DllCanUnloadNow()
@ stdcall GetHandleVerifier()
