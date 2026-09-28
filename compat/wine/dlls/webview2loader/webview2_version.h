/*
 * The WebView2 runtime version TuxBlox reports.
 *
 * Copyright 2026 TuxBlox Developers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_WEBVIEW2_VERSION_H
#define __WINE_WEBVIEW2_VERSION_H

/* Reported by GetAvailableCoreWebView2BrowserVersionString, recorded in the
 * EdgeUpdate registry keys, and used as the runtime folder's name. A loader
 * reads the version from the registry and builds the folder path from it, so
 * these three cannot be allowed to drift apart. */
#define WEBVIEW2_RUNTIME_VERSION L"109.0.1518.140"

#endif /* __WINE_WEBVIEW2_VERSION_H */
