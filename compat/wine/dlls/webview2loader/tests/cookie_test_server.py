#!/usr/bin/env python3
# Test fixture for test_delete_all_cookies in webview2loader.c -- NOT part of
# the DLL, NOT built or run by build.sh (this whole test suite is manual;
# --disable-tests skips tests/ repo-wide, see CLAUDE.md). Run this by hand,
# alongside the manual cross-compile/wine-run recipe in
# .superpowers/sdd/2026-08-10-webview2loader-core/task-8-report.md, whenever
# re-verifying test_delete_all_cookies.
#
# Why this exists instead of a DLL-exported "add a test cookie" hook: an
# earlier version of that test added cookies via a
# __wine_test_webview2loader_add_cookie export in webview2loader.dll itself.
# Code review rejected it -- it let any in-process code holding a live
# ICoreWebView2* inject arbitrary cookies into the real cookie store with no
# validation, and this Makefile.in builds one unconditional production DLL
# (no test/production split), so that capability would have shipped for
# real. This script instead lets the test add its cookie through the
# already-legitimate, already-implemented Navigate() path: a real HTTP
# response with a real Set-Cookie header is exactly how any actual WebView2
# client (including the real Roblox login flow) gets a cookie set -- nothing
# new or privileged, just an ordinary page load.
#
# Usage: python3 cookie_test_server.py [port]  (default port 18765, must
# match the port test_delete_all_cookies navigates to).

import http.server
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 18765


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Set-Cookie", "tuxblox_test_cookie=1; Path=/")
        self.end_headers()
        self.wfile.write(b"<html><body>tuxblox cookie test fixture</body></html>")

    def log_message(self, *args):
        pass  # keep the manual test run's output quiet


if __name__ == "__main__":
    http.server.HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
