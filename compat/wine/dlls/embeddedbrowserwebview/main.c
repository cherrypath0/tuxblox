/*
 * The runtime face of TuxBlox's WebView2 bridge.
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "wine/debug.h"

/* Wine's debug channel name is capped at 14 characters; ebwebview is what
 * Microsoft's own loader calls the folder its runtime installs into. */
WINE_DEFAULT_DEBUG_CHANNEL(ebwebview);

HRESULT WINAPI CreateCoreWebView2EnvironmentWithOptions(PCWSTR,PCWSTR,void*,void*);

/***********************************************************************
 *           CreateWebViewEnvironmentWithOptionsInternal
 *
 * What Microsoft's loader calls once it has found the runtime and loaded this
 * DLL out of it. An application that links that loader statically never loads
 * our webview2loader, so this is the only way it can reach the bridge.
 * in_private and crash_reporting are accepted and ignored: the bridge has
 * neither a private mode nor a crash reporter, and neither changes the
 * environment the caller gets back.
 */
HRESULT WINAPI CreateWebViewEnvironmentWithOptionsInternal( BOOL in_private, BOOL crash_reporting,
                                                            PCWSTR user_data_folder, void *options,
                                                            void *handler )
{
    TRACE( "(%d, %d, %s, %p, %p)\n", in_private, crash_reporting,
           debugstr_w(user_data_folder), options, handler );

    /* NULL browserExecutableFolder: there is no browser to point at. */
    return CreateCoreWebView2EnvironmentWithOptions( NULL, user_data_folder, options, handler );
}

/***********************************************************************
 *           CreateSharedWebViewEnvironmentInternal
 *
 * Used by callers that want one browser process shared between environments.
 * The bridge runs one WebKitGTK host per environment, so there is nothing to
 * share and nothing that would make this behave as its name promises.
 */
HRESULT WINAPI CreateSharedWebViewEnvironmentInternal( void **env )
{
    FIXME( "(%p) stub\n", env );
    return E_NOTIMPL;
}

/***********************************************************************
 *           DllCanUnloadNow
 *
 * S_FALSE always: an environment handed out here outlives this call, and a
 * loader that unloaded us would take the bridge with it.
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

/***********************************************************************
 *           GetHandleVerifier
 *
 * Chromium's handle-tracking hook. There is no Chromium here.
 */
void * WINAPI GetHandleVerifier(void)
{
    return NULL;
}
